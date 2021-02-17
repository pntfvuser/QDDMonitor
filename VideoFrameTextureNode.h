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
        PlaybackClock::time_point frame_time;
        bool do_not_recycle = false;
    };

    static constexpr int kQueueSize = 16, kQueueStartPlaySize = 8;
    static constexpr auto kTextureFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
public:
    VideoFrameTextureNode(QQuickItem *item);
    ~VideoFrameTextureNode();

    QSGTexture *texture() const override;

    void AddVideoFrame(const QSharedPointer<VideoFrame> &frame);

    void Synchronize();
public slots:
    void Render();
    void UpdateScreen();
private:
    void ResynchronizeTimer();

    void NewTextureItem(int count);
    bool RenderFrame(TextureItem &target);

    QQuickWindow *window_ = nullptr;
    QQuickItem *item_ = nullptr;
    QSize size_;
    qreal dpr_;
    float aspect_ratio_ = 0;

    ID3D11Device *device_ = nullptr;

    QVector<QSharedPointer<VideoFrame>> video_frames_;
    QVector<TextureItem> rendered_texture_queue_, empty_texture_queue_;
    TextureItem garbage_texture_;
    bool playing_ = false;

#ifdef _DEBUG
    PlaybackClock::time_point last_frame_time_, last_second_;
    PlaybackClock::duration max_diff_time_, min_diff_time_;
    int frames_per_second_ = 0, renders_per_second_ = 0;
#endif
};

#endif // VIDEOFRAMETEXTURENODE_H
