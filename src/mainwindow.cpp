#include "mainwindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include <QSlider>
#include <QLineEdit>
#include <QToolBar>
#include <QStatusBar>
#include <QAction>
#include <QFileDialog>
#include <QDirIterator>
#include <QFileInfo>
#include <QMessageBox>
#include <QMediaPlayer>
#include <QMediaMetaData>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

#include "player.h"
#include "playlist.h"
#include "library.h"
#include "lrcparser.h"
#include "lyricswidget.h"
#include "transcoder.h"

// ============================================================
// Constructor / Destructor
// ============================================================

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_transcoder(nullptr)
{
    // Create core components
    m_player = new Player(this);
    m_playlist = new Playlist(this);
    m_library = new Library(this);
    m_lrcParser = new LrcParser();

    setupUI();
    applyStylesheet();

    // Load persisted library
    QList<Track> savedTracks = m_library->loadLibrary();
    if (!savedTracks.isEmpty()) {
        m_playlist->setTracks(savedTracks);
        refreshPlaylistWidget();
        statusBar()->showMessage(
            QString("Loaded %1 tracks from library").arg(savedTracks.size()), 3000);
    }

    // --- Connect signals ---

    // Playlist signals
    connect(m_playlist, &Playlist::playlistChanged,
            this, &MainWindow::onPlaylistChanged);
    connect(m_playlist, &Playlist::playModeChanged,
            this, &MainWindow::onPlayModeChanged);

    // Player signals
    connect(m_player, &Player::positionChanged,
            this, &MainWindow::onPlayerPositionChanged);
    connect(m_player, &Player::durationChanged,
            this, &MainWindow::onPlayerDurationChanged);
    connect(m_player, &Player::stateChanged,
            this, &MainWindow::onPlayerStateChanged);
    connect(m_player, &Player::mediaEnded,
            this, &MainWindow::onMediaEnded);
    connect(m_player, &Player::metaDataChanged,
            this, &MainWindow::onPlayerMetaDataChanged);
    connect(m_player, &Player::errorOccurred,
            this, &MainWindow::onPlayerError);

    // Playlist widget
    connect(m_playlistWidget, &QListWidget::doubleClicked,
            this, &MainWindow::onPlaylistDoubleClicked);

    // Search
    connect(m_searchEdit, &QLineEdit::textChanged,
            this, &MainWindow::onSearchTextChanged);

    // Lyrics widget
    connect(m_lyricsWidget, &LyricsWidget::jumpRequested,
            this, &MainWindow::onLyricsJumpRequested);
    connect(m_lyricsWidget, &LyricsWidget::browsingStateChanged,
            this, &MainWindow::onLyricsBrowsingChanged);

    // Progress slider
    connect(m_progressSlider, &QSlider::sliderPressed,
            this, &MainWindow::onProgressSliderPressed);
    connect(m_progressSlider, &QSlider::sliderReleased,
            this, &MainWindow::onProgressSliderReleased);
    connect(m_progressSlider, &QSlider::valueChanged,
            this, &MainWindow::onProgressSliderMoved);

    // Volume
    connect(m_volumeSlider, &QSlider::valueChanged,
            this, &MainWindow::onVolumeChanged);

    // Initialize UI state
    onPlayModeChanged();
    m_playButton->setText(QString::fromUtf8("\xE2\x96\xB6")); // ▶
    m_timeLabel->setText("00:00 / 00:00");

    m_lyricsOffsetLabel->setText("Offset: 0.0s");
}

MainWindow::~MainWindow()
{
    // Save library on exit
    m_library->saveLibrary(m_playlist->allTracks());
    delete m_lrcParser;
}

// ============================================================
// Main UI Setup
// ============================================================

void MainWindow::setupUI()
{
    setWindowTitle("Music Player");
    resize(1100, 720);
    setMinimumSize(900, 600);

    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout *rootLayout = new QVBoxLayout(centralWidget);
    rootLayout->setSpacing(0);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    QHBoxLayout *mainLayout = new QHBoxLayout();
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(8, 8, 8, 8);

    setupLeftPanel(centralWidget, mainLayout);
    setupRightPanel(centralWidget, mainLayout);

    rootLayout->addLayout(mainLayout);

    QWidget *controlBar = setupControlBar();
    rootLayout->addWidget(controlBar);

    setupToolBar();
    statusBar()->showMessage("Ready");
}

