#include "pch.h"
#include "VideoFrameRenderNodeOGL.h"

Q_LOGGING_CATEGORY(CategoryVideoPlayback, "qddm.video")

namespace
{

static constexpr int kVertexCount = 4 * 2; //quad, vec2
static constexpr int kVertexSize = kVertexCount * sizeof(GLfloat);

constexpr inline qreal ScreenRefreshRate(qreal refresh_rate)
{
    if (refresh_rate >= 59 && refresh_rate <= 60)
        refresh_rate = 60; //Manual patch
    return refresh_rate;
}

constexpr int DivideTwoRoundUp(int a)
{
    return (a + 1) / 2;
}

struct ColorMatrix
{
    constexpr ColorMatrix(const double (&eff)[5], const double (&range_eff)[3][2])
        :color_matrix()
    {
        double matrix[4][4] = {
            { 1, 0,                         eff[4],                    0 },
            { 1, -eff[2] * eff[3] / eff[1], -eff[0] * eff[4] / eff[1], 0 },
            { 1, eff[3],                    0,                         0 },
            { 0, 0,                         0,                         1 },
        };
        for (int i = 0; i < 3; ++i)
        {
            matrix[i][3] = range_eff[0][1] * matrix[i][0] + range_eff[1][1] * matrix[i][1] + range_eff[2][1] * matrix[i][2];
            matrix[i][0] *= range_eff[0][0];
            matrix[i][1] *= range_eff[1][0];
            matrix[i][2] *= range_eff[2][0];
        }
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                color_matrix[i * 4 + j] = matrix[i][j];
    }

    float color_matrix[16];
};

static constexpr double kBT601Eff[5] = { 0.299, 0.587, 0.114, 1.772, 1.402 };
static constexpr double kBT709Eff[5] = { 0.2126, 0.7152, 0.0722, 1.8556, 1.5748 };
static constexpr double kBT2020Eff[5] = { 0.2627, 0.6780, 0.0593, 1.8814, 1.4747 };

static constexpr double kMpegRangeEff[3][2] = { { 255.0 / 219.0, -16.0 / 219.0 }, { 255.0 / 224.0, -128.0 / 224.0 }, { 255.0 / 224.0, -128.0 / 224.0 } };
static constexpr double kJpegRangeEff[3][2] = { { 1, 0 }, { 1, -128.0 / 255.0 }, { 1, -128.0 / 255.0 } };

static constexpr ColorMatrix kColorMatrixBT601M(kBT601Eff, kMpegRangeEff);
static constexpr ColorMatrix kColorMatrixBT601J(kBT601Eff, kJpegRangeEff);
static constexpr ColorMatrix kColorMatrixBT709M(kBT709Eff, kMpegRangeEff);
static constexpr ColorMatrix kColorMatrixBT709J(kBT709Eff, kJpegRangeEff);
static constexpr ColorMatrix kColorMatrixBT2020M(kBT2020Eff, kMpegRangeEff);
static constexpr ColorMatrix kColorMatrixBT2020J(kBT2020Eff, kJpegRangeEff);

void InitSinglePixelUnpackBuffer(std::unique_ptr<QOpenGLBuffer> &buffer, int buffer_size)
{
    buffer = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::PixelUnpackBuffer);
    buffer->create();
    buffer->bind();
    buffer->allocate(buffer_size);
    buffer->release();
}

void InitSingleTexture(std::unique_ptr<QOpenGLTexture> &texture, int width, int height, QOpenGLTexture::TextureFormat texture_format, QOpenGLTexture::PixelFormat pixel_format, QOpenGLTexture::PixelType pixel_type)
{
    texture = std::make_unique<QOpenGLTexture>(QOpenGLTexture::Target2D);
    texture->setSize(width, height);
    texture->setFormat(texture_format);
    texture->allocateStorage(pixel_format, pixel_type);
    texture->setMinificationFilter(QOpenGLTexture::Linear);
    texture->setMagnificationFilter(QOpenGLTexture::Linear);
    texture->setWrapMode(QOpenGLTexture::ClampToEdge);
}

