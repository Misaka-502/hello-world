#ifndef PLAYER_H
#define PLAYER_H

#include <QObject>
#include <QMediaPlayer>
#include <QAudioOutput>

// Wraps QMediaPlayer with a clean interface.
// The rest of the application should go through Player, not QMediaPlayer directly.
class Player : public QObject
{
    Q_OBJECT

public:
    explicit Player(QObject *parent = nullptr);
    ~Player() override;

    // Playback control
    void play();
    void pause();
    void stop();
    void togglePlayPause();

    // Load and play a file
    void loadAndPlay(const QString &filePath);

    // Seek to position (milliseconds)
    void seekTo(qint64 positionMs);

    // Volume (0.0 - 1.0)
    void setVolume(float volume);
    float volume() const;

    // State queries
    bool isPlaying() const;
    qint64 position() const;   // milliseconds
    qint64 duration() const;   // milliseconds
    QString currentFile() const { return m_currentFile; }

    // Access to underlying player (read-only, for metadata)
    QMediaPlayer *mediaPlayer() const { return m_mediaPlayer; }

signals:
    void positionChanged(qint64 positionMs);
    void durationChanged(qint64 durationMs);
    void stateChanged(QMediaPlayer::PlaybackState state);
    void mediaEnded();
    void metaDataChanged();
    void errorOccurred(const QString &error);

private slots:
    void onPositionChanged(qint64 position);
    void onDurationChanged(qint64 duration);
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);
    void onErrorOccurred(QMediaPlayer::Error error, const QString &errorString);

private:
    QMediaPlayer *m_mediaPlayer;
    QAudioOutput *m_audioOutput;
    QString m_currentFile;
};

#endif // PLAYER_H
