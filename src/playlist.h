#ifndef PLAYLIST_H
#define PLAYLIST_H

#include <QObject>
#include <QList>
#include "track.h"
#include "playmodestrategy.h"

// Manages a collection of tracks: add, remove, navigation, filtering, play mode
class Playlist : public QObject
{
    Q_OBJECT

public:
    explicit Playlist(QObject *parent = nullptr);

    // Track management
    void addTrack(const Track &track);
    void addTracks(const QList<Track> &tracks);
    void removeTrack(int index);
    void clear();
    int count() const { return m_tracks.size(); }
    bool isEmpty() const { return m_tracks.isEmpty(); }

    // Access
    const Track &trackAt(int index) const;
    Track &trackAt(int index);
    QList<Track> allTracks() const { return m_tracks; }
    QList<Track> filteredTracks() const;
    int currentIndex() const { return m_currentIndex; }
    void setCurrentIndex(int index);

    // Navigation (respects play mode)
    int nextIndex() const;
    int previousIndex() const;

    // Play mode (polymorphism via strategy pattern)
    void setPlayMode(PlayModeStrategy *mode);
    PlayModeStrategy *playMode() const { return m_playMode; }
    QString playModeName() const;
    QString playModeIcon() const;
    void cyclePlayMode();

    // Search/filter
    void setSearchFilter(const QString &keyword);
    QString searchFilter() const { return m_searchFilter; }

    // Persistence helpers
    QList<Track> &tracksRef() { return m_tracks; }
    void setTracks(const QList<Track> &tracks);

signals:
    void playlistChanged();
    void currentIndexChanged(int index);
    void playModeChanged();

private:
    QList<Track> m_tracks;
    int m_currentIndex = -1;
    QString m_searchFilter;

    // Owned strategies (one of three)
    ListLoopStrategy *m_listLoop;
    SingleLoopStrategy *m_singleLoop;
    RandomStrategy *m_random;
    PlayModeStrategy *m_playMode;  // points to one of the above
};

#endif // PLAYLIST_H
