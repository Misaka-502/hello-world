#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMediaPlayer>
#include "track.h"

class QHBoxLayout;
class QListWidget;
class QLabel;
class QPushButton;
class QSlider;
class QLineEdit;
class QWidget;
class QTimer;

class Player;
class Playlist;
class Library;
class LrcParser;
class LyricsWidget;
class Transcoder;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    // Playlist
    void onPlaylistDoubleClicked(const QModelIndex &index);
    void onPlaylistChanged();

    // Playback control
    void onPlayPause();
    void onStop();
    void onPrev();
    void onNext();
    void onProgressSliderPressed();
    void onProgressSliderReleased();
    void onProgressSliderMoved(int value);
    void onVolumeChanged(int value);

    // Player signals
    void onPlayerPositionChanged(qint64 positionMs);
    void onPlayerDurationChanged(qint64 durationMs);
    void onPlayerStateChanged(QMediaPlayer::PlaybackState state);
    void onMediaEnded();
    void onPlayerMetaDataChanged();
    void onPlayerError(const QString &error);

    // Play mode
    void onModeButton();
    void onPlayModeChanged();

    // Search
    void onSearchTextChanged(const QString &text);

    // Lyrics
    void onLyricsJumpRequested(qint64 timeMs);
    void onLyricsBrowsingChanged(bool isBrowsing);
    void onLyricsOffsetPlus();
    void onLyricsOffsetMinus();

    // Scanning
    void onScanFolder();
    void onAddFolder();

    // Transcoding
    void onTranscode();
    void onTranscodeProgress(int percent);
    void onTranscodeFinished(bool success, const QString &outputFile);

private:
    // UI setup
    void setupUI();
    void setupLeftPanel(QWidget *parent, QHBoxLayout *mainLayout);
    void setupRightPanel(QWidget *parent, QHBoxLayout *mainLayout);
    QWidget *setupControlBar();
    void setupToolBar();
    void applyStylesheet();

    // Helpers
    void loadTrack(int playlistIndex);
    void loadLyricsForCurrentTrack();
    void refreshPlaylistWidget();
    void updateTimeLabel();
    void updateTrackInfoDisplay();
    QString formatTime(qint64 ms) const;
    void scanDirectory(const QString &dirPath, QList<Track> &outTracks);

    // --- Core components ---
    Player *m_player;
    Playlist *m_playlist;
    Library *m_library;
    LrcParser *m_lrcParser;
    Transcoder *m_transcoder;

    // --- UI: Left panel ---
    QListWidget *m_playlistWidget;
    QLabel *m_playlistTitleLabel;

    // --- UI: Right panel (track info + lyrics) ---
    QWidget *m_rightPanel;
    QLabel *m_coverPlaceholder;
    QLabel *m_titleLabel;
    QLabel *m_artistLabel;
    QLabel *m_albumLabel;
    LyricsWidget *m_lyricsWidget;
    QPushButton *m_lyricsOffsetPlusBtn;
    QPushButton *m_lyricsOffsetMinusBtn;
    QLabel *m_lyricsOffsetLabel;

    // --- UI: Control bar ---
    QPushButton *m_prevButton;
    QPushButton *m_playButton;
    QPushButton *m_stopButton;
    QPushButton *m_nextButton;
    QPushButton *m_modeButton;
    QSlider *m_progressSlider;
    QSlider *m_volumeSlider;
    QLabel *m_timeLabel;
    QLabel *m_modeLabel;

    // --- UI: Toolbar ---
    QLineEdit *m_searchEdit;

    // --- State ---
    bool m_sliderDragging = false;
    qint64 m_currentDuration = 0;
};

#endif // MAINWINDOW_H