void MainWindow::setupLeftPanel(QWidget *parent, QHBoxLayout *mainLayout)
{
    QGroupBox *playlistGroup = new QGroupBox("Playlist", parent);
    playlistGroup->setMinimumWidth(260);
    playlistGroup->setMaximumWidth(320);
    playlistGroup->setObjectName("playlistGroup");

    QVBoxLayout *layout = new QVBoxLayout(playlistGroup);
    layout->setContentsMargins(6, 18, 6, 6);
    layout->setSpacing(4);

    m_playlistWidget = new QListWidget(parent);
    m_playlistWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_playlistWidget->setObjectName("playlistList");
    layout->addWidget(m_playlistWidget);

    mainLayout->addWidget(playlistGroup);
}

void MainWindow::setupRightPanel(QWidget *parent, QHBoxLayout *mainLayout)
{
    m_rightPanel = new QWidget(parent);
    m_rightPanel->setObjectName("rightPanel");
    QVBoxLayout *layout = new QVBoxLayout(m_rightPanel);
    layout->setSpacing(8);
    layout->setContentsMargins(12, 12, 12, 12);

    // --- Track info header ---
    QWidget *infoHeader = new QWidget(parent);
    QHBoxLayout *infoLayout = new QHBoxLayout(infoHeader);
    infoLayout->setContentsMargins(0, 0, 0, 0);

    // Cover placeholder
    m_coverPlaceholder = new QLabel(parent);
    m_coverPlaceholder->setFixedSize(80, 80);
    m_coverPlaceholder->setAlignment(Qt::AlignCenter);
    m_coverPlaceholder->setObjectName("coverPlaceholder");
    m_coverPlaceholder->setText("CD");
    infoLayout->addWidget(m_coverPlaceholder);

    QWidget *infoText = new QWidget(parent);
    QVBoxLayout *textLayout = new QVBoxLayout(infoText);
    textLayout->setSpacing(2);
    textLayout->setContentsMargins(12, 0, 0, 0);

    m_titleLabel = new QLabel("No Track", parent);
    m_titleLabel->setObjectName("trackTitle");
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    textLayout->addWidget(m_titleLabel);

    m_artistLabel = new QLabel("--", parent);
    m_artistLabel->setObjectName("trackArtist");
    m_artistLabel->setFont(QFont("sans-serif", 12));
    textLayout->addWidget(m_artistLabel);

    m_albumLabel = new QLabel("--", parent);
    m_albumLabel->setObjectName("trackAlbum");
    m_albumLabel->setFont(QFont("sans-serif", 11));
    textLayout->addWidget(m_albumLabel);

    infoLayout->addWidget(infoText);
    infoLayout->addStretch();
    layout->addWidget(infoHeader);

    // --- Lyrics offset controls ---
    QWidget *offsetBar = new QWidget(parent);
    QHBoxLayout *offsetLayout = new QHBoxLayout(offsetBar);
    offsetLayout->setContentsMargins(0, 0, 0, 0);
    offsetLayout->setSpacing(6);

    QLabel *offsetLabelTitle = new QLabel("Lyrics:", parent);
    offsetLabelTitle->setObjectName("offsetTitle");
    offsetLayout->addWidget(offsetLabelTitle);

    m_lyricsOffsetMinusBtn = new QPushButton("-0.5s", parent);
    m_lyricsOffsetMinusBtn->setFixedSize(55, 28);
    m_lyricsOffsetMinusBtn->setObjectName("offsetBtn");
    connect(m_lyricsOffsetMinusBtn, &QPushButton::clicked,
            this, &MainWindow::onLyricsOffsetMinus);
    offsetLayout->addWidget(m_lyricsOffsetMinusBtn);

    m_lyricsOffsetLabel = new QLabel("0.0s", parent);
    m_lyricsOffsetLabel->setObjectName("offsetValue");
    m_lyricsOffsetLabel->setAlignment(Qt::AlignCenter);
    m_lyricsOffsetLabel->setMinimumWidth(60);
    offsetLayout->addWidget(m_lyricsOffsetLabel);

    m_lyricsOffsetPlusBtn = new QPushButton("+0.5s", parent);
    m_lyricsOffsetPlusBtn->setFixedSize(55, 28);
    m_lyricsOffsetPlusBtn->setObjectName("offsetBtn");
    connect(m_lyricsOffsetPlusBtn, &QPushButton::clicked,
            this, &MainWindow::onLyricsOffsetPlus);
    offsetLayout->addWidget(m_lyricsOffsetPlusBtn);

    offsetLayout->addStretch();
    layout->addWidget(offsetBar);

    // --- Lyrics widget ---
    m_lyricsWidget = new LyricsWidget(parent);
    m_lyricsWidget->setObjectName("lyricsArea");
    layout->addWidget(m_lyricsWidget, 1);

    mainLayout->addWidget(m_rightPanel, 2);
}

