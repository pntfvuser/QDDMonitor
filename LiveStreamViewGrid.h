#ifndef LIVESTREAMVIEWGRID_H
#define LIVESTREAMVIEWGRID_H

class LiveStreamViewGridAttachedType : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int row READ row WRITE setRow NOTIFY rowChanged)
    Q_PROPERTY(int column READ column WRITE setColumn NOTIFY columnChanged)
    Q_PROPERTY(int rowSpan READ rowSpan WRITE setRowSpan NOTIFY rowSpanChanged)
    Q_PROPERTY(int columnSpan READ columnSpan WRITE setColumnSpan NOTIFY columnSpanChanged)
public:
    explicit LiveStreamViewGridAttachedType(QObject *parent = nullptr) :QObject(parent) {}

    int row() const { return row_; }
    void setRow(int new_row) { if (row_ != new_row) { row_ = new_row; emit rowChanged(); } }
    int column() const { return column_; }
    void setColumn(int new_column) { if (column_ != new_column) { column_ = new_column; emit columnChanged(); } }
    int rowSpan() const { return row_span_; }
    void setRowSpan(int new_row_span) { if (row_span_ != new_row_span) { row_span_ = new_row_span; emit rowSpanChanged(); } }
    int columnSpan() const { return column_span_; }
    void setColumnSpan(int new_column_span) { if (column_span_ != new_column_span) { column_span_ = new_column_span; emit columnSpanChanged(); } }
signals:
    void rowChanged();
    void columnChanged();
    void rowSpanChanged();
    void columnSpanChanged();
private:
    int row_, column_, row_span_, column_span_;
};

class LiveStreamViewGrid : public QQuickItem
{
    Q_OBJECT

    Q_PROPERTY(int rows READ rows WRITE setRows NOTIFY rowsChanged)
    Q_PROPERTY(int columns READ columns WRITE setColumns NOTIFY columnsChanged)
public:
    explicit LiveStreamViewGrid(QQuickItem *parent = nullptr);

    int rows() const { return rows_; }
    void setRows(int rows);
    int columns() const { return columns_; }
    void setColumns(int columns);

    static LiveStreamViewGridAttachedType *qmlAttachedProperties(QObject *parent) { return new LiveStreamViewGridAttachedType(parent); }
signals:
    void rowsChanged();
    void columnsChanged();
private slots:
    void OnWidthChanged();
    void OnHeightChanged();
protected:
    void itemChange(QQuickItem::ItemChange change, const QQuickItem::ItemChangeData &value) override;
private:
    void AddChild(QQuickItem *child);
    void RemoveChild(QQuickItem *child);

    void Reposition();
    void RepositionChild(qreal row_height, qreal column_width, QQuickItem *child);

    int rows_ = 1, columns_ = 1;
    qreal row_height = 0, column_width = 0;
    bool repositioning_ = false;
};
QML_DECLARE_TYPEINFO(LiveStreamViewGrid, QML_HAS_ATTACHED_PROPERTIES)

#endif // LIVESTREAMVIEWGRID_H
