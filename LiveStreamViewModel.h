#ifndef LIVESTREAMVIEWMODEL_H
#define LIVESTREAMVIEWMODEL_H

class LiveStreamSource;
class LiveStreamSourceModel;
class LiveStreamViewLayoutModel;
class AudioOutput;

class LiveStreamViewInfo
{
    Q_GADGET

    Q_PROPERTY(int row READ row)
    Q_PROPERTY(int column READ column)
    Q_PROPERTY(int rowSpan READ rowSpan)
    Q_PROPERTY(int columnSpan READ columnSpan)
    Q_PROPERTY(int sourceId READ sourceId)
    Q_PROPERTY(LiveStreamSource *source READ source)
    Q_PROPERTY(AudioOutput *audioOut READ audioOut)
public:
    LiveStreamViewInfo() = default;
    LiveStreamViewInfo(int row, int column, int row_span, int column_span, int source_id, LiveStreamSource *source, AudioOutput *audio_out)
        :row_(row), column_(column), row_span_(row_span), column_span_(column_span), source_id_(source_id), source_(source), audio_out_(audio_out)
    {
    }

    int row() const { return row_; }
    int column() const { return column_; }
    int rowSpan() const { return row_span_; }
    int columnSpan() const { return column_span_; }
    int sourceId() const { return source_id_; }
    LiveStreamSource *source() const { return source_; }
    void setSource(int source_id, LiveStreamSource *source) { source_id_ = source_id; source_ = source; }
    AudioOutput *audioOut() const { return audio_out_; }
private:
    int row_;
    int column_;
    int row_span_;
    int column_span_;
    int source_id_;
    LiveStreamSource *source_;
    AudioOutput *audio_out_;
};

class LiveStreamViewModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(LiveStreamSourceModel *sourceModel READ sourceModel WRITE setSourceModel NOTIFY sourceModelChanged)
    Q_PROPERTY(int rows READ rows NOTIFY rowsChanged)
    Q_PROPERTY(int columns READ columns NOTIFY columnsChanged)
public:
    explicit LiveStreamViewModel(QObject *parent = nullptr);
    ~LiveStreamViewModel();

    int rowCount(const QModelIndex & = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;

    LiveStreamSourceModel *sourceModel() const { return source_model_; }
    void setSourceModel(LiveStreamSourceModel *new_source_model);

    int rows() const { return rows_; }
    void setRows(int new_rows) { if (rows_ != new_rows) { rows_ = new_rows; emit rowsChanged(); } }
    int columns() const { return columns_; }
    void setColumns(int new_columns) { if (columns_ != new_columns) { columns_ = new_columns; emit columnsChanged(); } }

    Q_INVOKABLE void resetLayout(LiveStreamViewLayoutModel *layout_model);
    Q_INVOKABLE void setSource(int index, int source_id);
signals:
    void sourceModelChanged();
    void rowsChanged();
    void columnsChanged();
private slots:
    void OnDeleteSource(int source_id);
private:
    LiveStreamSourceModel *source_model_ = nullptr;

    QThread audio_thread_;
    AudioOutput *audio_out_ = nullptr;

    int rows_ = 1, columns_ = 1;
    std::vector<LiveStreamViewInfo> view_info_;
};

#endif // LIVESTREAMVIEWMODEL_H