void UpdateSinglePixelUnpackBuffer(QOpenGLBuffer *buffer, const void *data, int line_size, int texture_line_size, int height)
{
    buffer->bind();
    if (line_size == texture_line_size)
    {
        void *buffer_mapped = buffer->map(QOpenGLBuffer::WriteOnly);
        if (buffer_mapped)
        {
            memcpy(buffer_mapped, data, line_size * height);
            buffer->unmap();
        }
    }
    else
    {
        void *buffer_mapped = buffer->map(QOpenGLBuffer::WriteOnly);
        if (buffer_mapped)
        {
            for (int i = 0; i < height; ++i)
                memcpy(static_cast<char *>(buffer_mapped) + i * texture_line_size, static_cast<const char *>(data) + i * line_size, texture_line_size);
            buffer->unmap();
        }
    }
    buffer->release();
}

void UpdateSingleTexture(QOpenGLTexture *texture, QOpenGLTexture::PixelFormat pixel_format, QOpenGLTexture::PixelType pixel_type, QOpenGLBuffer *buffer)
{
    buffer->bind();
    texture->setData(pixel_format, pixel_type, reinterpret_cast<const void *>(0));
    buffer->release();
}

}

bool VideoFrameRenderNodeOGL::PixelUnpackBufferItem::IsCompatible(AVFrame *frame)
{
    if (frame_size != QSize(frame->width, frame->height))
        return false;
    if (pixel_format != frame->format)
        return false;
    if (color_range != frame->color_range)
        return false;
    if (colorspace != frame->colorspace)
        return false;
    return true;
}

VideoFrameRenderNodeOGL::VideoFrameRenderNodeOGL()
{
    texture_buffers_empty_.resize(kQueueSize);
}

VideoFrameRenderNodeOGL::~VideoFrameRenderNodeOGL()
{
    VideoFrameRenderNodeOGL::releaseResources();
}

void VideoFrameRenderNodeOGL::releaseResources()
{
    shader_ = nullptr;
    frame_size_ = QSize();
    pixel_format_ = AV_PIX_FMT_NONE;
    color_range_ = AVCOL_RANGE_UNSPECIFIED;
    colorspace_ = AVCOL_SPC_UNSPECIFIED;
    vertex_buffer_ = nullptr;
    for (int i = 0; i < kTextureItemCount; ++i)
        textures_[i] = nullptr;
    texture_buffers_uploaded_.clear();
    texture_buffers_empty_.clear();
    texture_buffers_empty_.resize(kQueueSize);
    texture_buffers_used_.clear();
}

void VideoFrameRenderNodeOGL::InitShader()
{
    shader_ = std::make_unique<QOpenGLShaderProgram>();

    switch (pixel_format_)
    {
    case AV_PIX_FMT_RGB0:
        shader_->addCacheableShaderFromSourceFile(QOpenGLShader::Vertex, ":/shaders/default.vert");
        shader_->addCacheableShaderFromSourceFile(QOpenGLShader::Fragment, ":/shaders/rgbx.frag");
        break;
    case AV_PIX_FMT_NV12:
    case AV_PIX_FMT_NV21:
        shader_->addCacheableShaderFromSourceFile(QOpenGLShader::Vertex, ":/shaders/default.vert");
        shader_->addCacheableShaderFromSourceFile(QOpenGLShader::Fragment, ":/shaders/yuvbiplanar.frag");
        break;
    case AV_PIX_FMT_YUV420P:
    case AV_PIX_FMT_YUV444P:
    case AV_PIX_FMT_YUVJ444P:
        shader_->addCacheableShaderFromSourceFile(QOpenGLShader::Vertex, ":/shaders/default.vert");
        shader_->addCacheableShaderFromSourceFile(QOpenGLShader::Fragment, ":/shaders/yuvtriplanar.frag");
        break;
    default:
        qCWarning(CategoryVideoPlayback) << "Unsupported pixel format";
        shader_ = nullptr;
        return;
    }

    shader_->bindAttributeLocation("position", 0);
    shader_->bindAttributeLocation("texCoordIn", 1);
    shader_->link();

    matrix_uniform_index_ = shader_->uniformLocation("matrix");
    opacity_uniform_index_ = shader_->uniformLocation("opacity");
    color_matrix_uniform_index_ = shader_->uniformLocation("colorMatrix");
    texture_0_uniform_index_ = shader_->uniformLocation("texture0");
    texture_1_uniform_index_ = shader_->uniformLocation("texture1");
    texture_2_uniform_index_ = shader_->uniformLocation("texture2");
}