QWidget *MainWindow::setupControlBar()
{
    QWidget *controlBar = new QWidget(this);
    controlBar->setFixedHeight(64);
    controlBar->setObjectName("controlBar");

    QHBoxLayout *layout = new QHBoxLayout(controlBar);
    layout->setSpacing(6);
    layout->setContentsMargins(16, 8, 16, 8);

    m_prevButton = new QPushButton(QString::fromUtf8("\xE2\x8F\xAE"), this); // ⏮
    m_prevButton->setFixedSize(40, 40);
    m_prevButton->setObjectName("controlBtn");
    connect(m_prevButton, &QPushButton::clicked, this, &MainWindow::onPrev);
    layout->addWidget(m_prevButton);

    m_playButton = new QPushButton(QString::fromUtf8("\xE2\x96\xB6"), this); // ▶
    m_playButton->setFixedSize(48, 48);
    m_playButton->setObjectName("playButton");
    connect(m_playButton, &QPushButton::clicked, this, &MainWindow::onPlayPause);
    layout->addWidget(m_playButton);

    m_stopButton = new QPushButton(QString::fromUtf8("\xE2\x8F\xB9"), this); // ⏹
    m_stopButton->setFixedSize(40, 40);
    m_stopButton->setObjectName("controlBtn");
    connect(m_stopButton, &QPushButton::clicked, this, &MainWindow::onStop);
    layout->addWidget(m_stopButton);

    m_nextButton = new QPushButton(QString::fromUtf8("\xE2\x8F\xAD"), this); // ⏭
    m_nextButton->setFixedSize(40, 40);
    m_nextButton->setObjectName("controlBtn");
    connect(m_nextButton, &QPushButton::clicked, this, &MainWindow::onNext);
    layout->addWidget(m_nextButton);

    layout->addSpacing(10);

    m_progressSlider = new QSlider(Qt::Horizontal, this);
    m_progressSlider->setRange(0, 0);
    m_progressSlider->setObjectName("progressSlider");
    layout->addWidget(m_progressSlider);

    layout->addSpacing(6);

    m_timeLabel = new QLabel("00:00 / 00:00", this);
    m_timeLabel->setMinimumWidth(120);
    m_timeLabel->setAlignment(Qt::AlignCenter);
    m_timeLabel->setObjectName("timeLabel");
    layout->addWidget(m_timeLabel);

    layout->addSpacing(6);

    QLabel *volumeIcon = new QLabel(QString::fromUtf8("\xF0\x9F\x94\x8A"), this); // 🔊
    layout->addWidget(volumeIcon);

    m_volumeSlider = new QSlider(Qt::Horizontal, this);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(80);
    m_volumeSlider->setFixedWidth(80);
    m_volumeSlider->setObjectName("volumeSlider");
    layout->addWidget(m_volumeSlider);

    layout->addSpacing(8);

    m_modeButton = new QPushButton(this);
    m_modeButton->setFixedSize(40, 40);
    m_modeButton->setObjectName("modeBtn");
    connect(m_modeButton, &QPushButton::clicked, this, &MainWindow::onModeButton);
    layout->addWidget(m_modeButton);

    return controlBar;
}

void MainWindow::setupToolBar()
{
    QToolBar *toolbar = addToolBar("Toolbar");
    toolbar->setMovable(false);
    toolbar->setObjectName("mainToolBar");

    QAction *scanAction = toolbar->addAction(
        QString::fromUtf8("\xF0\x9F\x93\x81") + " Scan");
    connect(scanAction, &QAction::triggered, this, &MainWindow::onScanFolder);

    QAction *addAction = toolbar->addAction(
        QString::fromUtf8("\xE2\x9E\x95") + " Add");
    connect(addAction, &QAction::triggered, this, &MainWindow::onAddFolder);

    QAction *transcodeAction = toolbar->addAction(
        QString::fromUtf8("\xF0\x9F\x94\x84") + " Convert");
    connect(transcodeAction, &QAction::triggered, this, &MainWindow::onTranscode);

    toolbar->addSeparator();

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("Search tracks...");
    m_searchEdit->setFixedWidth(200);
    m_searchEdit->setObjectName("searchBox");
    toolbar->addWidget(m_searchEdit);

    QWidget *spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolbar->addWidget(spacer);

    m_modeLabel = new QLabel("List Loop", this);
    m_modeLabel->setObjectName("modeLabel");
    toolbar->addWidget(m_modeLabel);
}

