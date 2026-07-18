#ifndef LIBRARY_H
#define LIBRARY_H

#include <QObject>
#include <QString>
#include <QList>
#include "track.h"

// Persistence layer: saves/loads the track library as JSON
// Uses QStandardPaths::AppDataLocation for storage path
class Library : public QObject
{
    Q_OBJECT

public:
    explicit Library(QObject *parent = nullptr);

    // Load tracks from the persistent JSON file. Returns empty list if none exists.
    QList<Track> loadLibrary();

    // Save tracks to the persistent JSON file.
    void saveLibrary(const QList<Track> &tracks);

    // Get the path to the library JSON file
    QString libraryFilePath() const;

    // Update a single track's metadata (e.g., after reading tags)
    void updateTrack(const Track &track);

private:
    QString m_dataDir;
};

#endif // LIBRARY_H
