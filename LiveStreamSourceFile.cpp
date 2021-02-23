#include "pch.h"
#include "LiveStreamSourceFile.h"

LiveStreamSourceFile::LiveStreamSourceFile(QObject *parent)
    :LiveStreamSource(parent), feed_timer_(new QTimer(this))
{
    connect(this, &LiveStreamSourceFile::newInputStream, this, &LiveStreamSourceFile::OnNewInputStream);
    connect(feed_timer_, &QTimer::timeout, this, &LiveStreamSourceFile::FeedTick);
}

LiveStreamSourceFile::~LiveStreamSourceFile()
{
    feed_timer_->stop();
}

QString LiveStreamSourceFile::filePath() const
{
    return file_path_;
}

void LiveStreamSourceFile::setFilePath(const QString &file_path)
{
    if (file_path != file_path_)
    {
        file_path_ = file_path;
        emit filePathChanged();
    }
}

void LiveStreamSourceFile::start()
{
    QMetaObject::invokeMethod(this, "DoStart");
}

void LiveStreamSourceFile::DoStart()
{
    if (fin_)
    {
        fin_->deleteLater();
    }
    fin_ = new QFile(file_path_, this);
    fin_->open(QIODevice::ReadOnly);

    emit newInputStream(this, &LiveStreamSourceFile::AVIOReadCallback);
    feed_timer_->start(50);

    QSharedPointer<SubtitleFrame> dmk = QSharedPointer<SubtitleFrame>::create();
    dmk->content = "test";
    dmk->style = SubtitleStyle::TOP;
    dmk->color = QColor(128, 128, 0);
    emit newSubtitleFrame(dmk);
    dmk = QSharedPointer<SubtitleFrame>::create();
    dmk->content = "test";
    dmk->style = SubtitleStyle::BOTTOM;
    dmk->color = QColor(128, 128, 0);
    emit newSubtitleFrame(dmk);
}

void LiveStreamSourceFile::FeedTick()
{
    if (!open())
    {
        feed_timer_->stop();
        return;
    }

    QSharedPointer<SubtitleFrame> dmk = QSharedPointer<SubtitleFrame>::create();
    dmk->content = "test";
    dmk->style = SubtitleStyle::NORMAL;
    dmk->color = QColor(128, 0, 128);
    emit newSubtitleFrame(dmk);
}

int LiveStreamSourceFile::AVIOReadCallback(void *opaque, uint8_t *buf, int buf_size)
{
    LiveStreamSourceFile *self = reinterpret_cast<LiveStreamSourceFile *>(opaque);
    qint64 read_size = self->fin_->read(reinterpret_cast<char *>(buf), buf_size);
    if (read_size <= 0)
        return AVERROR_EOF;
    return (int)read_size;
}
