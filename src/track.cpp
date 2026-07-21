#include "track.h"
#include <QFileInfo>
#include <QJsonArray>

QJsonObject Track::toJson() const
{
    QJsonObject obj;
    obj["filePath"] = filePath;
    obj["title"] = title;
    obj["artist"] = artist;
    obj["album"] = album;
    obj["duration"] = duration;
    obj["lyricsOffset"] = lyricsOffset;
    QJsonArray tagsArr;
    for (const auto &tag : tags)
        tagsArr.append(tag);
    obj["tags"] = tagsArr;
    return obj;
}

Track Track::fromJson(const QJsonObject &obj)
{
    Track t;
    t.filePath = obj["filePath"].toString();
    t.title = obj["title"].toString();
    t.artist = obj["artist"].toString();
    t.album = obj["album"].toString();
    t.duration = static_cast<qint64>(obj["duration"].toDouble());
    t.lyricsOffset = obj["lyricsOffset"].toInt();
    QJsonArray tagsArr = obj["tags"].toArray();
    for (const auto &v : tagsArr)
        t.tags.append(v.toString());
    return t;
}

QString Track::titleFromPath(const QString &path)
{
    QFileInfo fi(path);
    return fi.completeBaseName();
}
