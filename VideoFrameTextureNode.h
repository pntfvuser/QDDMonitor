#ifndef VIDEOFRAMETEXTURENODE_H
#define VIDEOFRAMETEXTURENODE_H

#include "VideoFrame.h"

class QQuickWindow;
class QQuickItem;

class VideoFrameTextureNode : public QSGTextureProvider, public QSGSimpleTextureNode
{
    Q_OBJECT

    struct TextureItem
    {
        TextureItem() = default;
        TextureItem(QQuickWindow *window, ID3D11Device *device, const D3D11_TEXTURE2D_DESC &desc, const QSize &size);
        TextureItem(const TextureItem &) = default;
        TextureItem(TextureItem &&) = default;
        TextureItem &operator=(const TextureItem &) = default;
        TextureItem &operator=(TextureItem &&) = default;
        ~TextureItem() = default;

        ComPtr<ID3D11Texture2D> texture;
        HANDLE texture_share_handle;
        std::shared_ptr<QSGTexture> texture_qsg;
        PlaybackClock::time_point present_time;
        bool do_not_recycle = false;
    };

    static constexpr size_t kUsedQueueSize = 6;
    static constexpr auto kTextureFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
public:
    static constexpr size_t kQueueSize = 12;

    VideoFrameTextureNode(QQuickItem *item);
    ~VideoFrameTextureNode();

    QSGTexture *texture() const override;

    void AddVideoFrame(const QSharedPointer<VideoFrame> &frame);
    void AddVideoFrames(std::vector<QSharedPointer<VideoFrame>> &&frame);

    void Synchronize(QQuickItem *item);
public slots:
    void Render();
    void UpdateScreen(QScreen *screen);
private:
    void UpdateWindow(QQuickWindow *new_window);
    void ResynchronizeTimer(PlaybackClock::time_point current_time = PlaybackClock::now());

    void NewTextureItem(int count);
    bool RenderFrame(TextureItem &target);

    QQuickWindow *window_ = nullptr;
    QQuickItem *item_ = nullptr;
    QSize size_;
    qreal dpr_;
    float aspect_ratio_ = 0;

    ID3D11Device *device_ = nullptr;

    std::vector<QSharedPointer<VideoFrame>> video_frames_;
    std::vector<TextureItem> empty_texture_queue_, rendered_texture_queue_, used_texture_queue_;
    PlaybackClock::time_point playback_time_base_;
    PlaybackClock::duration playback_time_interval_;
    int playback_time_tick_;
    qreal refresh_rate_ = 1;

#ifdef _DEBUG
    PlaybackClock::time_point last_frame_time_, last_texture_change_time_, last_second_;
    PlaybackClock::duration max_diff_time_, min_diff_time_, max_texture_diff_time_, min_texture_diff_time_, max_latency_, min_latency_, min_timing_diff_;
    int frames_per_second_ = 0, renders_per_second_ = 0, texture_updates_per_second_ = 0;
#endif
};

#endif // VIDEOFRAMETEXTURENODE_H