void VideoFrameRenderNodeOGL::InitTexture()
{
    switch (pixel_format_)
    {
    case AV_PIX_FMT_RGB0:
        InitSingleTexture(textures_[0], frame_size_.width(), frame_size_.height(), QOpenGLTexture::RGBA8_UNorm, QOpenGLTexture::RGBA, QOpenGLTexture::UInt8);
        textures_[1] = nullptr; Q_ASSERT(texture_1_uniform_index_ == -1);
        textures_[2] = nullptr; Q_ASSERT(texture_2_uniform_index_ == -1);
        break;
    case AV_PIX_FMT_NV12:
    case AV_PIX_FMT_NV21:
        InitSingleTexture(textures_[0], frame_size_.width(), frame_size_.height(), QOpenGLTexture::R8_UNorm, QOpenGLTexture::Red, QOpenGLTexture::UInt8);
        InitSingleTexture(textures_[1], DivideTwoRoundUp(frame_size_.width()), DivideTwoRoundUp(frame_size_.height()), QOpenGLTexture::RG8_UNorm, QOpenGLTexture::RG, QOpenGLTexture::UInt8);
        textures_[2] = nullptr; Q_ASSERT(texture_2_uniform_index_ == -1);
        break;
    case AV_PIX_FMT_YUV420P:
        InitSingleTexture(textures_[0], frame_size_.width(), frame_size_.height(), QOpenGLTexture::R8_UNorm, QOpenGLTexture::Red, QOpenGLTexture::UInt8);
        InitSingleTexture(textures_[1], DivideTwoRoundUp(frame_size_.width()), DivideTwoRoundUp(frame_size_.height()), QOpenGLTexture::R8_UNorm, QOpenGLTexture::Red, QOpenGLTexture::UInt8);
        InitSingleTexture(textures_[2], DivideTwoRoundUp(frame_size_.width()), DivideTwoRoundUp(frame_size_.height()), QOpenGLTexture::R8_UNorm, QOpenGLTexture::Red, QOpenGLTexture::UInt8);
        break;
    case AV_PIX_FMT_YUV444P:
    case AV_PIX_FMT_YUVJ444P:
        InitSingleTexture(textures_[0], frame_size_.width(), frame_size_.height(), QOpenGLTexture::R8_UNorm, QOpenGLTexture::Red, QOpenGLTexture::UInt8);
        InitSingleTexture(textures_[1], frame_size_.width(), frame_size_.height(), QOpenGLTexture::R8_UNorm, QOpenGLTexture::Red, QOpenGLTexture::UInt8);
        InitSingleTexture(textures_[2], frame_size_.width(), frame_size_.height(), QOpenGLTexture::R8_UNorm, QOpenGLTexture::Red, QOpenGLTexture::UInt8);
        break;
    default:
        qCWarning(CategoryVideoPlayback) << "Unsupported pixel format";
    }
}

void VideoFrameRenderNodeOGL::UpdateTexture(PixelUnpackBufferItem &item)
{
    switch (pixel_format_)
    {
    case AV_PIX_FMT_RGB0:
        UpdateSingleTexture(textures_[0].get(), QOpenGLTexture::RGBA, QOpenGLTexture::UInt8, item.buffers[0].get());
        break;
    case AV_PIX_FMT_NV12:
    case AV_PIX_FMT_NV21:
        UpdateSingleTexture(textures_[0].get(), QOpenGLTexture::Red, QOpenGLTexture::UInt8, item.buffers[0].get());
        UpdateSingleTexture(textures_[1].get(), QOpenGLTexture::RG, QOpenGLTexture::UInt8, item.buffers[1].get());
        break;
    case AV_PIX_FMT_YUV420P:
        UpdateSingleTexture(textures_[0].get(), QOpenGLTexture::Red, QOpenGLTexture::UInt8, item.buffers[0].get());
        UpdateSingleTexture(textures_[1].get(), QOpenGLTexture::Red, QOpenGLTexture::UInt8, item.buffers[1].get());
        UpdateSingleTexture(textures_[2].get(), QOpenGLTexture::Red, QOpenGLTexture::UInt8, item.buffers[2].get());
        break;
    case AV_PIX_FMT_YUV444P:
    case AV_PIX_FMT_YUVJ444P:
        UpdateSingleTexture(textures_[0].get(), QOpenGLTexture::Red, QOpenGLTexture::UInt8, item.buffers[0].get());
        UpdateSingleTexture(textures_[1].get(), QOpenGLTexture::Red, QOpenGLTexture::UInt8, item.buffers[1].get());
        UpdateSingleTexture(textures_[2].get(), QOpenGLTexture::Red, QOpenGLTexture::UInt8, item.buffers[2].get());
        break;
    default:
        qCWarning(CategoryVideoPlayback) << "Unsupported pixel format";
    }
}

