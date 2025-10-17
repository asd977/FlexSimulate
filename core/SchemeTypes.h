#pragma once

#include <QDateTime>
#include <QVector>
#include <QString>

namespace FlexSimulate
{
struct ModelRecord
{
    QString id;
    QString name;
    QString directory;
    QString jsonPath;
    QString batPath;
    QString remarks;
    QString fingerprint;
};

struct SchemeLibraryEntry
{
    QString id;
    QString name;
    QString directory;
    QString thumbnailPath;
    bool deletable = false;
};

struct SchemeRecord
{
    QString id;
    QString name;
    QString libraryId;
    QString workingDirectory;
    QString thumbnailPath;
    QString remarks;
    QVector<ModelRecord> models;
};

struct ProjectMetadata
{
    QString remarks;
    QDateTime createdAt;
    QDateTime updatedAt;
};
}
