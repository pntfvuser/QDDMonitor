#ifndef BLOCKQUEUEBUFFER_H
#define BLOCKQUEUEBUFFER_H

class BlockingFIFOBuffer
{
    static constexpr uint64_t kBufferBlockSize = 0x40000;
    struct BufferBlock
    {
        uint8_t block_data[kBufferBlockSize];
    };

public:
    BlockingFIFOBuffer()
    {
    }

    uint64_t SizeLocked()
    {
        QMutexLocker lock(&mutex_);
        return active_chunks_.size() * kBufferBlockSize - front_pos_ + back_pos_ - kBufferBlockSize;
    }

    uint64_t Size()
    {
        return active_chunks_.size() * kBufferBlockSize - front_pos_ + back_pos_ - kBufferBlockSize;
    }

    void Open()
    {
        free_chunks_.splice(free_chunks_.end(), active_chunks_);
        front_pos_ = 0;
        back_pos_ = kBufferBlockSize;
        open_ = true;
    }

    uint64_t Read(uint8_t *data, uint64_t max_size)
    {
        QMutexLocker lock(&mutex_);
        while (open_ && active_chunks_.empty())
            condition_.wait(lock.mutex());
        if (!open_)
            return 0;

        uint64_t size_read = 0;
        while (active_chunks_.size() > 1)
        {
            uint64_t front_size = kBufferBlockSize - front_pos_;
            if (front_size <= max_size)
            {
                memcpy(data, active_chunks_.front().block_data + front_pos_, front_size);
                size_read += front_size;
                data += front_size;
                max_size -= front_size;
                free_chunks_.splice(free_chunks_.end(), active_chunks_, active_chunks_.begin());
                front_pos_ = 0;
            }
            else
            {
                memcpy(data, active_chunks_.front().block_data + front_pos_, max_size);
                size_read += max_size;
                //Don't need to update data and max_size since this is last memcpy
                front_pos_ += max_size;
                return size_read;
            }
        }
        if (!active_chunks_.empty())
        {
            uint64_t front_size = back_pos_ - front_pos_;
            if (front_size <= max_size)
            {
                memcpy(data, active_chunks_.front().block_data + front_pos_, front_size);
                size_read += front_size;
                //Don't need to update data and max_size since it's last memcpy
                free_chunks_.splice(free_chunks_.end(), active_chunks_, active_chunks_.begin());
                front_pos_ = 0;
                back_pos_ = kBufferBlockSize;
            }
            else
            {
                memcpy(data, active_chunks_.front().block_data + front_pos_, max_size);
                size_read += max_size;
                //Don't need to update data and max_size since it's last memcpy
                front_pos_ += max_size;
            }
        }
        return size_read;
    }

    uint64_t Write(const uint8_t *data, uint64_t max_size)
    {
        QMutexLocker lock(&mutex_);
        if (!open_)
            return 0;
        const uint64_t size_written = max_size; //Will always write all data
        if (back_pos_ != kBufferBlockSize) //back_pos_ == kBufferChunkSize when active_chunks_.empty()
        {
            uint64_t back_size = kBufferBlockSize - back_pos_;
            if (back_size >= max_size)
            {
                memcpy(active_chunks_.back().block_data + back_pos_, data, max_size);
                back_pos_ += max_size;
                if (size_written > 0)
                {
                    lock.unlock();
                    condition_.notify_all();
                }
                return size_written;
            }
            else
            {
                memcpy(active_chunks_.back().block_data + back_pos_, data, back_size);
                data += back_size;
                max_size -= back_size;
                //Don't need to update back_pos_ here, since back_size < max_size means it must go into main loop where back_pos_ will be updated
            }
        }
        while (true) //Break is handled inside
        {
            if (free_chunks_.empty())
                active_chunks_.emplace_back();
            else
                active_chunks_.splice(active_chunks_.end(), free_chunks_, free_chunks_.begin());
            if (max_size <= kBufferBlockSize)
            {
                memcpy(active_chunks_.back().block_data, data, max_size);
                back_pos_ = max_size;
                break;
            }
            else
            {
                memcpy(active_chunks_.back().block_data, data, kBufferBlockSize);
                data += kBufferBlockSize;
                max_size -= kBufferBlockSize;
            }
        }
        if (size_written > 0)
        {
            lock.unlock();
            condition_.notify_all();
        }
        return size_written;
    }

    uint64_t Fill(QIODevice *source, uint64_t fill_to)
    {
        QMutexLocker lock(&mutex_);
        if (!open_)
            return 0;
        uint64_t current_size = Size();
        if (current_size >= fill_to)
            return 0;
        uint64_t max_size = fill_to - current_size;
        const uint64_t size_written = max_size; //Will always write all data
        if (back_pos_ != kBufferBlockSize) //back_pos_ == kBufferChunkSize when active_chunks_.empty()
        {
            uint64_t back_size = kBufferBlockSize - back_pos_;
            if (back_size >= max_size)
            {
                source->read(reinterpret_cast<char *>(active_chunks_.back().block_data + back_pos_), max_size);
                back_pos_ += max_size;
                if (size_written > 0)
                {
                    lock.unlock();
                    condition_.notify_all();
                }
                return size_written;
            }
            else
            {
                source->read(reinterpret_cast<char *>(active_chunks_.back().block_data + back_pos_), back_size);
                max_size -= back_size;
                //Don't need to update back_pos_ here, since back_size < max_size means it must go into main loop where back_pos_ will be updated
            }
        }
        while (true) //Break is handled inside
        {
            if (free_chunks_.empty())
                active_chunks_.emplace_back();
            else
                active_chunks_.splice(active_chunks_.end(), free_chunks_, free_chunks_.begin());
            if (max_size <= kBufferBlockSize)
            {
                source->read(reinterpret_cast<char *>(active_chunks_.back().block_data), max_size);
                back_pos_ = max_size;
                break;
            }
            else
            {
                source->read(reinterpret_cast<char *>(active_chunks_.back().block_data), kBufferBlockSize);
                max_size -= kBufferBlockSize;
            }
        }
        if (size_written > 0)
        {
            lock.unlock();
            condition_.notify_all();
        }
        return size_written;
    }

    void Close()
    {
        QMutexLocker lock(&mutex_);
        open_ = false;
        lock.unlock();
        condition_.notify_all();
    }
private:
    QMutex mutex_;
    QWaitCondition condition_;

    bool open_ = false;
    std::list<BufferBlock> active_chunks_, free_chunks_;
    uint64_t front_pos_ = 0, back_pos_ = kBufferBlockSize;
};

#endif // BLOCKQUEUEBUFFER_H