void VideoFrameRenderNodeOGL::InitPixelUnpackBuffer(PixelUnpackBufferItem &item)
{
    switch (item.pixel_format)
    {
    case AV_PIX_FMT_RGB0:
        InitSinglePixelUnpackBuffer(item.buffers[0], item.frame_size.width() * item.frame_size.height() * 4 * sizeof(GLubyte));
        break;
    case AV_PIX_FMT_NV12:
    case AV_PIX_FMT_NV21:
        InitSinglePixelUnpackBuffer(item.buffers[0], item.frame_size.width() * item.frame_size.height() * 1 * sizeof(GLubyte));
        InitSinglePixelUnpackBuffer(item.buffers[1], DivideTwoRoundUp(item.frame_size.width()) * DivideTwoRoundUp(item.frame_size.height()) * 2 * sizeof(GLubyte));
        break;
    case AV_PIX_FMT_YUV420P:
        InitSinglePixelUnpackBuffer(item.buffers[0], item.frame_size.width() * item.frame_size.height() * 1 * sizeof(GLubyte));
        InitSinglePixelUnpackBuffer(item.buffers[1], DivideTwoRoundUp(item.frame_size.width()) * DivideTwoRoundUp(item.frame_size.height()) * 1 * sizeof(GLubyte));
        InitSinglePixelUnpackBuffer(item.buffers[2], DivideTwoRoundUp(item.frame_size.width()) * DivideTwoRoundUp(item.frame_size.height()) * 1 * sizeof(GLubyte));
        break;
    case AV_PIX_FMT_YUV444P:
    case AV_PIX_FMT_YUVJ444P:
        InitSinglePixelUnpackBuffer(item.buffers[0], item.frame_size.width() * item.frame_size.height() * 1 * sizeof(GLubyte));
        InitSinglePixelUnpackBuffer(item.buffers[1], item.frame_size.width() * item.frame_size.height() * 1 * sizeof(GLubyte));
        InitSinglePixelUnpackBuffer(item.buffers[2], item.frame_size.width() * item.frame_size.height() * 1 * sizeof(GLubyte));
        break;
    default:
        qCWarning(CategoryVideoPlayback) << "Unsupported pixel format";
    }
}

void VideoFrameRenderNodeOGL::UpdatePixelUnpackBuffer(PixelUnpackBufferItem &item, AVFrame *frame)
{
    switch (item.pixel_format)
    {
    case AV_PIX_FMT_RGB0:
        UpdateSinglePixelUnpackBuffer(item.buffers[0].get(), frame->data[0], frame->linesize[0], item.frame_size.width() * 4 * sizeof(GLubyte), item.frame_size.height());
        break;
    case AV_PIX_FMT_NV12:
    case AV_PIX_FMT_NV21:
        UpdateSinglePixelUnpackBuffer(item.buffers[0].get(), frame->data[0], frame->linesize[0], item.frame_size.width() * 1 * sizeof(GLubyte), item.frame_size.height());
        UpdateSinglePixelUnpackBuffer(item.buffers[1].get(), frame->data[1], frame->linesize[1], DivideTwoRoundUp(item.frame_size.width()) * 2 * sizeof(GLubyte), DivideTwoRoundUp(item.frame_size.height()));
        break;
    case AV_PIX_FMT_YUV420P:
        UpdateSinglePixelUnpackBuffer(item.buffers[0].get(), frame->data[0], frame->linesize[0], item.frame_size.width() * 1 * sizeof(GLubyte), item.frame_size.height());
        UpdateSinglePixelUnpackBuffer(item.buffers[1].get(), frame->data[1], frame->linesize[1], DivideTwoRoundUp(item.frame_size.width()) * 1 * sizeof(GLubyte), DivideTwoRoundUp(item.frame_size.height()));
        UpdateSinglePixelUnpackBuffer(item.buffers[2].get(), frame->data[2], frame->linesize[2], DivideTwoRoundUp(item.frame_size.width()) * 1 * sizeof(GLubyte), DivideTwoRoundUp(item.frame_size.height()));
        break;
    case AV_PIX_FMT_YUV444P:
    case AV_PIX_FMT_YUVJ444P:
        UpdateSinglePixelUnpackBuffer(item.buffers[0].get(), frame->data[0], frame->linesize[0], item.frame_size.width() * 1 * sizeof(GLubyte), item.frame_size.height());
        UpdateSinglePixelUnpackBuffer(item.buffers[1].get(), frame->data[1], frame->linesize[1], item.frame_size.width() * 1 * sizeof(GLubyte), item.frame_size.height());
        UpdateSinglePixelUnpackBuffer(item.buffers[2].get(), frame->data[2], frame->linesize[2], item.frame_size.width() * 1 * sizeof(GLubyte), item.frame_size.height());
        break;
    default:
        qCWarning(CategoryVideoPlayback) << "Unsupported pixel format";
    }
}

