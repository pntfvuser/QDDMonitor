#ifndef VIDEOFRAMERENDERNODEOGL_H
#define VIDEOFRAMERENDERNODEOGL_H

#include "VideoFrame.h"

class VideoFrameRenderNodeOGL : public QSGRenderNode
{
    static constexpr int kTextureItemCount = 3;
    struct PixelUnpackBufferItem
    {
        std::unique_ptr<QOpenGLBuffer> buffers[kTextureItemCount];
        PlaybackClock::time_point present_time;

        QSize frame_size;
        AVPixelFormat pixel_format = AV_PIX_FMT_NONE;
        AVColorRange color_range = AVCOL_RANGE_UNSPECIFIED;
        AVColorSpace colorspace = AVCOL_SPC_UNSPECIFIED;

        bool IsCompatible(AVFrame *frame);
    };

    static constexpr size_t kQueueSize = 6;
    static constexpr size_t kUsedQueueSize = 2;
public:
    VideoFrameRenderNodeOGL();
    ~VideoFrameRenderNodeOGL();

    void render(const RenderState *state) override;
    void releaseResources() override;
    StateFlags changedStates() const override;
    RenderingFlags flags() const override;
    QRectF rect() const override;

    void AddVideoFrame(const QSharedPointer<VideoFrame> &frame);
    void AddVideoFrames(std::vector<QSharedPointer<VideoFrame>> &&frames);

    void Synchronize(QQuickItem *item);
private:
    void InitShader();
    void InitTexture();
    void UpdateTexture(PixelUnpackBufferItem &item);
    static void InitPixelUnpackBuffer(PixelUnpackBufferItem &item);
    static void UpdatePixelUnpackBuffer(PixelUnpackBufferItem &item, AVFrame *frame);
    void InitColorMatrix();
    void InitVertexBuffer();

    void Upload(PlaybackClock::time_point current_time);

    void ResynchronizeTimer(PlaybackClock::time_point current_time);

    int width_ = 0, height_ = 0;
    QScreen *screen_ = nullptr;

    std::vector<QSharedPointer<VideoFrame>> video_frames_;
    PlaybackClock::time_point playback_time_base_;
    PlaybackClock::duration playback_time_interval_ = 1s;
    int playback_time_tick_;
    qreal refresh_rate_ = 1;

    QSize frame_size_;
    AVPixelFormat pixel_format_ = AV_PIX_FMT_NONE;
    AVColorRange color_range_ = AVCOL_RANGE_UNSPECIFIED;
    AVColorSpace colorspace_ = AVCOL_SPC_UNSPECIFIED;
    QMatrix4x4 color_matrix_;

    std::unique_ptr<QOpenGLShaderProgram> shader_;
    int matrix_uniform_index_ = -1, opacity_uniform_index_ = -1, color_matrix_uniform_index_ = -1, texture_0_uniform_index_ = -1, texture_1_uniform_index_ = -1, texture_2_uniform_index_ = -1;
    std::unique_ptr<QOpenGLTexture> textures_[kTextureItemCount];
    std::vector<PixelUnpackBufferItem> texture_buffers_uploaded_, texture_buffers_empty_, texture_buffers_used_;
    std::unique_ptr<QOpenGLBuffer> vertex_buffer_;
    bool vertex_buffer_need_update_ = false;

#ifdef _DEBUG
    PlaybackClock::time_point last_frame_time_, last_texture_change_time_, last_second_;
    PlaybackClock::duration max_diff_time_, min_diff_time_, max_texture_diff_time_, min_texture_diff_time_, max_latency_, min_latency_, min_timing_diff_;
    int frames_per_second_ = 0, renders_per_second_ = 0, texture_updates_per_second_ = 0;
#endif
};

#endif // VIDEOFRAMERENDERNODEOGL_H
