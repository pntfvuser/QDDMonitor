#ifndef LIVESTREAMVIEWMODEL_H
#define LIVESTREAMVIEWMODEL_H

class LiveStreamSource;
class AudioOutput;

class LiveStreamViewInfo
{
    Q_GADGET

    Q_PROPERTY(int row READ row)
    Q_PROPERTY(int column READ column)
    Q_PROPERTY(int rowSpan READ rowSpan)
    Q_PROPERTY(int columnSpan READ columnSpan)
    Q_PROPERTY(LiveStreamSource *source READ source)
    Q_PROPERTY(AudioOutput *audioOut READ audioOut)
public:
    LiveStreamViewInfo() = default;
    LiveStreamViewInfo(int row, int column, int row_span, int column_span, LiveStreamSource *source, AudioOutput *audio_out) :row_(row), column_(column), row_span_(row_span), column_span_(column_span), source_(source), audio_out_(audio_out) {}

    int row() const { return row_; }
    int column() const { return column_; }
    int rowSpan() const { return row_span_; }
    int columnSpan() const { return column_span_; }
    LiveStreamSource *source() const { return source_; }
    AudioOutput *audioOut() const { return audio_out_; }
private:
    int row_;
    int column_;
    int row_span_;
    int column_span_;
    LiveStreamSource *source_;
    AudioOutput *audio_out_;
};

class LiveStreamViewModel : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit LiveStreamViewModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex & = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
private:
    std::vector<LiveStreamViewInfo> view_info_;
};

#endif // LIVESTREAMVIEWMODEL_H