// ============================================================
// Playlist slots
// ============================================================

void MainWindow::onPlaylistDoubleClicked(const QModelIndex &index)
{
    int row = index.row();
    QList<Track> filtered = m_playlist->filteredTracks();
    if (row < 0 || row >= filtered.size())
        return;

    // Find the track in the full list
    const Track &selected = filtered.at(row);
    QList<Track> all = m_playlist->allTracks();
    for (int i = 0; i < all.size(); ++i) {
        if (all[i].filePath == selected.filePath) {
            m_playlist->setCurrentIndex(i);
            loadTrack(i);
            return;
        }
    }
}

void MainWindow::onPlaylistChanged()
{
    refreshPlaylistWidget();
}

// ============================================================
// Playback control slots
// ============================================================

void MainWindow::onPlayPause()
{
    if (m_playlist->count() == 0)
        return;

    if (m_player->isPlaying()) {
        m_player->pause();
    } else if (m_player->currentFile().isEmpty()) {
        // Nothing loaded yet, play current
        int idx = m_playlist->currentIndex();
        if (idx < 0) idx = 0;
        m_playlist->setCurrentIndex(idx);
        loadTrack(idx);
    } else {
        m_player->play();
    }
}

void MainWindow::onStop()
{
    m_player->stop();
    m_playButton->setText(QString::fromUtf8("\xE2\x96\xB6"));
    m_progressSlider->setValue(0);
    m_timeLabel->setText("00:00 / " + formatTime(m_currentDuration));
}

void MainWindow::onPrev()
{
    int idx = m_playlist->previousIndex();
    if (idx >= 0) {
        m_playlist->setCurrentIndex(idx);
        loadTrack(idx);
    }
}

void MainWindow::onNext()
{
    int idx = m_playlist->nextIndex();
    if (idx >= 0) {
        m_playlist->setCurrentIndex(idx);
        loadTrack(idx);
    }
}

void MainWindow::onProgressSliderPressed()
{
    m_sliderDragging = true;
}

void MainWindow::onProgressSliderReleased()
{
    m_sliderDragging = false;
    qint64 pos = static_cast<qint64>(m_progressSlider->value());
    m_player->seekTo(pos);
}

void MainWindow::onProgressSliderMoved(int value)
{
    if (m_sliderDragging) {
        m_timeLabel->setText(
            formatTime(value) + " / " + formatTime(m_currentDuration));
    }
}

void MainWindow::onVolumeChanged(int value)
{
    m_player->setVolume(value / 100.0f);
}

// ============================================================
// Player signal slots
// ============================================================

void MainWindow::onPlayerPositionChanged(qint64 positionMs)
{
    if (!m_sliderDragging) {
        m_progressSlider->setValue(static_cast<int>(positionMs));
    }
    updateTimeLabel();

    // Update lyrics display — tied directly to progress updates
    int idx = m_playlist->currentIndex();
    if (idx >= 0 && idx < m_playlist->count()) {
        int offset = m_playlist->trackAt(idx).lyricsOffset;
        m_lyricsWidget->updatePlaybackPosition(positionMs, offset);
    }
}

void MainWindow::onPlayerDurationChanged(qint64 durationMs)
{
    m_currentDuration = durationMs;
    m_progressSlider->setRange(0, static_cast<int>(durationMs));
    // Update duration in track
    int idx = m_playlist->currentIndex();
    if (idx >= 0) {
        m_playlist->trackAt(idx).duration = durationMs;
    }
    updateTimeLabel();
}

void MainWindow::onPlayerStateChanged(QMediaPlayer::PlaybackState state)
{
    if (state == QMediaPlayer::PlayingState) {
        m_playButton->setText(QString::fromUtf8("\xE2\x8F\xB8")); // ⏸
    } else {
        m_playButton->setText(QString::fromUtf8("\xE2\x96\xB6")); // ▶
    }
}