void VideoFrameRenderNodeOGL::InitColorMatrix()
{
    switch (colorspace_)
    {
    case AVCOL_SPC_RGB:
        color_matrix_.setToIdentity();
        break;
    case AVCOL_SPC_BT2020_CL:
    case AVCOL_SPC_BT2020_NCL:
        color_matrix_ = QMatrix4x4(color_range_ == AVCOL_RANGE_JPEG ? kColorMatrixBT2020J.color_matrix : kColorMatrixBT2020M.color_matrix);
        break;
    case AVCOL_SPC_BT470BG:
    case AVCOL_SPC_SMPTE170M:
    case AVCOL_SPC_SMPTE240M:
        color_matrix_ = QMatrix4x4(color_range_ == AVCOL_RANGE_JPEG ? kColorMatrixBT601J.color_matrix : kColorMatrixBT601M.color_matrix);
        break;
    case AVCOL_SPC_BT709:
    default:
        color_matrix_ = QMatrix4x4(color_range_ == AVCOL_RANGE_JPEG ? kColorMatrixBT709J.color_matrix : kColorMatrixBT709M.color_matrix);
        break;
    }
    if (pixel_format_ == AV_PIX_FMT_NV21)
    {
        color_matrix_ = color_matrix_ * QMatrix4x4(
                    1, 0, 0, 0,
                    0, 0, 1, 0,
                    0, 1, 0, 0,
                    0, 0, 0, 1);
    }
}

void VideoFrameRenderNodeOGL::InitVertexBuffer()
{
    static constexpr GLfloat kTexCoordIn[] = {
        0.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 1.0f,
        1.0f, 0.0f,
    };
    vertex_buffer_ = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
    vertex_buffer_->create();
    vertex_buffer_->bind();
    vertex_buffer_->allocate(kVertexSize + sizeof(kTexCoordIn));
    vertex_buffer_->write(kVertexSize, kTexCoordIn, sizeof(kTexCoordIn));
    vertex_buffer_->release();
}

void VideoFrameRenderNodeOGL::Upload(PlaybackClock::time_point current_time)
{
    if (texture_buffers_used_.size() > kUsedQueueSize)
    {
        auto itr = texture_buffers_used_.begin(), itr_end = itr + texture_buffers_used_.size() - kUsedQueueSize;
        texture_buffers_empty_.insert(texture_buffers_empty_.end(), std::make_move_iterator(itr), std::make_move_iterator(itr_end));
        texture_buffers_used_.erase(itr, itr_end);
    }

    RemoveImpossibleFrame(current_time);
    while (!video_frames_.empty() && !texture_buffers_empty_.empty())
    {
        auto &frame = video_frames_.front();
        auto &buffer = texture_buffers_empty_.front();
        Q_ASSERT(frame->present_time > current_time);
        if (!buffer.IsCompatible(frame->frame.Get()))
        {
            for (int i = 0; i < kTextureItemCount; ++i)
                buffer.buffers[i].reset();
            buffer.frame_size = QSize(frame->frame->width, frame->frame->height);
            buffer.pixel_format = static_cast<AVPixelFormat>(frame->frame->format);
            buffer.color_range = frame->frame->color_range;
            buffer.colorspace = frame->frame->colorspace;
            InitPixelUnpackBuffer(buffer);
        }
        buffer.present_time = frame->present_time;
        UpdatePixelUnpackBuffer(buffer, frame->frame.Get());

        texture_buffers_uploaded_.push_back(std::move(buffer));
        texture_buffers_empty_.erase(texture_buffers_empty_.begin());
        video_frames_.erase(video_frames_.begin());
    }
}

