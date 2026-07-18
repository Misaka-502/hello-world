#ifndef TRACK_H
#define TRACK_H

#include <QString>
#include <QJsonObject>

// Represents a single audio track with metadata and lyrics offset
struct Track {
    QString filePath;
    QString title;
    QString artist;
    QString album;
    qint64 duration = 0;       // milliseconds
    int lyricsOffset = 0;      // milliseconds, persisted per-song for lyrics sync correction

    // Serialize to/from JSON for persistence
    QJsonObject toJson() const;
    static Track fromJson(const QJsonObject &obj);

    // Extract a default title from the filename (without extension)
    static QString titleFromPath(const QString &path);
};

#endif // TRACK_H
