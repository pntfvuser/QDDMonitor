#ifndef LIVESTREAMSOURCEMODEL_H
#define LIVESTREAMSOURCEMODEL_H

class LiveStreamSourceModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    explicit LiveStreamSourceModel(QObject *parent = nullptr);
};

#endif // LIVESTREAMSOURCEMODEL_H