void VideoFrameRenderNodeOGL::render(const RenderState *state)
{
    auto playback_time = playback_time_base_ + playback_time_interval_ * playback_time_tick_;
    auto current_time = PlaybackClock::now();
    if (std::chrono::abs(playback_time - current_time) > playback_time_interval_)
    {
        ResynchronizeTimer(current_time);
        playback_time = current_time;
        playback_time_tick_ = 1; //Progress tick
    }
    else
    {
        playback_time_tick_ += 1;
    }

#ifdef _DEBUG
    PlaybackClock::time_point render_begin = PlaybackClock::now();
#endif

    auto present_time_limit = playback_time;
    if (!texture_buffers_uploaded_.empty() && texture_buffers_uploaded_.front().present_time <= present_time_limit)
    {
        decltype(texture_buffers_uploaded_)::iterator texture_buffer_itr = texture_buffers_uploaded_.begin(), texture_buffer_itr_end = texture_buffers_uploaded_.end(), selected_texture_buffer_itr;
        for (; texture_buffer_itr != texture_buffer_itr_end && texture_buffer_itr->present_time <= present_time_limit; ++texture_buffer_itr)
            selected_texture_buffer_itr = texture_buffer_itr;

        if (selected_texture_buffer_itr->pixel_format != pixel_format_)
        {
            pixel_format_ = static_cast<AVPixelFormat>(selected_texture_buffer_itr->pixel_format);
            frame_size_ = selected_texture_buffer_itr->frame_size;
            InitShader();
            InitTexture();
            vertex_buffer_need_update_ = true;
            if (color_matrix_uniform_index_ != -1)
            {
                color_range_ = selected_texture_buffer_itr->color_range;
                colorspace_ = selected_texture_buffer_itr->colorspace;
                InitColorMatrix();
            }
        }
        else
        {
            if (frame_size_ != selected_texture_buffer_itr->frame_size)
            {
                frame_size_ = selected_texture_buffer_itr->frame_size;
                InitTexture();
                vertex_buffer_need_update_ = true;
            }
            if (color_matrix_uniform_index_ != -1 && (color_range_ != selected_texture_buffer_itr->color_range || colorspace_ != selected_texture_buffer_itr->colorspace))
            {
                color_range_ = selected_texture_buffer_itr->color_range;
                colorspace_ = selected_texture_buffer_itr->colorspace;
                InitColorMatrix();
            }
        }

        UpdateTexture(*selected_texture_buffer_itr);

#ifdef _DEBUG
        texture_updates_per_second_ += 1;
        if (current_time - last_texture_change_time_ > max_texture_diff_time_)
            max_texture_diff_time_ = current_time - last_texture_change_time_;
        if (current_time - last_texture_change_time_ < min_texture_diff_time_)
            min_texture_diff_time_ = current_time - last_texture_change_time_;
        if (current_time - selected_texture_buffer_itr->present_time > max_latency_)
            max_latency_ = current_time - selected_texture_buffer_itr->present_time;
        if (current_time - selected_texture_buffer_itr->present_time < min_latency_)
            min_latency_ = current_time - selected_texture_buffer_itr->present_time;
        last_texture_change_time_ = current_time;
#endif

        texture_buffers_used_.insert(texture_buffers_used_.end(), std::make_move_iterator(texture_buffers_uploaded_.begin()), std::make_move_iterator(texture_buffer_itr));
        texture_buffers_uploaded_.erase(texture_buffers_uploaded_.begin(), texture_buffer_itr);
    }

    Upload(playback_time);

    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();

    if (shader_)
    {
        if (!vertex_buffer_)
        {
            InitVertexBuffer();
            vertex_buffer_need_update_ = true;
        }

        shader_->bind();
        shader_->setUniformValue(matrix_uniform_index_, *state->projectionMatrix() * *matrix());
        shader_->setUniformValue(opacity_uniform_index_, (float)inheritedOpacity());
        if (color_matrix_uniform_index_ != -1)
            shader_->setUniformValue(color_matrix_uniform_index_, color_matrix_);

        if (textures_[0])
        {
            f->glActiveTexture(GL_TEXTURE0);
            textures_[0]->bind();
            shader_->setUniformValue(texture_0_uniform_index_, 0);
            if (textures_[1])
            {
                f->glActiveTexture(GL_TEXTURE1);
                textures_[1]->bind();
                shader_->setUniformValue(texture_1_uniform_index_, 1);
                if (textures_[2])
                {
                    f->glActiveTexture(GL_TEXTURE2);
                    textures_[2]->bind();
                    shader_->setUniformValue(texture_2_uniform_index_, 2);
                }
                f->glActiveTexture(GL_TEXTURE0);
            }
        }

        vertex_buffer_->bind();

        if (vertex_buffer_need_update_)
        {
            float x1, y1, x2, y2;
            int r1 = frame_size_.width() * height_, r2 = width_ * frame_size_.height();
            if (r1 == r2)
            {
                x1 = 0;
                x2 = width_ - 1;
                y1 = 0;
                y2 = height_ - 1;
            }
            else if (r1 > r2)
            {
                x1 = 0;
                x2 = width_ - 1;
                float render_height = (float)(frame_size_.height() * width_) / frame_size_.width();
                y1 = (height_ - render_height) / 2;
                y2 = y1 + render_height - 1;
            }
            else
            {
                y1 = 0;
                y2 = height_ - 1;
                float render_width = (float)(frame_size_.width() * height_) / frame_size_.height();
                x1 = (width_ - render_width) / 2;
                x2 = x1 + render_width - 1;
            }

            GLfloat vertices[] = {
                GLfloat(x1), GLfloat(y1),
                GLfloat(x1), GLfloat(y2),
                GLfloat(x2), GLfloat(y2),
                GLfloat(x2), GLfloat(y1),
            };
            static_assert(sizeof(vertices) == kVertexSize);
            vertex_buffer_->write(0, vertices, sizeof(vertices));

            vertex_buffer_need_update_ = false;
        }

        shader_->setAttributeBuffer(0, GL_FLOAT, 0, 2);
        shader_->setAttributeBuffer(1, GL_FLOAT, kVertexSize, 2);
        shader_->enableAttributeArray(0);
        shader_->enableAttributeArray(1);

        f->glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

        f->glEnable(GL_BLEND);
        f->glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

        if (state->scissorEnabled())
        {
            f->glEnable(GL_SCISSOR_TEST);
            const QRect r = state->scissorRect();
            f->glScissor(r.x(), r.y(), r.width(), r.height());
        }
        if (state->stencilEnabled())
        {
            f->glEnable(GL_STENCIL_TEST);
            f->glStencilFunc(GL_EQUAL, state->stencilValue(), 0xFF);
            f->glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
        }

        f->glDrawArrays(GL_QUADS, 0, 4);

#ifdef _DEBUG
        PlaybackClock::time_point render_end = PlaybackClock::now();
        auto render_time = render_end - render_begin;
        if (render_time > min_timing_diff_)
            min_timing_diff_ = render_time;
#endif
    }

#ifdef _DEBUG
    renders_per_second_ += 1;
    if (current_time - last_frame_time_ > max_diff_time_)
        max_diff_time_ = current_time - last_frame_time_;
    if (current_time - last_frame_time_ < min_diff_time_)
        min_diff_time_ = current_time - last_frame_time_;
    last_frame_time_ = current_time;
    if (current_time - last_second_ > std::chrono::seconds(1))
    {
        if (current_time - last_second_ > std::chrono::seconds(3))
            last_second_ = current_time;
        else
            last_second_ += std::chrono::seconds(1);
        qCDebug(CategoryVideoPlayback) << texture_buffers_uploaded_.size() << " textures uploaded";
        qCDebug(CategoryVideoPlayback) << texture_buffers_used_.size() << " textures used";
        qCDebug(CategoryVideoPlayback) << texture_buffers_empty_.size() << " textures empty";
        qCDebug(CategoryVideoPlayback) << video_frames_.size() << " frames in pending queue";
        qCDebug(CategoryVideoPlayback) << frames_per_second_ << " fps from source";
        qCDebug(CategoryVideoPlayback) << renders_per_second_ << " fps render";
        qCDebug(CategoryVideoPlayback) << texture_updates_per_second_ << " texture updates";
        qCDebug(CategoryVideoPlayback) << std::chrono::duration_cast<std::chrono::microseconds>(max_diff_time_).count() << "us max diff (frame to frame)";
        qCDebug(CategoryVideoPlayback) << std::chrono::duration_cast<std::chrono::microseconds>(min_diff_time_).count() << "us min diff (frame to frame)";
        qCDebug(CategoryVideoPlayback) << std::chrono::duration_cast<std::chrono::microseconds>(max_texture_diff_time_).count() << "us max diff (texture to texture)";
        qCDebug(CategoryVideoPlayback) << std::chrono::duration_cast<std::chrono::microseconds>(min_texture_diff_time_).count() << "us min diff (texture to texture)";
        qCDebug(CategoryVideoPlayback) << std::chrono::duration_cast<std::chrono::microseconds>(max_latency_).count() << "us max latency";
        qCDebug(CategoryVideoPlayback) << std::chrono::duration_cast<std::chrono::microseconds>(min_latency_).count() << "us min latency";
        qCDebug(CategoryVideoPlayback) << std::chrono::duration_cast<std::chrono::microseconds>(min_timing_diff_).count() << "us max render time";
        qCDebug(CategoryVideoPlayback) << std::chrono::duration_cast<std::chrono::microseconds>(playback_time_interval_).count() << "us time unit";
        max_diff_time_ = max_texture_diff_time_ = max_latency_ = min_timing_diff_ = std::chrono::seconds(-10);
        min_diff_time_ = min_texture_diff_time_ = min_latency_ = std::chrono::seconds(10);
        frames_per_second_ = renders_per_second_ = texture_updates_per_second_ = 0;
    }
#endif
}

