#include "pch.h"
#include "D3D11FlushHelper.h"

#include "D3D11SharedResource.h"

D3D11FlushHelper::D3D11FlushHelper(QQuickItem *parent)
    :QQuickItem(parent)
{
    connect(this, &QQuickItem::windowChanged, this, &D3D11FlushHelper::onWindowChanged);
}

void D3D11FlushHelper::onWindowChanged(QQuickWindow *window)
{
    if (window_)
    {
        disconnect(window_, &QQuickWindow::afterSynchronizing, this, &D3D11FlushHelper::onAfterSynchronizing);
    }
    window_ = window;
    if (window_)
        connect(window_, &QQuickWindow::afterSynchronizing, this, &D3D11FlushHelper::onAfterSynchronizing, Qt::DirectConnection);
}

void D3D11FlushHelper::onAfterSynchronizing()
{
    if (window_in_use_ != window_)
    {
        if (window_in_use_)
        {
            disconnect(window_in_use_, nullptr, this, nullptr);
        }
        window_in_use_ = window_;
    }
    //Nodes shouldn't be changed after afterSynchronizing so this must be the last slot to be invoked
    if (window_in_use_ != nullptr)
        connect(window_in_use_, &QQuickWindow::frameSwapped, this, &D3D11FlushHelper::onFrameSwapped, Qt::DirectConnection);
}

void D3D11FlushHelper::onFrameSwapped()
{
    disconnect(window_in_use_, &QQuickWindow::frameSwapped, this, &D3D11FlushHelper::onFrameSwapped);
    D3D11SharedResource *shared_resource = D3D11SharedResource::resource;
    ID3D11DeviceContext *device_context = shared_resource->device_context;
    {
        AVD3D11VADeviceContextLockGuard lock(shared_resource->d3d11_device_ctx);
        device_context->Flush();
    }
}
