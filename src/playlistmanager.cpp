#include "playlistmanager.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

// ============================================================
// Constructor
// ============================================================

PlaylistManager::PlaylistManager(QObject *parent)
    : QObject(parent)
{
    m_dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(m_dataDir);
    load();
}

// ============================================================
// Helpers
// ============================================================

QString PlaylistManager::filePath() const
{
    return m_dataDir + "/playlists.json";
}

int PlaylistManager::indexOf(const QString &name) const
{
    for (int i = 0; i < m_playlists.size(); ++i) {
        if (m_playlists[i].name == name)
            return i;
    }
    return -1;
}

// ============================================================
// Playlist CRUD
// ============================================================

QStringList PlaylistManager::playlistNames() const
{
    // Pinned playlists first
    QStringList pinned, unpinned;
    for (const auto &p : m_playlists) {
        if (p.pinned)
            pinned.append(p.name);
        else
            unpinned.append(p.name);
    }
    return pinned + unpinned;
}

QStringList PlaylistManager::userPlaylistNames() const
{
    QStringList pinned, unpinned;
    for (const auto &p : m_playlists) {
        if (p.type != "user")
            continue;
        if (p.pinned)
            pinned.append(p.name);
        else
            unpinned.append(p.name);
    }
    return pinned + unpinned;
}

bool PlaylistManager::playlistExists(const QString &name) const
{
    return indexOf(name) >= 0;
}

bool PlaylistManager::createPlaylist(const QString &name, const QString &type)
{
    if (name.isEmpty() || playlistExists(name))
        return false;

    PlaylistEntry entry;
    entry.name = name;
    entry.type = type;
    m_playlists.append(entry);
    save();
    emit playlistsChanged();
    return true;
}

bool PlaylistManager::deletePlaylist(const QString &name)
{
    int idx = indexOf(name);
    if (idx < 0)
        return false;
    // Only user and auto-generated playlists can be deleted
    m_playlists.removeAt(idx);
    save();
    emit playlistsChanged();
    return true;
}

bool PlaylistManager::renamePlaylist(const QString &oldName, const QString &newName)
{
    if (newName.isEmpty() || playlistExists(newName))
        return false;
    int idx = indexOf(oldName);
    if (idx < 0)
        return false;
    m_playlists[idx].name = newName;
    save();
    emit playlistsChanged();
    return true;
}

// ============================================================
// Track management
// ============================================================

bool PlaylistManager::addTrack(const QString &playlistName, const QString &filePath)
{
    int idx = indexOf(playlistName);
    if (idx < 0)
        return false;
    if (m_playlists[idx].trackPaths.contains(filePath))
        return false;  // already in playlist
    m_playlists[idx].trackPaths.append(filePath);
    save();
    emit playlistsChanged();
    return true;
}

bool PlaylistManager::removeTrack(const QString &playlistName, const QString &filePath)
{
    int idx = indexOf(playlistName);
    if (idx < 0)
        return false;
    m_playlists[idx].trackPaths.removeAll(filePath);
    save();
    emit playlistsChanged();
    return true;
}

QStringList PlaylistManager::tracksInPlaylist(const QString &name) const
{
    int idx = indexOf(name);
    if (idx < 0)
        return {};
    return m_playlists[idx].trackPaths;
}

int PlaylistManager::trackCount(const QString &name) const
{
    int idx = indexOf(name);
    if (idx < 0)
        return 0;
    return m_playlists[idx].trackPaths.size();
}

// ============================================================
// Auto-generate playlists
// ============================================================

void PlaylistManager::generateByArtist(const QList<Track> &allTracks)
{
    // Remove existing artist playlists
    m_playlists.erase(
        std::remove_if(m_playlists.begin(), m_playlists.end(),
                       [](const PlaylistEntry &e) { return e.type == "artist"; }),
        m_playlists.end());

    // Group by artist
    QMap<QString, QStringList> groups;
    for (const auto &t : allTracks) {
        QString key = t.artist.isEmpty() ? "Unknown Artist" : t.artist;
        groups[key].append(t.filePath);
    }

    for (auto it = groups.begin(); it != groups.end(); ++it) {
        PlaylistEntry entry;
        entry.name = it.key();
        entry.type = "artist";
        entry.trackPaths = it.value();
        m_playlists.append(entry);
    }

    save();
    emit playlistsChanged();
}

