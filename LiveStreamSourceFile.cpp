#include "pch.h"
#include "LiveStreamSourceFile.h"

LiveStreamSourceFile::LiveStreamSourceFile(const QString &file_path, QObject *parent)
    :LiveStreamSource(parent), file_path_(file_path), feed_timer_(new QTimer(this))
{
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

LiveStreamSourceFile *LiveStreamSourceFile::FromJson(const QJsonObject &json, QObject *parent)
{
    QString file_path = json.value("file_path").toString();
    if (file_path.isEmpty())
        return nullptr;
    LiveStreamSourceFile *source = new LiveStreamSourceFile(file_path, parent);
    return source;
}

QString LiveStreamSourceFile::SourceType() const
{
    return "file";
}

QJsonObject LiveStreamSourceFile::ToJson() const
{
    QJsonObject obj;
    obj["file_path"] = file_path_;
    return obj;
}

void LiveStreamSourceFile::UpdateInfo()
{
    emit infoUpdated(STATUS_ONLINE, file_path_, QList<QString>());
}

void LiveStreamSourceFile::Activate(const QString &)
{
    if (fin_)
        return;
    fin_ = new QFile(file_path_, this);
    fin_->open(QIODevice::ReadOnly);
    if (!fin_->isOpen())
    {
        fin_->deleteLater();
        fin_ = nullptr;
        emit invalidSourceArgument();
        return;
    }

    BeginData();
    emit newInputStream(file_path_);
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

void LiveStreamSourceFile::Deactivate()
{
    if (!fin_)
        return;
    fin_->deleteLater();
    fin_ = nullptr;
    CloseData();
    emit deleteInputStream();
}

void LiveStreamSourceFile::FeedTick()
{
    if (!fin_)
    {
        feed_timer_->stop();
        return;
    }

    PushData(fin_);
    if (fin_->atEnd())
        EndData();

    QSharedPointer<SubtitleFrame> dmk = QSharedPointer<SubtitleFrame>::create();
    dmk->content = "test";
    dmk->style = SubtitleStyle::NORMAL;
    dmk->color = QColor(128, 0, 128);
    emit newSubtitleFrame(dmk);
}
