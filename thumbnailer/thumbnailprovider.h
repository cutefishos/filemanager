#pragma once

#include <QQuickAsyncImageProvider>

class ThumbnailProvider : public QQuickAsyncImageProvider
{
public:
    QQuickImageResponse *requestImageResponse(const QString &id,
                                             const QSize &requestedSize) override;
};
