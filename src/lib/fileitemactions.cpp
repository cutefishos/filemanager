#include "fileitemactions.h"
#include <KMimeTypeTrader>

FileItemActions::FileItemActions(QObject *parent)
    : QObject(parent)
{

}

KService::List FileItemActions::associatedApplications(const QStringList &mimeTypeList, const QString &traderConstraint)
{
    const KService::List firstOffers = KMimeTypeTrader::self()->query(mimeTypeList.first(), "Application", traderConstraint);
    QStringList serviceList;

    for (int i = 0; i < firstOffers.count(); ++i) {

    }

    return KService::List();
}
