#ifndef AVOBJECTWRAPPER_H
#define AVOBJECTWRAPPER_H

template <typename ObjectType, class ReleaseFunctor>
class AVObjectBase
{
public:
    AVObjectBase() :object(nullptr) {}
    AVObjectBase(std::nullptr_t) :object(nullptr) {}
    AVObjectBase(ObjectType *ptr) :object(ptr) {}
    AVObjectBase(const AVObjectBase &obj) = delete;
    AVObjectBase(AVObjectBase &&obj) :object(obj.object) { obj.object = nullptr; }

    AVObjectBase &operator=(std::nullptr_t)
    {
        if (object != nullptr)
            ReleaseFunctor()(&object);
        return *this;
    }
    AVObjectBase &operator=(ObjectType *ptr)
    {
        if (object != nullptr)
            ReleaseFunctor()(&object);
        object = ptr;
        return *this;
    }
    AVObjectBase &operator=(const AVObjectBase &rhs) = delete;
    AVObjectBase &operator=(AVObjectBase &&rhs)
    {
        if (object != nullptr)
            ReleaseFunctor()(&object);
        object = rhs.object;
        rhs.object = nullptr;
        return *this;
    }

    ~AVObjectBase()
    {
        if (object != nullptr)
            ReleaseFunctor()(&object);
    }

    ObjectType *DetachObject()
    {
        ObjectType *ptr = object;
        object = nullptr;
        return ptr;
    }

    operator bool() const { return object != nullptr; }
    bool operator!() const { return object == nullptr; }

    const ObjectType *Get() const { return object; }
    ObjectType *Get() { return object; }
    ObjectType **GetAddressOf() { return &object; }
    const ObjectType &operator*() const { return *object; }
    ObjectType &operator*() { return *object; }
    const ObjectType *operator->() const { return object; }
    ObjectType *operator->() { return object; }
private:
    ObjectType *object;
};

struct AVBufferRefReleaseFunctor
{
    void operator()(AVBufferRef **object) const { av_buffer_unref(object); }
};
using AVBufferRefObject = AVObjectBase<AVBufferRef, AVBufferRefReleaseFunctor>;

struct AVFrameReleaseFunctor
{
    void operator()(AVFrame **object) const { av_frame_free(object); }
};
using AVFrameObject = AVObjectBase<AVFrame, AVFrameReleaseFunctor>;

struct AVPacketReleaseFunctor
{
    void operator()(AVPacket **object) const { av_packet_free(object); }
};
using AVPacketObject = AVObjectBase<AVPacket, AVPacketReleaseFunctor>;

Q_DECLARE_METATYPE(const AVCodecContext *);

#endif // AVOBJECTWRAPPER_H