void MainWindow::onMediaEnded()
{
    int idx = m_playlist->nextIndex();
    if (idx >= 0) {
        m_playlist->setCurrentIndex(idx);
        loadTrack(idx);
    }
}

void MainWindow::onPlayerMetaDataChanged()
{
    updateTrackInfoDisplay();
}

void MainWindow::onPlayerError(const QString &error)
{
    statusBar()->showMessage("Error: " + error, 5000);
}

// ============================================================
// Play mode slots
// ============================================================

void MainWindow::onModeButton()
{
    m_playlist->cyclePlayMode();
}

void MainWindow::onPlayModeChanged()
{
    m_modeButton->setText(m_playlist->playModeIcon());
    m_modeLabel->setText(m_playlist->playModeName());
    statusBar()->showMessage("Mode: " + m_playlist->playModeName(), 2000);
}

// ============================================================
// Search slots
// ============================================================

void MainWindow::onSearchTextChanged(const QString &text)
{
    m_playlist->setSearchFilter(text);
}

// ============================================================
// Lyrics slots
// ============================================================

void MainWindow::onLyricsJumpRequested(qint64 timeMs)
{
    m_player->seekTo(timeMs);
    statusBar()->showMessage("Jumped to " + formatTime(timeMs), 2000);
}

void MainWindow::onLyricsBrowsingChanged(bool isBrowsing)
{
    Q_UNUSED(isBrowsing)
    // Could show/hide a "browsing" indicator
}

void MainWindow::onLyricsOffsetPlus()
{
    int idx = m_playlist->currentIndex();
    if (idx < 0) return;
    // Add 500ms
    m_playlist->trackAt(idx).lyricsOffset += 500;
    m_library->saveLibrary(m_playlist->allTracks());
    int offsetMs = m_playlist->trackAt(idx).lyricsOffset;
    m_lyricsOffsetLabel->setText(
        QString("Offset: %1s").arg(offsetMs / 1000.0, 0, 'f', 1));
}

void MainWindow::onLyricsOffsetMinus()
{
    int idx = m_playlist->currentIndex();
    if (idx < 0) return;
    m_playlist->trackAt(idx).lyricsOffset -= 500;
    m_library->saveLibrary(m_playlist->allTracks());
    int offsetMs = m_playlist->trackAt(idx).lyricsOffset;
    m_lyricsOffsetLabel->setText(
        QString("Offset: %1s").arg(offsetMs / 1000.0, 0, 'f', 1));
}

// ============================================================
// Scan / Add folder slots
// ============================================================

void MainWindow::onScanFolder()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Select Music Folder");
    if (dir.isEmpty())
        return;

    m_playlist->clear();
    m_library->saveLibrary(QList<Track>());

    QList<Track> tracks;
    scanDirectory(dir, tracks);

    if (tracks.isEmpty()) {
        statusBar()->showMessage("No audio files found", 3000);
        return;
    }

    m_playlist->setTracks(tracks);
    m_library->saveLibrary(tracks);
    refreshPlaylistWidget();
    statusBar()->showMessage(
        QString("Scanned %1 tracks from folder").arg(tracks.size()), 3000);
}

void MainWindow::onAddFolder()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Add Music Folder");
    if (dir.isEmpty())
        return;

    QList<Track> newTracks;
    scanDirectory(dir, newTracks);

    if (newTracks.isEmpty()) {
        statusBar()->showMessage("No audio files found in added folder", 3000);
        return;
    }

    // Check for duplicates by file path
    QSet<QString> existingPaths;
    for (const auto &t : m_playlist->allTracks())
        existingPaths.insert(t.filePath);

    int added = 0;
    for (const auto &t : newTracks) {
        if (!existingPaths.contains(t.filePath)) {
            m_playlist->addTrack(t);
            existingPaths.insert(t.filePath);
            added++;
        }
    }

    m_library->saveLibrary(m_playlist->allTracks());
    refreshPlaylistWidget();
    statusBar()->showMessage(
        QString("Added %1 new tracks").arg(added), 3000);
}

// ============================================================
// Transcode slots
// ============================================================