QSGRenderNode::StateFlags VideoFrameRenderNodeOGL::changedStates() const
{
    return ColorState | BlendState | ScissorState | StencilState;
}

QSGRenderNode::RenderingFlags VideoFrameRenderNodeOGL::flags() const
{
    //The node itself is DepthAwareRendering, but the flag causes strange behavior (Rectangles being clipped unexpectedly)
    return BoundedRectRendering;
}

QRectF VideoFrameRenderNodeOGL::rect() const
{
    return QRect(0, 0, width_, height_);
}

void VideoFrameRenderNodeOGL::AddVideoFrame(const QSharedPointer<VideoFrame> &frame)
{
#ifdef _DEBUG
    frames_per_second_ += 1;
#endif
    auto ritr = video_frames_.rbegin(), ritr_end = video_frames_.rend();
    for (; ritr != ritr_end; ++ritr)
        if ((*ritr)->present_time < frame->present_time)
            break;
    video_frames_.insert(ritr.base(), frame);
    RemoveImpossibleFrame(PlaybackClock::now());
}

void VideoFrameRenderNodeOGL::AddVideoFrames(std::vector<QSharedPointer<VideoFrame>> &&frames)
{
#ifdef _DEBUG
    frames_per_second_ += frames.size();
#endif
    for (auto &frame : frames)
    {
        auto ritr = video_frames_.rbegin(), ritr_end = video_frames_.rend();
        for (; ritr != ritr_end; ++ritr)
            if ((*ritr)->present_time < frame->present_time)
                break;
        video_frames_.insert(ritr.base(), std::move(frame));
    }
    RemoveImpossibleFrame(PlaybackClock::now());
}

