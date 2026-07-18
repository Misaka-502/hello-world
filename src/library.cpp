#include "library.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

Library::Library(QObject *parent)
    : QObject(parent)
{
    m_dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(m_dataDir);
}

QString Library::libraryFilePath() const
{
    return m_dataDir + "/library.json";
}

QList<Track> Library::loadLibrary()
{
    QList<Track> tracks;

    QFile file(libraryFilePath());
    if (!file.open(QIODevice::ReadOnly))
        return tracks;

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray())
        return tracks;

    QJsonArray arr = doc.array();
    for (const auto &val : arr) {
        if (val.isObject()) {
            Track t = Track::fromJson(val.toObject());
            // Only keep tracks whose files still exist
            if (QFile::exists(t.filePath))
                tracks.append(t);
        }
    }

    return tracks;
}

void Library::saveLibrary(const QList<Track> &tracks)
{
    QJsonArray arr;
    for (const auto &t : tracks) {
        arr.append(t.toJson());
    }

    QJsonDocument doc(arr);

    QFile file(libraryFilePath());
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
    }
}

void Library::updateTrack(const Track &track)
{
    QList<Track> tracks = loadLibrary();
    for (auto &t : tracks) {
        if (t.filePath == track.filePath) {
            t = track;
            break;
        }
    }
    saveLibrary(tracks);
}
