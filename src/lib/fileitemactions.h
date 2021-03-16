#ifndef FILEITEMACTIONS_H
#define FILEITEMACTIONS_H

#include <QObject>
#include <KService>

class FileItemActions : public QObject
{
    Q_OBJECT

public:
    explicit FileItemActions(QObject *parent = nullptr);

    static KService::List associatedApplications(const QStringList& mimeTypeList, const QString& traderConstraint);

};

#endif // FILEITEMACTIONS_H
