#ifndef LIVESTREAMVIEWLAYOUTMODEL_H
#define LIVESTREAMVIEWLAYOUTMODEL_H

class LiveStreamViewLayoutItem
{
    Q_GADGET

    Q_PROPERTY(int row READ row)
    Q_PROPERTY(int column READ column)
    Q_PROPERTY(int rowSpan READ rowSpan)
    Q_PROPERTY(int columnSpan READ columnSpan)
public:
    LiveStreamViewLayoutItem() = default;
    LiveStreamViewLayoutItem(int row, int column, int row_span, int column_span) :row_(row), column_(column), row_span_(row_span), column_span_(column_span) {}

    int row() const { return row_; }
    int column() const { return column_; }
    int rowSpan() const { return row_span_; }
    int columnSpan() const { return column_span_; }
private:
    int row_;
    int column_;
    int row_span_;
    int column_span_;
};

class LiveStreamViewLayoutModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(int rows READ rows NOTIFY rowsChanged)
    Q_PROPERTY(int columns READ columns NOTIFY columnsChanged)
public:
    explicit LiveStreamViewLayoutModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex & = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;

    int rows() const { return rows_; }
    void setRows(int new_rows) { if (rows_ != new_rows) { rows_ = new_rows; emit rowsChanged(); } }
    int columns() const { return columns_; }
    void setColumns(int new_columns) { if (columns_ != new_columns) { columns_ = new_columns; emit columnsChanged(); } }

    Q_INVOKABLE void resetLayout(int rows, int columns);
    Q_INVOKABLE void addLayoutItem(int row, int column, int row_span, int column_span);

    Q_INVOKABLE bool itemWillIntersect(int row, int column, int row_span, int column_span) const;

    const std::vector<LiveStreamViewLayoutItem> &LayoutItems() const { return layout_items_; }
signals:
    void rowsChanged();
    void columnsChanged();
private:
    int rows_ = 1, columns_ = 1;
    std::vector<LiveStreamViewLayoutItem> layout_items_;
};

#endif // LIVESTREAMVIEWLAYOUTMODEL_H
