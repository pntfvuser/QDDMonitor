#include "pch.h"
#include "LiveStreamSourceFile.h"

LiveStreamSourceFile::LiveStreamSourceFile(QObject *parent)
    :LiveStreamSource(parent), feed_timer_(new QTimer(this))
{
    connect(this, &LiveStreamSourceFile::startSignal, this, &LiveStreamSourceFile::DoStart);
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
    emit startSignal();
}

void LiveStreamSourceFile::DoStart()
{
    if (fin_)
    {
        fin_->deleteLater();
    }
    fin_ = new QFile(file_path_, this);
    fin_->open(QIODevice::ReadOnly);
    if (!fin_->isOpen())
    {
        emit InvalidSourceArgument();
        return;
    }

    emit RequestNewInputStream(file_path_);
    feed_timer_->start(50);

    QSharedPointer<SubtitleFrame> dmk = QSharedPointer<SubtitleFrame>::create();
    dmk->content = "test";
    dmk->style = SubtitleStyle::TOP;
    dmk->color = QColor(128, 128, 0);
    emit NewSubtitleFrame(dmk);
    dmk = QSharedPointer<SubtitleFrame>::create();
    dmk->content = "test";
    dmk->style = SubtitleStyle::BOTTOM;
    dmk->color = QColor(128, 128, 0);
    emit NewSubtitleFrame(dmk);
}

void LiveStreamSourceFile::FeedTick()
{
    PushData(fin_);

    QSharedPointer<SubtitleFrame> dmk = QSharedPointer<SubtitleFrame>::create();
    dmk->content = "test";
    dmk->style = SubtitleStyle::NORMAL;
    dmk->color = QColor(128, 0, 128);
    emit NewSubtitleFrame(dmk);
}
