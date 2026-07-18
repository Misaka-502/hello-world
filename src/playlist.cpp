#include "playlist.h"
#include <algorithm>

Playlist::Playlist(QObject *parent)
    : QObject(parent)
    , m_listLoop(new ListLoopStrategy)
    , m_singleLoop(new SingleLoopStrategy)
    , m_random(new RandomStrategy)
    , m_playMode(m_listLoop)
{
}

void Playlist::addTrack(const Track &track)
{
    m_tracks.append(track);
    if (m_currentIndex < 0 && !m_tracks.isEmpty())
        m_currentIndex = 0;
    emit playlistChanged();
}

void Playlist::addTracks(const QList<Track> &tracks)
{
    m_tracks.append(tracks);
    if (m_currentIndex < 0 && !m_tracks.isEmpty())
        m_currentIndex = 0;
    emit playlistChanged();
}

void Playlist::removeTrack(int index)
{
    if (index < 0 || index >= m_tracks.size())
        return;
    m_tracks.removeAt(index);
    if (m_tracks.isEmpty()) {
        m_currentIndex = -1;
    } else if (m_currentIndex >= m_tracks.size()) {
        m_currentIndex = m_tracks.size() - 1;
    } else if (index < m_currentIndex) {
        m_currentIndex--;
    }
    emit playlistChanged();
}

void Playlist::clear()
{
    m_tracks.clear();
    m_currentIndex = -1;
    emit playlistChanged();
}

const Track &Playlist::trackAt(int index) const
{
    return m_tracks.at(index);
}

Track &Playlist::trackAt(int index)
{
    return m_tracks[index];
}

QList<Track> Playlist::filteredTracks() const
{
    if (m_searchFilter.isEmpty())
        return m_tracks;

    QList<Track> result;
    QString kw = m_searchFilter.toLower();
    for (const auto &t : m_tracks) {
        if (t.title.toLower().contains(kw) ||
            t.artist.toLower().contains(kw) ||
            t.album.toLower().contains(kw))
            result.append(t);
    }
    return result;
}

void Playlist::setCurrentIndex(int index)
{
    if (index >= 0 && index < m_tracks.size() && index != m_currentIndex) {
        m_currentIndex = index;
        emit currentIndexChanged(index);
    }
}

int Playlist::nextIndex() const
{
    if (m_tracks.isEmpty())
        return -1;
    if (m_currentIndex < 0)
        return 0;
    return m_playMode->nextIndex(m_currentIndex, m_tracks.size());
}

int Playlist::previousIndex() const
{
    if (m_tracks.isEmpty() || m_currentIndex < 0)
        return -1;
    if (m_tracks.size() == 1)
        return 0;
    return (m_currentIndex - 1 + m_tracks.size()) % m_tracks.size();
}

void Playlist::setPlayMode(PlayModeStrategy *mode)
{
    if (m_playMode != mode) {
        m_playMode = mode;
        emit playModeChanged();
    }
}

QString Playlist::playModeName() const
{
    return m_playMode ? m_playMode->name() : QString();
}

QString Playlist::playModeIcon() const
{
    return m_playMode ? m_playMode->icon() : QString();
}

void Playlist::cyclePlayMode()
{
    if (m_playMode == m_listLoop)
        setPlayMode(m_singleLoop);
    else if (m_playMode == m_singleLoop)
        setPlayMode(m_random);
    else
        setPlayMode(m_listLoop);
}

void Playlist::setSearchFilter(const QString &keyword)
{
    if (m_searchFilter != keyword) {
        m_searchFilter = keyword;
        emit playlistChanged();
    }
}

void Playlist::setTracks(const QList<Track> &tracks)
{
    m_tracks = tracks;
    if (m_currentIndex >= m_tracks.size())
        m_currentIndex = m_tracks.isEmpty() ? -1 : 0;
    if (m_currentIndex < 0 && !m_tracks.isEmpty())
        m_currentIndex = 0;
    emit playlistChanged();
}
