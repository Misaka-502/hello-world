#include "player.h"

Player::Player(QObject *parent)
    : QObject(parent)
    , m_mediaPlayer(new QMediaPlayer(this))
    , m_audioOutput(new QAudioOutput(this))
{
    m_mediaPlayer->setAudioOutput(m_audioOutput);
    m_audioOutput->setVolume(0.8f);

    connect(m_mediaPlayer, &QMediaPlayer::positionChanged,
            this, &Player::onPositionChanged);
    connect(m_mediaPlayer, &QMediaPlayer::durationChanged,
            this, &Player::onDurationChanged);
    connect(m_mediaPlayer, &QMediaPlayer::mediaStatusChanged,
            this, &Player::onMediaStatusChanged);
    connect(m_mediaPlayer, &QMediaPlayer::errorOccurred,
            this, &Player::onErrorOccurred);
    connect(m_mediaPlayer, &QMediaPlayer::metaDataChanged,
            this, &Player::metaDataChanged);
    connect(m_mediaPlayer, &QMediaPlayer::playbackStateChanged,
            this, &Player::stateChanged);
}

Player::~Player() = default;

void Player::play()
{
    m_mediaPlayer->play();
}

void Player::pause()
{
    m_mediaPlayer->pause();
}

void Player::stop()
{
    m_mediaPlayer->stop();
}

void Player::togglePlayPause()
{
    if (m_mediaPlayer->playbackState() == QMediaPlayer::PlayingState)
        pause();
    else
        play();
}

void Player::loadAndPlay(const QString &filePath)
{
    m_mediaPlayer->stop();
    m_currentFile = filePath;
    m_mediaPlayer->setSource(QUrl::fromLocalFile(filePath));
    m_mediaPlayer->play();
}

void Player::seekTo(qint64 positionMs)
{
    m_mediaPlayer->setPosition(positionMs);
}

void Player::setVolume(float volume)
{
    m_audioOutput->setVolume(volume);
}

float Player::volume() const
{
    return m_audioOutput->volume();
}

bool Player::isPlaying() const
{
    return m_mediaPlayer->playbackState() == QMediaPlayer::PlayingState;
}

qint64 Player::position() const
{
    return m_mediaPlayer->position();
}

qint64 Player::duration() const
{
    return m_mediaPlayer->duration();
}

void Player::onPositionChanged(qint64 position)
{
    emit positionChanged(position);
}

void Player::onDurationChanged(qint64 duration)
{
    emit durationChanged(duration);
}

void Player::onMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
    if (status == QMediaPlayer::EndOfMedia)
        emit mediaEnded();
}

void Player::onErrorOccurred(QMediaPlayer::Error error, const QString &errorString)
{
    Q_UNUSED(error)
    emit errorOccurred(errorString);
}