void PlaylistManager::generateByAlbum(const QList<Track> &allTracks)
{
    // Remove existing album playlists
    m_playlists.erase(
        std::remove_if(m_playlists.begin(), m_playlists.end(),
                       [](const PlaylistEntry &e) { return e.type == "album"; }),
        m_playlists.end());

    // Group by album
    QMap<QString, QStringList> groups;
    for (const auto &t : allTracks) {
        QString key = t.album.isEmpty() ? "Unknown Album" : t.album;
        groups[key].append(t.filePath);
    }

    for (auto it = groups.begin(); it != groups.end(); ++it) {
        PlaylistEntry entry;
        entry.name = it.key();
        entry.type = "album";
        entry.trackPaths = it.value();
        m_playlists.append(entry);
    }

    save();
    emit playlistsChanged();
}

void PlaylistManager::generateByTag(const QList<Track> &allTracks)
{
    // Remove existing tag playlists
    m_playlists.erase(
        std::remove_if(m_playlists.begin(), m_playlists.end(),
                       [](const PlaylistEntry &e) { return e.type == "tag"; }),
        m_playlists.end());

    // Group by each tag (one track can appear in multiple tag playlists)
    QMap<QString, QStringList> groups;
    for (const auto &t : allTracks) {
        for (const auto &tag : t.tags) {
            QString key = tag.trimmed();
            if (!key.isEmpty())
                groups[key].append(t.filePath);
        }
    }

    for (auto it = groups.begin(); it != groups.end(); ++it) {
        PlaylistEntry entry;
        entry.name = it.key();
        entry.type = "tag";
        entry.trackPaths = it.value();
        m_playlists.append(entry);
    }

    save();
    emit playlistsChanged();
}

// ============================================================
// Persistence
// ============================================================

void PlaylistManager::load()
{
    m_playlists.clear();

    QFile file(filePath());
    if (!file.open(QIODevice::ReadOnly))
        return;

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray())
        return;

    QJsonArray arr = doc.array();
    for (const auto &val : arr) {
        if (!val.isObject())
            continue;
        QJsonObject obj = val.toObject();
        PlaylistEntry entry;
        entry.name = obj["name"].toString();
        entry.type = obj["type"].toString();
        QJsonArray paths = obj["tracks"].toArray();
        for (const auto &p : paths)
            entry.trackPaths.append(p.toString());
        entry.pinned = obj["pinned"].toBool(false);
        if (!entry.name.isEmpty())
            m_playlists.append(entry);
    }
}

void PlaylistManager::save() const
{
    QJsonArray arr;
    for (const auto &entry : m_playlists) {
        QJsonObject obj;
        obj["name"] = entry.name;
        obj["type"] = entry.type;
        obj["pinned"] = entry.pinned;
        QJsonArray paths;
        for (const auto &p : entry.trackPaths)
            paths.append(p);
        obj["tracks"] = paths;
        arr.append(obj);
    }

    QJsonDocument doc(arr);
    QFile file(filePath());
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
    }
}

bool PlaylistManager::isPinned(const QString &name) const
{
    int idx = indexOf(name);
    return idx >= 0 && m_playlists[idx].pinned;
}

void PlaylistManager::togglePin(const QString &name)
{
    int idx = indexOf(name);
    if (idx < 0)
        return;
    m_playlists[idx].pinned = !m_playlists[idx].pinned;
    save();
    emit playlistsChanged();
}

void PlaylistManager::removeMissingFiles()
{
    bool changed = false;
    for (auto &entry : m_playlists) {
        QStringList valid;
        for (const auto &p : entry.trackPaths) {
            if (QFile::exists(p))
                valid.append(p);
            else
                changed = true;
        }
        entry.trackPaths = valid;
    }
    if (changed) {
        save();
        emit playlistsChanged();
    }
}