void MainWindow::onTranscode()
{
    int idx = m_playlist->currentIndex();
    if (idx < 0) {
        QMessageBox::information(this, "Convert", "Select a track first.");
        return;
    }

    const Track &track = m_playlist->trackAt(idx);

    QString savePath = QFileDialog::getSaveFileName(
        this, "Save Converted File",
        QFileInfo(track.filePath).completeBaseName() + ".mp3",
        "MP3 (*.mp3);;FLAC (*.flac);;WAV (*.wav)");

    if (savePath.isEmpty())
        return;

    QString format = QFileInfo(savePath).suffix().toLower();

    if (!m_transcoder) {
        m_transcoder = new Transcoder(this);
        connect(m_transcoder, &Transcoder::progressChanged,
                this, &MainWindow::onTranscodeProgress);
        connect(m_transcoder, &Transcoder::finished,
                this, &MainWindow::onTranscodeFinished);
    }

    if (m_transcoder->isRunning()) {
        QMessageBox::information(this, "Convert", "A conversion is already in progress.");
        return;
    }

    bool ok = m_transcoder->start(track.filePath, savePath, format);
    if (!ok) {
        statusBar()->showMessage("Failed to start conversion. Is ffmpeg installed?", 5000);
    } else {
        statusBar()->showMessage("Converting...");
    }
}

void MainWindow::onTranscodeProgress(int percent)
{
    statusBar()->showMessage(QString("Converting... %1%").arg(percent));
}

void MainWindow::onTranscodeFinished(bool success, const QString &outputFile)
{
    if (success) {
        statusBar()->showMessage("Conversion complete: " + outputFile, 5000);
        QMessageBox::information(this, "Convert", "Conversion complete!\n" + outputFile);
    } else {
        statusBar()->showMessage("Conversion failed", 5000);
        QMessageBox::warning(this, "Convert", "Conversion failed.");
    }
}

// ============================================================
// Core helpers
// ============================================================

void MainWindow::loadTrack(int playlistIndex)
{
    if (playlistIndex < 0 || playlistIndex >= m_playlist->count())
        return;

    const Track &track = m_playlist->trackAt(playlistIndex);

    // Update lyrics offset display
    int offsetMs = track.lyricsOffset;
    m_lyricsOffsetLabel->setText(
        QString("Offset: %1s").arg(offsetMs / 1000.0, 0, 'f', 1));

    // Load and play
    m_player->loadAndPlay(track.filePath);

    // Load lyrics
    loadLyricsForCurrentTrack();

    // Update display
    updateTrackInfoDisplay();

    // Select in list
    QList<Track> filtered = m_playlist->filteredTracks();
    for (int i = 0; i < filtered.size(); ++i) {
        if (filtered[i].filePath == track.filePath) {
            m_playlistWidget->setCurrentRow(i);
            break;
        }
    }

    statusBar()->showMessage("Now playing: " + track.title);
}

void MainWindow::loadLyricsForCurrentTrack()
{
    int idx = m_playlist->currentIndex();
    if (idx < 0) {
        m_lyricsWidget->clearLyrics();
        return;
    }

    const Track &track = m_playlist->trackAt(idx);
    QFileInfo fi(track.filePath);
    QString lrcPath = fi.absolutePath() + "/" + fi.completeBaseName() + ".lrc";

    if (QFileInfo::exists(lrcPath)) {
        if (m_lrcParser->loadFromFile(lrcPath)) {
            m_lyricsWidget->setLyrics(*m_lrcParser);

            // Use [offset:] tag as initial offset if not already set
            if (track.lyricsOffset == 0 && m_lrcParser->fileOffset() != 0) {
                m_playlist->trackAt(idx).lyricsOffset = m_lrcParser->fileOffset();
                m_lyricsOffsetLabel->setText(
                    QString("Offset: %1s").arg(m_lrcParser->fileOffset() / 1000.0, 0, 'f', 1));
            }
            return;
        }
    }

    // No lyrics found
    m_lyricsWidget->clearLyrics();
}

void MainWindow::refreshPlaylistWidget()
{
    m_playlistWidget->clear();
    QList<Track> tracks = m_playlist->filteredTracks();
    for (const auto &t : tracks) {
        QString display = t.title;
        if (!t.artist.isEmpty())
            display += "  -  " + t.artist;
        m_playlistWidget->addItem(display);
    }

    int idx = m_playlist->currentIndex();
    if (idx >= 0) {
        // Find the current track in the filtered list
        const Track &curTrack = m_playlist->trackAt(idx);
        for (int i = 0; i < tracks.size(); ++i) {
            if (tracks[i].filePath == curTrack.filePath) {
                m_playlistWidget->setCurrentRow(i);
                break;
            }
        }
    }
}

