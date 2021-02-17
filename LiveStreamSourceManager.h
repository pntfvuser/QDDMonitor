#ifndef LIVESTREAMSOURCEMANAGER_H
#define LIVESTREAMSOURCEMANAGER_H

#include <QObject>

class LiveStreamSourceManager : public QObject
{
    Q_OBJECT
public:
    explicit LiveStreamSourceManager(QObject *parent = nullptr);

signals:

};

#endif // LIVESTREAMSOURCEMANAGER_H
