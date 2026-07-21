#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMediaPlayer>
#include "track.h"

class QComboBox;
class QHBoxLayout;
class QListWidget;
class QLabel;
class QMenu;
class QPushButton;
class QSlider;
class QLineEdit;
class QWidget;
class QTimer;

class Player;
class Playlist;
class PlaylistManager;
class Library;
class LrcParser;
class LyricsWidget;
class SpectrumWidget;
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

    // Karaoke toggle
    void onToggleKaraoke();

    // Theme toggle
    void onToggleTheme();

    // Playlist management
    void onPlaylistComboChanged(int index);
    void onNewPlaylist();
    void onAutoGenerate();
    void onPlaylistContextMenu(const QPoint &pos);
    void onEditTags();
    void onAddToPlaylist();
    void onQuickAddToPlaylist();
    void onPlaylistComboContextMenu(const QPoint &pos);

private:
    // UI setup
    void setupUI();
    void setupLeftPanel(QWidget *parent, QHBoxLayout *mainLayout);
    void setupRightPanel(QWidget *parent, QHBoxLayout *mainLayout);
    QWidget *setupControlBar();
    void setupToolBar();
    void applyTheme();                     // apply current theme
    QString lightStylesheet() const;       // light theme QSS
    QString darkStylesheet() const;        // dark theme QSS

    // Helpers
    void loadTrack(int playlistIndex);
    void loadLyricsForCurrentTrack();
    void refreshPlaylistWidget();
    void updateTimeLabel();
    void updateTrackInfoDisplay();
    QString formatTime(qint64 ms) const;
    void scanDirectory(const QString &dirPath, QList<Track> &outTracks);
    void applyPlaylistFilter();  // load selected playlist into playing queue

    // --- Core components ---
    Player *m_player;
    Playlist *m_playlist;
    PlaylistManager *m_playlistManager;
    Library *m_library;
    LrcParser *m_lrcParser;
    Transcoder *m_transcoder;

    // --- UI: Left panel ---
    QComboBox *m_playlistCombo;
    QPushButton *m_newPlaylistBtn;
    QPushButton *m_autoGenBtn;
    QListWidget *m_playlistWidget;
    QLabel *m_playlistTitleLabel;

    // --- UI: Right panel (track info + lyrics) ---
    QWidget *m_rightPanel;
    QLabel *m_coverPlaceholder;
    QLabel *m_titleLabel;
    QLabel *m_artistLabel;
    QLabel *m_albumLabel;
    LyricsWidget *m_lyricsWidget;
    SpectrumWidget *m_spectrumWidget;
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
    QAction *m_karaokeAction = nullptr;
    QAction *m_themeAction = nullptr;

    // --- State ---
    bool m_sliderDragging = false;
    bool m_darkTheme = false;
    qint64 m_currentDuration = 0;
    QString m_contextTrackPath;  // for right-click context menu
};

#endif // MAINWINDOW_H