void MainWindow::updateTimeLabel()
{
    qint64 pos = m_player->position();
    m_timeLabel->setText(formatTime(pos) + " / " + formatTime(m_currentDuration));
}

void MainWindow::updateTrackInfoDisplay()
{
    int idx = m_playlist->currentIndex();
    if (idx < 0)
        return;

    Track &track = m_playlist->trackAt(idx);

    // Read metadata from the media player
    QMediaMetaData meta = m_player->mediaPlayer()->metaData();

    QString title = meta.stringValue(QMediaMetaData::Title);
    if (!title.isEmpty()) {
        track.title = title;
        m_titleLabel->setText(title);
    } else if (track.title.isEmpty()) {
        track.title = Track::titleFromPath(track.filePath);
        m_titleLabel->setText(track.title);
    } else {
        m_titleLabel->setText(track.title);
    }

    QString artist = meta.stringValue(QMediaMetaData::ContributingArtist);
    if (artist.isEmpty())
        artist = meta.stringValue(QMediaMetaData::Author);
    if (!artist.isEmpty()) {
        track.artist = artist;
        m_artistLabel->setText(artist);
    } else if (!track.artist.isEmpty()) {
        m_artistLabel->setText(track.artist);
    } else {
        m_artistLabel->setText("Unknown Artist");
    }

    QString album = meta.stringValue(QMediaMetaData::AlbumTitle);
    if (!album.isEmpty()) {
        track.album = album;
        m_albumLabel->setText(album);
    } else if (!track.album.isEmpty()) {
        m_albumLabel->setText(track.album);
    } else {
        m_albumLabel->setText("Unknown Album");
    }

    m_currentDuration = m_player->duration();
    if (m_currentDuration > 0) {
        track.duration = m_currentDuration;
        m_progressSlider->setRange(0, static_cast<int>(m_currentDuration));
    }

    updateTimeLabel();
    refreshPlaylistWidget();

    // Persist updated metadata
    m_library->updateTrack(track);
}

QString MainWindow::formatTime(qint64 ms) const
{
    if (ms <= 0) return "00:00";
    int totalSecs = static_cast<int>(ms / 1000);
    int minutes = totalSecs / 60;
    int seconds = totalSecs % 60;
    return QString("%1:%2")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
}

void MainWindow::scanDirectory(const QString &dirPath, QList<Track> &outTracks)
{
    QStringList filters;
    filters << "*.mp3" << "*.flac" << "*.wav" << "*.ogg"
            << "*.wma" << "*.m4a" << "*.aac";

    QDirIterator it(dirPath, filters, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        QFileInfo fi(it.filePath());
        Track t;
        t.filePath = it.filePath();
        t.title = Track::titleFromPath(it.filePath());
        outTracks.append(t);
    }
}

// ============================================================
// Stylesheet - Dark theme
// ============================================================

