#ifndef PLAYLISTMANAGER_H
#define PLAYLISTMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include "track.h"

// One named playlist (user-created or auto-generated).
struct PlaylistEntry {
    QString name;
    QString type;          // "user", "artist", "album", "tag"
    QStringList trackPaths; // references to tracks by filePath
    bool pinned = false;
};

// Manages multiple named playlists with JSON persistence.
// Owns playlist CRUD, auto-generation by artist/album/tag,
// and add/remove tracks from playlists.
class PlaylistManager : public QObject
{
    Q_OBJECT

public:
    explicit PlaylistManager(QObject *parent = nullptr);

    // --- Playlist CRUD ---
    QStringList playlistNames() const;
    QStringList userPlaylistNames() const;
    bool createPlaylist(const QString &name, const QString &type = "user");
    bool deletePlaylist(const QString &name);
    bool renamePlaylist(const QString &oldName, const QString &newName);
    bool playlistExists(const QString &name) const;

    // --- Track management within a playlist ---
    bool addTrack(const QString &playlistName, const QString &filePath);
    bool removeTrack(const QString &playlistName, const QString &filePath);
    QStringList tracksInPlaylist(const QString &name) const;
    int trackCount(const QString &name) const;

    // --- Auto-generate playlists from track metadata ---
    void generateByArtist(const QList<Track> &allTracks);
    void generateByAlbum(const QList<Track> &allTracks);
    void generateByTag(const QList<Track> &allTracks);

    // --- Persistence ---
    void load();
    void save() const;
    QString filePath() const;

    // --- Pin / Unpin ---
    bool isPinned(const QString &name) const;
    void togglePin(const QString &name);

    // --- Cleanup ---
    void removeMissingFiles();  // remove tracks that no longer exist on disk

signals:
    void playlistsChanged();

private:
    QList<PlaylistEntry> m_playlists;
    QString m_dataDir;

    int indexOf(const QString &name) const;
};

#endif // PLAYLISTMANAGER_H
