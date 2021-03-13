#include "pch.h"
#include "VideoFrameRenderNodeOGL.h"

Q_LOGGING_CATEGORY(CategoryVideoPlayback, "qddm.video")

namespace
{

static constexpr int kVertexCount = 4 * 2; //quad
static constexpr int kVertexSize = kVertexCount * sizeof(GLfloat);

constexpr inline qreal ScreenRefreshRate(qreal refresh_rate)
{
    if (refresh_rate >= 59 && refresh_rate <= 60)
        refresh_rate = 60; //Manual patch
    return refresh_rate;
}

constexpr int DivideTwoRoundUp(int a)
{
    return a / 2 + a % 2;
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

void UpdateSingleTexture(QOpenGLTexture *texture, QOpenGLTexture::PixelFormat pixel_format, QOpenGLTexture::PixelType pixel_type, const void *data, int line_size, int actual_line_size, int height)
{
    if (line_size == actual_line_size)
    {
        texture->setData(pixel_format, pixel_type, data);
    }
    else if (actual_line_size % 8 != 0 && line_size % 8 == 0 && (uintptr_t)data % 8 == 0)
    {
        QOpenGLPixelTransferOptions options;
        options.setAlignment(8);
        texture->setData(pixel_format, pixel_type, data, &options);
    }
    else
    {
        //Have to use another buffer there
        thread_local std::vector<char> buffer;
        buffer.resize(actual_line_size * height);
        for (int i = 0; i < height; ++i)
            memcpy(buffer.data() + i * actual_line_size, static_cast<const char *>(data) + i * line_size, actual_line_size);

        QOpenGLPixelTransferOptions options;
        options.setAlignment(1);
        texture->setData(pixel_format, pixel_type, static_cast<const char *>(buffer.data()), &options);
    }
}

}

VideoFrameRenderNodeOGL::VideoFrameRenderNodeOGL()
{

}

VideoFrameRenderNodeOGL::~VideoFrameRenderNodeOGL()
{
    releaseResources();
}

void VideoFrameRenderNodeOGL::releaseResources()
{
    shader_ = nullptr;
    frame_size_ = QSize();
    pixel_format_ = AV_PIX_FMT_NONE;
    color_range_ = AVCOL_RANGE_UNSPECIFIED;
    colorspace_ = AVCOL_SPC_UNSPECIFIED;
    vertex_buffer_ = nullptr;
    texture_0_ = nullptr;
    texture_1_ = nullptr;
    texture_2_ = nullptr;
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
        InitSingleTexture(texture_0_, frame_size_.width(), frame_size_.height(), QOpenGLTexture::RGBA8_UNorm, QOpenGLTexture::RGBA, QOpenGLTexture::UInt8);
        texture_1_ = nullptr; Q_ASSERT(texture_1_uniform_index_ == -1);
        texture_2_ = nullptr; Q_ASSERT(texture_2_uniform_index_ == -1);
        break;
    case AV_PIX_FMT_NV12:
    case AV_PIX_FMT_NV21:
        InitSingleTexture(texture_0_, frame_size_.width(), frame_size_.height(), QOpenGLTexture::R8_UNorm, QOpenGLTexture::Red, QOpenGLTexture::UInt8);
        InitSingleTexture(texture_1_, DivideTwoRoundUp(frame_size_.width()), DivideTwoRoundUp(frame_size_.height()), QOpenGLTexture::RG8_UNorm, QOpenGLTexture::RG, QOpenGLTexture::UInt8);
        texture_2_ = nullptr; Q_ASSERT(texture_2_uniform_index_ == -1);
        break;
    case AV_PIX_FMT_YUV444P:
    case AV_PIX_FMT_YUVJ444P:
        InitSingleTexture(texture_0_, frame_size_.width(), frame_size_.height(), QOpenGLTexture::R8_UNorm, QOpenGLTexture::Red, QOpenGLTexture::UInt8);
        InitSingleTexture(texture_1_, frame_size_.width(), frame_size_.height(), QOpenGLTexture::R8_UNorm, QOpenGLTexture::Red, QOpenGLTexture::UInt8);
        InitSingleTexture(texture_2_, frame_size_.width(), frame_size_.height(), QOpenGLTexture::R8_UNorm, QOpenGLTexture::Red, QOpenGLTexture::UInt8);
        break;
    default:
        qCWarning(CategoryVideoPlayback) << "Unsupported pixel format";
        shader_ = nullptr;
        return;
    }
}