void MainWindow::applyStylesheet()
{
    QString style = R"(
        /* ===== Main Window ===== */
        QMainWindow {
            background-color: #f0f2f5;
        }

        QWidget#rightPanel {
            background-color: #ffffff;
            border-radius: 8px;
        }

        QWidget#controlBar {
            background-color: #ffffff;
            border-top: 1px solid #e4e7ed;
        }

        /* ===== Group Box ===== */
        QGroupBox {
            color: #303133;
            border: 1px solid #e4e7ed;
            border-radius: 8px;
            margin-top: 12px;
            background-color: #ffffff;
            font-weight: bold;
            font-size: 13px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 12px;
            padding: 0 8px;
            color: #909399;
        }

        /* ===== Playlist ===== */
        QListWidget {
            background-color: #ffffff;
            border: none;
            border-radius: 4px;
            color: #303133;
            padding: 4px;
            outline: none;
            font-size: 13px;
        }
        QListWidget::item {
            padding: 10px 12px;
            border-radius: 6px;
            margin: 1px 0;
        }
        QListWidget::item:hover {
            background-color: #f0f2f5;
        }
        QListWidget::item:selected {
            background-color: #ecf5ff;
            color: #409eff;
        }

        /* ===== Track Info ===== */
        QLabel#trackTitle {
            color: #303133;
        }
        QLabel#trackArtist {
            color: #606266;
        }
        QLabel#trackAlbum {
            color: #909399;
        }
        QLabel#coverPlaceholder {
            background-color: #f0f2f5;
            color: #909399;
            border-radius: 8px;
            font-size: 18px;
            font-weight: bold;
        }
        QLabel#offsetTitle {
            color: #909399;
            font-size: 12px;
        }
        QLabel#offsetValue {
            color: #409eff;
            font-size: 12px;
            font-weight: bold;
        }

        /* ===== Buttons ===== */
        QPushButton {
            background-color: #f5f7fa;
            color: #303133;
            border: 1px solid #dcdfe6;
            border-radius: 6px;
            padding: 6px 12px;
            font-size: 14px;
        }
        QPushButton:hover {
            background-color: #ecf5ff;
            border-color: #c6e2ff;
            color: #409eff;
        }
        QPushButton:pressed {
            background-color: #d9ecff;
        }

        QPushButton#offsetBtn {
            background-color: #f5f7fa;
            color: #606266;
            border: 1px solid #dcdfe6;
            border-radius: 4px;
            font-size: 11px;
            padding: 2px 6px;
        }
        QPushButton#offsetBtn:hover {
            background-color: #ecf5ff;
            color: #409eff;
        }

        QPushButton#controlBtn {
            background-color: transparent;
            border: none;
            font-size: 18px;
            color: #606266;
        }
        QPushButton#controlBtn:hover {
            background-color: #f0f2f5;
            border-radius: 8px;
            color: #409eff;
        }

        QPushButton#playButton {
            background-color: #409eff;
            color: white;
            border: none;
            border-radius: 24px;
            font-size: 20px;
        }
        QPushButton#playButton:hover {
            background-color: #66b1ff;
        }
        QPushButton#playButton:pressed {
            background-color: #3a8ee6;
        }

        QPushButton#modeBtn {
            background-color: transparent;
            border: none;
            font-size: 18px;
            color: #606266;
        }
        QPushButton#modeBtn:hover {
            background-color: #f0f2f5;
            border-radius: 8px;
            color: #409eff;
        }

        /* ===== Labels ===== */
        QLabel {
            color: #303133;
        }

        QLabel#modeLabel {
            color: #909399;
            font-size: 12px;
            padding: 0 8px;
        }

        QLabel#timeLabel {
            color: #606266;
            font-size: 12px;
        }

        /* ===== Sliders ===== */
        QSlider#progressSlider::groove:horizontal {
            height: 4px;
            background-color: #e4e7ed;
            border-radius: 2px;
        }
        QSlider#progressSlider::handle:horizontal {
            background-color: #409eff;
            width: 14px;
            height: 14px;
            margin: -5px 0;
            border-radius: 7px;
        }
        QSlider#progressSlider::sub-page:horizontal {
            background-color: #409eff;
            border-radius: 2px;
        }

        QSlider#volumeSlider::groove:horizontal {
            height: 3px;
            background-color: #e4e7ed;
            border-radius: 2px;
        }
        QSlider#volumeSlider::handle:horizontal {
            background-color: #909399;
            width: 12px;
            height: 12px;
            margin: -4px 0;
            border-radius: 6px;
        }
        QSlider#volumeSlider::sub-page:horizontal {
            background-color: #909399;
            border-radius: 2px;
        }

        /* ===== Search Box ===== */
        QLineEdit#searchBox {
            background-color: #f5f7fa;
            color: #303133;
            border: 1px solid #e4e7ed;
            border-radius: 16px;
            padding: 6px 14px;
            font-size: 13px;
        }
        QLineEdit#searchBox:focus {
            border-color: #409eff;
            background-color: #ffffff;
        }

        /* ===== Toolbar ===== */
        QToolBar#mainToolBar {
            background-color: #ffffff;
            border: none;
            border-bottom: 1px solid #e4e7ed;
            spacing: 8px;
            padding: 4px 12px;
        }
        QToolBar#mainToolBar QToolButton {
            color: #606266;
            padding: 4px 10px;
            border-radius: 4px;
        }
        QToolBar#mainToolBar QToolButton:hover {
            background-color: #f0f2f5;
        }
        QToolBar#mainToolBar::separator {
            color: #e4e7ed;
            width: 2px;
            margin: 4px 4px;
        }

        /* ===== Status Bar ===== */
        QStatusBar {
            background-color: #f5f7fa;
            color: #909399;
            border-top: 1px solid #e4e7ed;
            font-size: 12px;
            padding: 2px 8px;
        }
    )";

    setStyleSheet(style);
}