void VideoFrameRenderNodeOGL::Synchronize(QQuickItem *item)
{
    bool size_changed = false;
    if (width_ != item->width())
    {
        width_ = item->width();
        size_changed = true;
    }
    if (height_ != item->height())
    {
        height_ = item->height();
        size_changed = true;
    }
    if (size_changed)
    {
        this->markDirty(DirtyGeometry);
        vertex_buffer_need_update_ = true;
    }
    this->markDirty(DirtyMaterial);

    QQuickWindow *window = item->window();
    if (window)
    {
        QScreen *screen = window->screen();
        if (screen_ != screen)
        {
            screen_ = screen;
            if (screen)
            {
                static constexpr auto kTickPerSecond = std::chrono::duration_cast<PlaybackClock::duration>(std::chrono::seconds(1)).count();
                refresh_rate_ = ScreenRefreshRate(screen->refreshRate());
                playback_time_interval_ = PlaybackClock::duration(static_cast<PlaybackClock::duration::rep>(round(kTickPerSecond / refresh_rate_)));
            }
        }
    }
}

void VideoFrameRenderNodeOGL::ResynchronizeTimer(PlaybackClock::time_point current_time)
{
    qCDebug(CategoryVideoPlayback) << "Resynchronizing clock, Error: " << std::chrono::duration_cast<std::chrono::microseconds>(current_time - playback_time_base_).count() << "us";
    playback_time_base_ = current_time;
    playback_time_tick_ = 0;
}

void VideoFrameRenderNodeOGL::RemoveImpossibleFrame(PlaybackClock::time_point current_time)
{
    auto itr_begin = video_frames_.begin(), itr_end = video_frames_.end();
    auto itr = itr_begin;
    for (; itr != itr_end; ++itr)
        if ((*itr)->present_time > current_time)
            break;
#ifdef _DEBUG
    if (itr != itr_begin)
        qCDebug(CategoryVideoPlayback) << "Skipping " << itr - itr_begin << "impossible frame";
#endif
    video_frames_.erase(itr_begin, itr);
}