void VideoFrameRenderNodeOGL::UpdateTexture(AVFrame *frame)
{
    switch (pixel_format_)
    {
    case AV_PIX_FMT_RGB0:
        UpdateSingleTexture(texture_0_.get(), QOpenGLTexture::RGBA, QOpenGLTexture::UInt8, frame->data[0], frame->linesize[0], frame_size_.width() * sizeof(GLubyte), frame_size_.height());
        break;
    case AV_PIX_FMT_NV12:
    case AV_PIX_FMT_NV21:
        UpdateSingleTexture(texture_0_.get(), QOpenGLTexture::Red, QOpenGLTexture::UInt8, frame->data[0], frame->linesize[0], frame_size_.width() * sizeof(GLubyte), frame_size_.height());
        UpdateSingleTexture(texture_1_.get(), QOpenGLTexture::RG, QOpenGLTexture::UInt8, frame->data[1], frame->linesize[1], DivideTwoRoundUp(frame_size_.width()) * sizeof(GLubyte) * 2, DivideTwoRoundUp(frame_size_.height()));
        break;
    case AV_PIX_FMT_YUV444P:
    case AV_PIX_FMT_YUVJ444P:
        UpdateSingleTexture(texture_0_.get(), QOpenGLTexture::Red, QOpenGLTexture::UInt8, frame->data[0], frame->linesize[0], frame_size_.width() * sizeof(GLubyte), frame_size_.height());
        UpdateSingleTexture(texture_1_.get(), QOpenGLTexture::Red, QOpenGLTexture::UInt8, frame->data[1], frame->linesize[1], frame_size_.width() * sizeof(GLubyte), frame_size_.height());
        UpdateSingleTexture(texture_2_.get(), QOpenGLTexture::Red, QOpenGLTexture::UInt8, frame->data[2], frame->linesize[2], frame_size_.width() * sizeof(GLubyte), frame_size_.height());
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
    vertex_buffer_ = std::make_unique<QOpenGLBuffer>();
    vertex_buffer_->create();
    vertex_buffer_->bind();
    vertex_buffer_->allocate(kVertexSize + sizeof(kTexCoordIn));
    vertex_buffer_->write(kVertexSize, kTexCoordIn, sizeof(kTexCoordIn));
    vertex_buffer_->release();
}

void VideoFrameRenderNodeOGL::render(const RenderState *state)
{
    auto playback_time = playback_time_base_ + playback_time_interval_ * playback_time_tick_;
    auto current_time = PlaybackClock::now();
    if (std::chrono::abs(playback_time - current_time) > playback_time_interval_ * 2)
    {
        ResynchronizeTimer(current_time);
        playback_time = current_time;
        playback_time_tick_ = 1; //Progress tick
    }
    else
    {
        playback_time_tick_ += 1;
    }

    auto present_time_limit = playback_time;
    auto frame_itr = video_frames_.begin(), frame_itr_end = video_frames_.end();
    for (; frame_itr != frame_itr_end; ++frame_itr)
        if ((*frame_itr)->present_time > present_time_limit)
            break;

    QSharedPointer<VideoFrame> next_frame;
    if (frame_itr != frame_itr_end)
    {
        next_frame = std::move(*frame_itr);
        video_frames_.erase(video_frames_.begin(), ++frame_itr);
#ifdef _DEBUG
        auto timing_diff = present_time_limit - next_frame->present_time;
        if (timing_diff < min_timing_diff_)
            min_timing_diff_ = timing_diff;
        texture_updates_per_second_ += 1;
        if (current_time - last_texture_change_time_ > max_texture_diff_time_)
            max_texture_diff_time_ = current_time - last_texture_change_time_;
        if (current_time - last_texture_change_time_ < min_texture_diff_time_)
            min_texture_diff_time_ = current_time - last_texture_change_time_;
        if (current_time - next_frame->present_time > max_latency_)
            max_latency_ = current_time - next_frame->present_time;
        if (current_time - next_frame->present_time < min_latency_)
            min_latency_ = current_time - next_frame->present_time;
        last_texture_change_time_ = current_time;
#endif
    }
    else
    {
        video_frames_.clear();
    }

    if (next_frame)
    {
        if (next_frame->frame->format != pixel_format_)
        {
            pixel_format_ = static_cast<AVPixelFormat>(next_frame->frame->format);
            InitShader();
            frame_size_ = QSize(next_frame->frame->width, next_frame->frame->height);
            InitTexture();
            if (color_matrix_uniform_index_ != -1)
            {
                color_range_ = next_frame->frame->color_range;
                colorspace_ = next_frame->frame->colorspace;
                InitColorMatrix();
            }
        }
        else
        {
            QSize frame_size = QSize(next_frame->frame->width, next_frame->frame->height);
            if (frame_size_ != frame_size)
            {
                frame_size_ = frame_size;
                InitTexture();
            }
            if (color_matrix_uniform_index_ != -1 && (color_range_ != next_frame->frame->color_range || colorspace_ != next_frame->frame->colorspace))
            {
                color_range_ = next_frame->frame->color_range;
                colorspace_ = next_frame->frame->colorspace;
                InitColorMatrix();
            }
        }

        UpdateTexture(next_frame->frame.Get());
    }

    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();

    if (shader_)
    {
        if (!vertex_buffer_)
        {
            InitVertexBuffer();
        }

        shader_->bind();
        shader_->setUniformValue(matrix_uniform_index_, *state->projectionMatrix() * *matrix());
        shader_->setUniformValue(opacity_uniform_index_, float(inheritedOpacity()));
        if (color_matrix_uniform_index_ != -1)
            shader_->setUniformValue(color_matrix_uniform_index_, color_matrix_);

        if (texture_0_)
        {
            f->glActiveTexture(GL_TEXTURE0);
            texture_0_->bind();
            shader_->setUniformValue(texture_0_uniform_index_, 0);
            if (texture_1_)
            {
                f->glActiveTexture(GL_TEXTURE1);
                texture_1_->bind();
                shader_->setUniformValue(texture_1_uniform_index_, 1);
                if (texture_2_)
                {
                    f->glActiveTexture(GL_TEXTURE2);
                    texture_2_->bind();
                    shader_->setUniformValue(texture_2_uniform_index_, 2);
                }
                f->glActiveTexture(GL_TEXTURE0);
            }
        }

        vertex_buffer_->bind();

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

        GLfloat vertices[kVertexCount] = {
            GLfloat(x1), GLfloat(y1),
            GLfloat(x1), GLfloat(y2),
            GLfloat(x2), GLfloat(y2),
            GLfloat(x2), GLfloat(y1),
        };
        static_assert(sizeof(vertices) == kVertexSize);
        vertex_buffer_->write(0, vertices, sizeof(vertices));

        shader_->setAttributeBuffer(0, GL_FLOAT, 0, 2);
        shader_->setAttributeBuffer(1, GL_FLOAT, sizeof(vertices), 2);
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
    }
    else
    {
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
        last_second_ += std::chrono::seconds(1);
        qCDebug(CategoryVideoPlayback, "%d frames in pending queue", video_frames_.size());
        qCDebug(CategoryVideoPlayback, "%d fps from source", frames_per_second_);
        qCDebug(CategoryVideoPlayback, "%d texture updates", texture_updates_per_second_);
        qCDebug(CategoryVideoPlayback, "%lldus max diff (frame to frame)", std::chrono::duration_cast<std::chrono::microseconds>(max_diff_time_).count());
        qCDebug(CategoryVideoPlayback, "%lldus min diff (frame to frame)", std::chrono::duration_cast<std::chrono::microseconds>(min_diff_time_).count());
        qCDebug(CategoryVideoPlayback, "%lldus max diff (texture to texture)", std::chrono::duration_cast<std::chrono::microseconds>(max_texture_diff_time_).count());
        qCDebug(CategoryVideoPlayback, "%lldus min diff (texture to texture)", std::chrono::duration_cast<std::chrono::microseconds>(min_texture_diff_time_).count());
        qCDebug(CategoryVideoPlayback, "%lldus max latency", std::chrono::duration_cast<std::chrono::microseconds>(max_latency_).count());
        qCDebug(CategoryVideoPlayback, "%lldus min latency", std::chrono::duration_cast<std::chrono::microseconds>(min_latency_).count());
        qCDebug(CategoryVideoPlayback, "%lldus min timing diff", std::chrono::duration_cast<std::chrono::microseconds>(min_timing_diff_).count());
        max_diff_time_ = max_texture_diff_time_ = max_latency_ = std::chrono::seconds(-10);
        min_diff_time_ = min_texture_diff_time_ = min_latency_ = min_timing_diff_ = std::chrono::seconds(10);
        frames_per_second_ = renders_per_second_ = texture_updates_per_second_ = 0;
    }
#endif
}

QSGRenderNode::StateFlags VideoFrameRenderNodeOGL::changedStates() const
{
    return BlendState | ScissorState | StencilState | CullState;
}

QSGRenderNode::RenderingFlags VideoFrameRenderNodeOGL::flags() const
{
    return BoundedRectRendering | DepthAwareRendering;
}

QRectF VideoFrameRenderNodeOGL::rect() const
{
    return QRect(0, 0, width_, height_);
}

void VideoFrameRenderNodeOGL::AddVideoFrame(const QSharedPointer<VideoFrame> &frame)
{
    auto ritr = video_frames_.rbegin(), ritr_end = video_frames_.rend();
    for (; ritr != ritr_end; ++ritr)
        if ((*ritr)->present_time < frame->present_time)
            break;
    video_frames_.insert(ritr.base(), frame);
    this->markDirty(DirtyMaterial);
}

void VideoFrameRenderNodeOGL::AddVideoFrames(std::vector<QSharedPointer<VideoFrame>> &&frames)
{
    for (auto &frame : frames)
    {
        auto ritr = video_frames_.rbegin(), ritr_end = video_frames_.rend();
        for (; ritr != ritr_end; ++ritr)
            if ((*ritr)->present_time < frame->present_time)
                break;
        video_frames_.insert(ritr.base(), std::move(frame));
    }
    this->markDirty(DirtyMaterial);
}

void VideoFrameRenderNodeOGL::Synchronize(QQuickItem *item)
{
    bool size_changed = false;
    if (width_ != item->width())
    {
        width_ = item->width();
    }
    if (height_ != item->height())
    {
        height_ = item->height();
        size_changed = true;
    }
    if (size_changed)
    {
        this->markDirty(DirtyGeometry);
    }

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
    qCDebug(CategoryVideoPlayback, "Resynchronizing clock, Error: %lldus", std::chrono::duration_cast<std::chrono::microseconds>(current_time - playback_time_base_).count());
    playback_time_base_ = current_time;
    playback_time_tick_ = 0;
#ifdef _DEBUG
    last_second_ = current_time;
#endif
}
