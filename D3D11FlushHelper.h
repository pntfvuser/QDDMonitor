#ifndef D3D11FLUSHHELPER_H
#define D3D11FLUSHHELPER_H

class D3D11FlushHelper : public QQuickItem
{
    Q_OBJECT
public:
    explicit D3D11FlushHelper(QQuickItem *parent = nullptr);
public slots:
    void onWindowChanged(QQuickWindow *window);
    void onAfterSynchronizing();
    void onFrameSwapped();
private:
    QQuickWindow *window_ = nullptr, *window_in_use_ = nullptr;
};

#endif // D3D11FLUSHHELPER_H
