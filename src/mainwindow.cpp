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
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QTimer>
#include <QMediaPlayer>
#include <QMediaMetaData>
#include <QSettings>
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
#include "playlistmanager.h"
#include "spectrumwidget.h"

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
    m_playlistManager = new PlaylistManager(this);
    m_library = new Library(this);
    m_lrcParser = new LrcParser();

    setupUI();

    // Load theme preference (defaults to light)
    QSettings settings;
    m_darkTheme = settings.value("theme/dark", false).toBool();
    applyTheme();

    // Load persisted library — keep full master copy in m_allTracks
    m_allTracks = m_library->loadLibrary();
    if (!m_allTracks.isEmpty()) {
        m_playlist->setTracks(m_allTracks);
        refreshPlaylistWidget();
        // Populate playlist combo
        m_playlistCombo->addItem("All Tracks");
        QStringList names = m_playlistManager->playlistNames();
        for (const auto &name : names)
            m_playlistCombo->addItem(name);
        m_playlistCombo->setCurrentIndex(0);
        statusBar()->showMessage(
            QString("Loaded %1 tracks from library").arg(m_allTracks.size()), 3000);
    }

    // --- Connect signals ---

    // Playlist signals
    connect(m_playlist, &Playlist::playlistChanged,
            this, &MainWindow::onPlaylistChanged);
    connect(m_playlist, &Playlist::playModeChanged,
            this, &MainWindow::onPlayModeChanged);

    // Playlist manager signals
    connect(m_playlistManager, &PlaylistManager::playlistsChanged,
            this, [this]() {
                // Refresh combo box without losing selection
                QString current = m_playlistCombo->currentText();
                m_playlistCombo->blockSignals(true);
                m_playlistCombo->clear();
                m_playlistCombo->addItem("All Tracks");
                QStringList names = m_playlistManager->playlistNames();
                for (const auto &name : names)
                    m_playlistCombo->addItem(name);
                int idx = m_playlistCombo->findText(current);
                m_playlistCombo->setCurrentIndex(idx >= 0 ? idx : 0);
                m_playlistCombo->blockSignals(false);
            });

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

    // Spectrum — update playhead on position change
    connect(m_player, &Player::positionChanged,
            this, [this](qint64 pos) {
                m_spectrumWidget->updatePosition(pos, m_player->duration());
            });

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
    // Save the full library (not just the currently visible playlist view)
    m_library->saveLibrary(m_allTracks);
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

    // --- Playlist selector (combo + buttons) ---
    QWidget *selectorRow = new QWidget(parent);
    QHBoxLayout *selLayout = new QHBoxLayout(selectorRow);
    selLayout->setContentsMargins(0, 0, 0, 0);
    selLayout->setSpacing(4);

    m_playlistCombo = new QComboBox(parent);
    m_playlistCombo->setObjectName("playlistCombo");
    m_playlistCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_playlistCombo->addItem("All Tracks");
    m_playlistCombo->view()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_playlistCombo->view(), &QWidget::customContextMenuRequested,
            this, &MainWindow::onPlaylistComboContextMenu);
    connect(m_playlistCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onPlaylistComboChanged);
    selLayout->addWidget(m_playlistCombo);

    m_newPlaylistBtn = new QPushButton("+", parent);
    m_newPlaylistBtn->setFixedSize(28, 28);
    m_newPlaylistBtn->setObjectName("miniBtn");
    m_newPlaylistBtn->setToolTip("New Playlist");
    connect(m_newPlaylistBtn, &QPushButton::clicked,
            this, &MainWindow::onNewPlaylist);
    selLayout->addWidget(m_newPlaylistBtn);

    m_autoGenBtn = new QPushButton("Auto", parent);
    m_autoGenBtn->setFixedHeight(28);
    m_autoGenBtn->setObjectName("miniBtn");
    m_autoGenBtn->setToolTip("Auto-generate playlists");
    connect(m_autoGenBtn, &QPushButton::clicked,
            this, &MainWindow::onAutoGenerate);
    selLayout->addWidget(m_autoGenBtn);

    layout->addWidget(selectorRow);

    // --- Playlist list ---
    m_playlistWidget = new QListWidget(parent);
    m_playlistWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_playlistWidget->setObjectName("playlistList");
    m_playlistWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_playlistWidget, &QListWidget::customContextMenuRequested,
            this, &MainWindow::onPlaylistContextMenu);
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

    m_spectrumWidget = new SpectrumWidget(parent);
    m_spectrumWidget->setObjectName("spectrumWidget");
    infoLayout->addWidget(m_spectrumWidget, 1);

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

    QPushButton *addToPlaylistBtn = new QPushButton("+", parent);
    addToPlaylistBtn->setFixedSize(28, 28);
    addToPlaylistBtn->setObjectName("miniBtn");
    addToPlaylistBtn->setToolTip("Add to playlist");
    connect(addToPlaylistBtn, &QPushButton::clicked,
            this, &MainWindow::onQuickAddToPlaylist);
    offsetLayout->addWidget(addToPlaylistBtn);

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

    m_karaokeAction = toolbar->addAction(
        QString::fromUtf8("\xF0\x9F\x8E\xA4") + " Karaoke");
    m_karaokeAction->setCheckable(true);
    connect(m_karaokeAction, &QAction::triggered, this, &MainWindow::onToggleKaraoke);

    m_themeAction = toolbar->addAction(
        QString::fromUtf8("\xF0\x9F\x8C\x99") + " Dark");
    m_themeAction->setCheckable(true);
    m_themeAction->setChecked(m_darkTheme);
    connect(m_themeAction, &QAction::triggered, this, &MainWindow::onToggleTheme);

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

void MainWindow::onToggleKaraoke()
{
    bool enabled = m_karaokeAction->isChecked();
    m_lyricsWidget->setKaraokeMode(enabled);
    statusBar()->showMessage(
        enabled ? "Karaoke mode ON" : "Karaoke mode OFF", 2000);
}

void MainWindow::onToggleTheme()
{
    m_darkTheme = m_themeAction->isChecked();
    m_themeAction->setText(m_darkTheme
        ? QString::fromUtf8("\xE2\x98\x80\xEF\xB8\x8F") + " Light"
        : QString::fromUtf8("\xF0\x9F\x8C\x99") + " Dark");
    applyTheme();

    // Persist choice
    QSettings settings;
    settings.setValue("theme/dark", m_darkTheme);
    statusBar()->showMessage(
        m_darkTheme ? "Dark theme" : "Light theme", 2000);
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
    // Add 500ms — update both the active view and the master list
    QString curPath = m_playlist->trackAt(idx).filePath;
    m_playlist->trackAt(idx).lyricsOffset += 500;
    for (auto &t : m_allTracks) {
        if (t.filePath == curPath) {
            t.lyricsOffset = m_playlist->trackAt(idx).lyricsOffset;
            break;
        }
    }
    m_library->saveLibrary(m_allTracks);
    int offsetMs = m_playlist->trackAt(idx).lyricsOffset;
    m_lyricsOffsetLabel->setText(
        QString("Offset: %1s").arg(offsetMs / 1000.0, 0, 'f', 1));
}

void MainWindow::onLyricsOffsetMinus()
{
    int idx = m_playlist->currentIndex();
    if (idx < 0) return;
    QString curPath = m_playlist->trackAt(idx).filePath;
    m_playlist->trackAt(idx).lyricsOffset -= 500;
    for (auto &t : m_allTracks) {
        if (t.filePath == curPath) {
            t.lyricsOffset = m_playlist->trackAt(idx).lyricsOffset;
            break;
        }
    }
    m_library->saveLibrary(m_allTracks);
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
    m_allTracks.clear();
    m_library->saveLibrary(m_allTracks);

    QList<Track> tracks;
    scanDirectory(dir, tracks);

    if (tracks.isEmpty()) {
        statusBar()->showMessage("No audio files found", 3000);
        return;
    }

    m_allTracks = tracks;
    m_playlist->setTracks(m_allTracks);
    m_library->saveLibrary(m_allTracks);
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

    // Check for duplicates by file path — use m_allTracks as master
    QSet<QString> existingPaths;
    for (const auto &t : m_allTracks)
        existingPaths.insert(t.filePath);

    int added = 0;
    for (const auto &t : newTracks) {
        if (!existingPaths.contains(t.filePath)) {
            m_playlist->addTrack(t);
            m_allTracks.append(t);
            existingPaths.insert(t.filePath);
            added++;
        }
    }

    m_library->saveLibrary(m_allTracks);
    refreshPlaylistWidget();
    statusBar()->showMessage(
        QString("Added %1 new tracks").arg(added), 3000);
}

// ============================================================
// Transcode slots
// ============================================================

void MainWindow::onTranscode()
{
    // Collect selected tracks from playlist (or use all filtered tracks if none selected)
    QList<Track> selectedTracks;
    QList<QListWidgetItem *> selItems = m_playlistWidget->selectedItems();
    QList<Track> filtered = m_playlist->filteredTracks();

    if (!selItems.isEmpty()) {
        for (auto *item : selItems) {
            int row = m_playlistWidget->row(item);
            if (row >= 0 && row < filtered.size())
                selectedTracks.append(filtered[row]);
        }
    } else {
        // No selection — fall back to the currently playing track
        int idx = m_playlist->currentIndex();
        if (idx >= 0 && idx < m_playlist->count())
            selectedTracks.append(m_playlist->trackAt(idx));
    }

    if (selectedTracks.isEmpty()) {
        QMessageBox::information(this, "Convert", "Select one or more tracks first.");
        return;
    }

    // --- Build options dialog ---
    QDialog dlg(this);
    dlg.setWindowTitle("Batch Convert — " + QString::number(selectedTracks.size()) + " track(s)");
    dlg.resize(420, 320);

    QVBoxLayout *dlgLayout = new QVBoxLayout(&dlg);
    dlgLayout->setSpacing(10);

    // Format selection
    QLabel *fmtLabel = new QLabel("Output format:");
    fmtLabel->setStyleSheet("font-weight: bold;");
    dlgLayout->addWidget(fmtLabel);

    QComboBox *formatCombo = new QComboBox(&dlg);
    formatCombo->addItem("MP3 (*.mp3)", "mp3");
    formatCombo->addItem("FLAC (*.flac)", "flac");
    formatCombo->addItem("WAV (*.wav)", "wav");
    formatCombo->setCurrentIndex(0);
    dlgLayout->addWidget(formatCombo);

    // Bitrate selection (relevant for MP3 mainly)
    QLabel *brLabel = new QLabel("Bitrate:");
    brLabel->setStyleSheet("font-weight: bold;");
    dlgLayout->addWidget(brLabel);

    QComboBox *bitrateCombo = new QComboBox(&dlg);
    bitrateCombo->addItem("Default (VBR q=2)", 0);
    bitrateCombo->addItem("128 kbps", 128);
    bitrateCombo->addItem("192 kbps", 192);
    bitrateCombo->addItem("256 kbps", 256);
    bitrateCombo->addItem("320 kbps", 320);
    bitrateCombo->setCurrentIndex(0);
    dlgLayout->addWidget(bitrateCombo);

    // Sample rate selection
    QLabel *srLabel = new QLabel("Sample rate:");
    srLabel->setStyleSheet("font-weight: bold;");
    dlgLayout->addWidget(srLabel);

    QComboBox *sampleRateCombo = new QComboBox(&dlg);
    sampleRateCombo->addItem("Keep original", 0);
    sampleRateCombo->addItem("44100 Hz", 44100);
    sampleRateCombo->addItem("48000 Hz", 48000);
    sampleRateCombo->setCurrentIndex(0);
    dlgLayout->addWidget(sampleRateCombo);

    // Output directory
    QLabel *dirLabel = new QLabel("Output directory:");
    dirLabel->setStyleSheet("font-weight: bold;");
    dlgLayout->addWidget(dirLabel);

    QWidget *dirRow = new QWidget(&dlg);
    QHBoxLayout *dirLayout = new QHBoxLayout(dirRow);
    dirLayout->setContentsMargins(0, 0, 0, 0);

    QLineEdit *dirEdit = new QLineEdit(&dlg);
    // Default to the first track's directory
    dirEdit->setText(QFileInfo(selectedTracks.first().filePath).absolutePath());
    dirEdit->setReadOnly(true);
    dirLayout->addWidget(dirEdit);

    QPushButton *browseBtn = new QPushButton("Browse...", &dlg);
    browseBtn->setObjectName("miniBtn");
    connect(browseBtn, &QPushButton::clicked, [&dirEdit, &dlg]() {
        QString dir = QFileDialog::getExistingDirectory(&dlg, "Select Output Directory");
        if (!dir.isEmpty())
            dirEdit->setText(dir);
    });
    dirLayout->addWidget(browseBtn);
    dlgLayout->addWidget(dirRow);

    // Info label
    QLabel *infoLabel = new QLabel(
        QString("%1 file(s) will be converted. Each file keeps its base name.")
            .arg(selectedTracks.size()));
    infoLabel->setStyleSheet("color: #909399; font-size: 12px;");
    infoLabel->setWordWrap(true);
    dlgLayout->addWidget(infoLabel);

    // Dialog buttons
    QDialogButtonBox *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    buttons->button(QDialogButtonBox::Ok)->setText("Start Convert");
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    dlgLayout->addWidget(buttons);

    if (dlg.exec() != QDialog::Accepted)
        return;

    // --- Gather settings ---
    QString outputDir = dirEdit->text();
    if (outputDir.isEmpty() || !QFileInfo::exists(outputDir)) {
        QMessageBox::warning(this, "Convert", "Output directory does not exist.");
        return;
    }

    m_batchQueue.clear();
    for (const auto &t : selectedTracks)
        m_batchQueue.append(t.filePath);

    m_batchIndex = 0;
    m_batchTotal = m_batchQueue.size();
    m_batchOutputDir = outputDir;
    m_batchFormat = formatCombo->currentData().toString();
    m_batchBitrate = bitrateCombo->currentData().toInt();
    m_batchSampleRate = sampleRateCombo->currentData().toInt();

    // Ensure transcoder instance exists
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

    processNextBatchItem();
}

void MainWindow::processNextBatchItem()
{
    if (m_batchQueue.isEmpty()) {
        // All done
        statusBar()->showMessage(
            QString("Batch conversion complete — %1 file(s)").arg(m_batchTotal), 5000);
        QMessageBox::information(this, "Convert",
            QString("Batch conversion finished!\n%1 file(s) processed.").arg(m_batchTotal));
        m_batchTotal = 0;
        m_batchIndex = 0;
        return;
    }

    QString inputFile = m_batchQueue.takeFirst();
    m_batchIndex++;

    QFileInfo fi(inputFile);
    QString outputFile = m_batchOutputDir + "/" + fi.completeBaseName() + "." + m_batchFormat;

    // Avoid overwriting the source file
    if (QFileInfo(outputFile).absoluteFilePath() == QFileInfo(inputFile).absoluteFilePath()) {
        outputFile = m_batchOutputDir + "/" + fi.completeBaseName() + "_converted." + m_batchFormat;
    }

    bool ok = m_transcoder->start(inputFile, outputFile, m_batchFormat,
                                  m_batchBitrate, m_batchSampleRate);
    if (!ok) {
        statusBar()->showMessage(
            QString("[%1/%2] Failed to start: %3")
                .arg(m_batchIndex).arg(m_batchTotal).arg(fi.fileName()), 5000);
        // Skip to next
        processNextBatchItem();
    } else {
        statusBar()->showMessage(
            QString("[%1/%2] Converting: %3...")
                .arg(m_batchIndex).arg(m_batchTotal).arg(fi.fileName()));
    }
}

void MainWindow::onTranscodeProgress(int percent)
{
    statusBar()->showMessage(
        QString("[%1/%2] Converting... %3%")
            .arg(m_batchIndex).arg(m_batchTotal).arg(percent));
}

void MainWindow::onTranscodeFinished(bool success, const QString &outputFile)
{
    if (success) {
        statusBar()->showMessage(
            QString("[%1/%2] Done: %3")
                .arg(m_batchIndex).arg(m_batchTotal)
                .arg(QFileInfo(outputFile).fileName()), 3000);
    } else {
        statusBar()->showMessage(
            QString("[%1/%2] Failed: %3")
                .arg(m_batchIndex).arg(m_batchTotal)
                .arg(QFileInfo(outputFile).fileName()), 5000);
    }

    // Process the next file in the queue
    QTimer::singleShot(500, this, &MainWindow::processNextBatchItem);
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

    // Load spectrum
    m_spectrumWidget->loadAudioFile(track.filePath);

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
                // Also sync to master list
                for (auto &t : m_allTracks) {
                    if (t.filePath == track.filePath) {
                        t.lyricsOffset = m_lrcParser->fileOffset();
                        break;
                    }
                }
                m_library->saveLibrary(m_allTracks);
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

    // Persist updated metadata — sync to master list and save
    for (auto &t : m_allTracks) {
        if (t.filePath == track.filePath) {
            t = track;
            break;
        }
    }
    m_library->saveLibrary(m_allTracks);
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

void MainWindow::onPlaylistComboContextMenu(const QPoint &pos)
{
    // Get the playlist name under the cursor
    QModelIndex index = m_playlistCombo->view()->indexAt(pos);
    if (!index.isValid())
        return;

    QString playlistName = index.data().toString();
    if (playlistName.isEmpty() || playlistName == "All Tracks")
        return;

    QMenu menu(this);

    QAction *playAction = menu.addAction(
        QString::fromUtf8("\xE2\x96\xB6") + " Play");
    QAction *pinAction = menu.addAction(
        m_playlistManager->isPinned(playlistName)
            ? QString::fromUtf8("\xF0\x9F\x93\x8C") + " Unpin"
            : QString::fromUtf8("\xF0\x9F\x93\x8C") + " Pin to Top");
    menu.addSeparator();
    QAction *deleteAction = menu.addAction(
        QString::fromUtf8("\xF0\x9F\x97\x91") + " Delete");

    QAction *chosen = menu.exec(m_playlistCombo->view()->viewport()->mapToGlobal(pos));

    if (chosen == playAction) {
        // Switch to playlist and start playing first track
        m_playlistCombo->setCurrentIndex(m_playlistCombo->findText(playlistName));
        applyPlaylistFilter();
        if (m_playlist->count() > 0) {
            m_playlist->setCurrentIndex(0);
            loadTrack(0);
        }
    } else if (chosen == pinAction) {
        m_playlistManager->togglePin(playlistName);
        statusBar()->showMessage(
            m_playlistManager->isPinned(playlistName)
                ? "Pinned: " + playlistName
                : "Unpinned: " + playlistName, 2000);
    } else if (chosen == deleteAction) {
        if (QMessageBox::question(this, "Delete Playlist",
                "Delete \"" + playlistName + "\"?",
                QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
            if (m_playlistCombo->currentText() == playlistName) {
                // Switch to All Tracks before deleting
                m_playlistCombo->setCurrentIndex(0);
            }
            m_playlistManager->deletePlaylist(playlistName);
            statusBar()->showMessage("Deleted: " + playlistName, 2000);
        }
    }
}

// ============================================================
// Playlist management slots
// ============================================================

void MainWindow::applyPlaylistFilter()
{
    QString name = m_playlistCombo->currentText();

    if (name == "All Tracks" || name.isEmpty()) {
        m_playlist->setTracks(m_allTracks);
    } else {
        QStringList paths = m_playlistManager->tracksInPlaylist(name);
        QList<Track> filtered;
        for (const auto &t : m_allTracks) {
            if (paths.contains(t.filePath))
                filtered.append(t);
        }
        m_playlist->setTracks(filtered);
    }

    refreshPlaylistWidget();
}

void MainWindow::onPlaylistComboChanged(int /*index*/)
{
    applyPlaylistFilter();
}

void MainWindow::onNewPlaylist()
{
    bool ok = false;
    QString name = QInputDialog::getText(
        this, "New Playlist", "Playlist name:",
        QLineEdit::Normal, "", &ok);

    if (!ok || name.trimmed().isEmpty())
        return;

    name = name.trimmed();

    if (m_playlistManager->playlistExists(name)) {
        QMessageBox::warning(this, "New Playlist",
                             "Playlist \"" + name + "\" already exists.");
        return;
    }

    // Show selection dialog with all tracks from master list
    if (m_allTracks.isEmpty()) {
        // Create empty playlist if no tracks in library
        if (m_playlistManager->createPlaylist(name)) {
            m_playlistCombo->setCurrentIndex(m_playlistCombo->findText(name));
            statusBar()->showMessage("Created playlist: " + name, 3000);
        }
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle("Select songs for \"" + name + "\"");
    dlg.resize(420, 400);

    QVBoxLayout *dlgLayout = new QVBoxLayout(&dlg);

    QLabel *hint = new QLabel("Check songs to add:");
    hint->setStyleSheet("color: #909399; font-size: 12px;");
    dlgLayout->addWidget(hint);

    QListWidget *list = new QListWidget(&dlg);
    list->setSelectionMode(QAbstractItemView::NoSelection);
    for (const auto &t : m_allTracks) {
        QString display = t.title;
        if (!t.artist.isEmpty())
            display += "  -  " + t.artist;
        QListWidgetItem *item = new QListWidgetItem(display, list);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
        item->setData(Qt::UserRole, t.filePath);
    }
    dlgLayout->addWidget(list);

    // Select all / Deselect all buttons
    QHBoxLayout *btnRow = new QHBoxLayout();
    QPushButton *selectAll = new QPushButton("Select All");
    QPushButton *deselectAll = new QPushButton("Deselect All");
    selectAll->setObjectName("miniBtn");
    deselectAll->setObjectName("miniBtn");
    btnRow->addWidget(selectAll);
    btnRow->addWidget(deselectAll);
    btnRow->addStretch();
    dlgLayout->addLayout(btnRow);

    connect(selectAll, &QPushButton::clicked, [list]() {
        for (int i = 0; i < list->count(); i++)
            list->item(i)->setCheckState(Qt::Checked);
    });
    connect(deselectAll, &QPushButton::clicked, [list]() {
        for (int i = 0; i < list->count(); i++)
            list->item(i)->setCheckState(Qt::Unchecked);
    });

    QDialogButtonBox *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    dlgLayout->addWidget(buttons);

    if (dlg.exec() != QDialog::Accepted)
        return;

    // Create playlist and add checked tracks
    if (!m_playlistManager->createPlaylist(name))
        return;

    int added = 0;
    for (int i = 0; i < list->count(); i++) {
        QListWidgetItem *item = list->item(i);
        if (item->checkState() == Qt::Checked) {
            QString path = item->data(Qt::UserRole).toString();
            if (m_playlistManager->addTrack(name, path))
                added++;
        }
    }

    m_playlistCombo->setCurrentIndex(m_playlistCombo->findText(name));
    statusBar()->showMessage(
        QString("Created \"%1\" with %2 songs").arg(name).arg(added), 3000);
}

void MainWindow::onAutoGenerate()
{
    QMenu menu(this);

    QAction *byArtist = menu.addAction("By Artist");
    QAction *byAlbum = menu.addAction("By Album");
    QAction *byTag = menu.addAction("By Tag");

    QAction *chosen = menu.exec(m_autoGenBtn->mapToGlobal(QPoint(0, m_autoGenBtn->height())));

    if (m_allTracks.isEmpty()) {
        QMessageBox::information(this, "Auto Generate",
                                 "No tracks in library. Scan a folder first.");
        return;
    }

    if (chosen == byArtist) {
        m_playlistManager->generateByArtist(m_allTracks);
        statusBar()->showMessage("Generated artist playlists", 3000);
    } else if (chosen == byAlbum) {
        m_playlistManager->generateByAlbum(m_allTracks);
        statusBar()->showMessage("Generated album playlists", 3000);
    } else if (chosen == byTag) {
        m_playlistManager->generateByTag(m_allTracks);
        statusBar()->showMessage("Generated tag playlists", 3000);
    }
}

void MainWindow::onPlaylistContextMenu(const QPoint &pos)
{
    QListWidgetItem *item = m_playlistWidget->itemAt(pos);
    if (!item)
        return;

    // Find the track in the filtered list
    int row = m_playlistWidget->row(item);
    QList<Track> filtered = m_playlist->filteredTracks();
    if (row < 0 || row >= filtered.size())
        return;

    // Store for context menu actions
    m_contextTrackPath = filtered[row].filePath;

    QMenu menu(this);
    QAction *editTagsAction = menu.addAction("Edit Tags...");
    menu.addSeparator();

    // "Add to Playlist" submenu
    QMenu *addMenu = menu.addMenu("Add to Playlist");
    QStringList userLists = m_playlistManager->userPlaylistNames();
    if (userLists.isEmpty()) {
        QAction *empty = addMenu->addAction("(no playlists — create one first)");
        empty->setEnabled(false);
    } else {
        for (const auto &name : userLists) {
            QAction *a = addMenu->addAction(name);
            a->setData(name);
        }
    }

    QAction *chosen = menu.exec(m_playlistWidget->mapToGlobal(pos));

    if (chosen == editTagsAction) {
        onEditTags();
    } else if (chosen && chosen->data().isValid()) {
        // Add to playlist
        QString playlistName = chosen->data().toString();
        if (m_playlistManager->addTrack(playlistName, m_contextTrackPath)) {
            statusBar()->showMessage(
                "Added to " + playlistName, 2000);
        }
    }
}

void MainWindow::onEditTags()
{
    if (m_contextTrackPath.isEmpty())
        return;

    // Find the track in the master list
    Track *track = nullptr;
    for (auto &t : m_allTracks) {
        if (t.filePath == m_contextTrackPath) {
            track = &t;
            break;
        }
    }
    if (!track)
        return;

    QString currentTags = track->tags.join(", ");
    bool ok = false;
    QString newTags = QInputDialog::getText(
        this, "Edit Tags",
        "Tags (comma-separated):",
        QLineEdit::Normal, currentTags, &ok);

    if (!ok)
        return;

    // Parse and update tags
    QStringList tagList;
    QStringList parts = newTags.split(',', Qt::SkipEmptyParts);
    for (const auto &p : parts) {
        QString trimmed = p.trimmed();
        if (!trimmed.isEmpty())
            tagList.append(trimmed);
    }
    track->tags = tagList;

    // Persist master list
    m_library->saveLibrary(m_allTracks);

    // Also update the in-memory playlist view
    QList<Track> playing = m_playlist->allTracks();
    for (auto &t : playing) {
        if (t.filePath == m_contextTrackPath)
            t.tags = tagList;
    }
    m_playlist->setTracks(playing);

    statusBar()->showMessage("Tags updated: " + track->tags.join(", "), 3000);
}

void MainWindow::onAddToPlaylist()
{
    // Called from context menu (handled inline in onPlaylistContextMenu)
}

void MainWindow::onQuickAddToPlaylist()
{
    int idx = m_playlist->currentIndex();
    if (idx < 0 || idx >= m_playlist->count()) {
        statusBar()->showMessage("No track playing", 2000);
        return;
    }

    QStringList userLists = m_playlistManager->userPlaylistNames();
    if (userLists.isEmpty()) {
        QMessageBox::information(this, "Add to Playlist",
                                 "No playlists yet. Create one first with the + button.");
        return;
    }

    QMenu menu(this);
    for (const auto &name : userLists) {
        QAction *a = menu.addAction(name);
        a->setData(name);
    }

    // Show menu below the + button (approximate position)
    QAction *chosen = menu.exec(QCursor::pos());

    if (chosen && chosen->data().isValid()) {
        QString playlistName = chosen->data().toString();
        const Track &track = m_playlist->trackAt(idx);
        if (m_playlistManager->addTrack(playlistName, track.filePath)) {
            statusBar()->showMessage(
                "Added \"" + track.title + "\" to " + playlistName, 2000);
        } else {
            statusBar()->showMessage("Already in " + playlistName, 2000);
        }
    }
}

// ============================================================
// Theme management
// ============================================================

void MainWindow::applyTheme()
{
    setStyleSheet(m_darkTheme ? darkStylesheet() : lightStylesheet());
    m_lyricsWidget->setDarkMode(m_darkTheme);
    m_lyricsWidget->update();
    m_spectrumWidget->update();
}

QString MainWindow::lightStylesheet() const
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

        QWidget#lyricsArea {
            background-color: #ffffff;
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

        QPushButton#miniBtn {
            background-color: #f5f7fa;
            color: #606266;
            border: 1px solid #dcdfe6;
            border-radius: 4px;
            font-size: 12px;
            padding: 2px 6px;
        }
        QPushButton#miniBtn:hover {
            background-color: #ecf5ff;
            color: #409eff;
        }

        /* ===== Combo Box ===== */
        QComboBox#playlistCombo {
            background-color: #f5f7fa;
            color: #303133;
            border: 1px solid #dcdfe6;
            border-radius: 4px;
            padding: 4px 8px;
            font-size: 12px;
        }
        QComboBox#playlistCombo:hover {
            border-color: #c6e2ff;
        }
        QComboBox#playlistCombo::drop-down {
            border: none;
            width: 20px;
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
        QToolBar#mainToolBar QToolButton:checked {
            background-color: #ecf5ff;
            color: #409eff;
            border-radius: 4px;
        }
        QToolBar#mainToolBar::separator {
            color: #e4e7ed;
            width: 2px;
            margin: 4px 4px;
        }

        /* ===== Spectrum Widget ===== */
        QWidget#spectrumWidget {
            background-color: #f5f7fa;
            border-radius: 6px;
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
    return style;
}

QString MainWindow::darkStylesheet() const
{
    return QString::fromUtf8(R"(
        /* ===== Main Window ===== */
        QMainWindow {
            background-color: #1a1a2e;
        }

        QWidget#rightPanel {
            background-color: #252536;
            border-radius: 8px;
        }

        QWidget#lyricsArea {
            background-color: #252536;
        }

        QWidget#controlBar {
            background-color: #252536;
            border-top: 1px solid #3e3e5e;
        }

        /* ===== Group Box ===== */
        QGroupBox {
            color: #cdd6f4;
            border: 1px solid #3e3e5e;
            border-radius: 8px;
            margin-top: 12px;
            background-color: #252536;
            font-weight: bold;
            font-size: 13px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 12px;
            padding: 0 8px;
            color: #a6adc8;
        }

        /* ===== Playlist ===== */
        QListWidget {
            background-color: #1e1e2e;
            border: none;
            border-radius: 4px;
            color: #cdd6f4;
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
            background-color: #313244;
        }
        QListWidget::item:selected {
            background-color: #313244;
            color: #89b4fa;
        }

        /* ===== Track Info ===== */
        QLabel#trackTitle {
            color: #cdd6f4;
        }
        QLabel#trackArtist {
            color: #a6adc8;
        }
        QLabel#trackAlbum {
            color: #6c7086;
        }
        QLabel#coverPlaceholder {
            background-color: #313244;
            color: #6c7086;
            border-radius: 8px;
            font-size: 18px;
            font-weight: bold;
        }
        QLabel#offsetTitle {
            color: #6c7086;
            font-size: 12px;
        }
        QLabel#offsetValue {
            color: #89b4fa;
            font-size: 12px;
            font-weight: bold;
        }

        /* ===== Buttons ===== */
        QPushButton {
            background-color: #313244;
            color: #cdd6f4;
            border: 1px solid #45475a;
            border-radius: 6px;
            padding: 6px 12px;
            font-size: 14px;
        }
        QPushButton:hover {
            background-color: #45475a;
            border-color: #585b70;
            color: #89b4fa;
        }
        QPushButton:pressed {
            background-color: #585b70;
        }

        QPushButton#offsetBtn {
            background-color: #313244;
            color: #a6adc8;
            border: 1px solid #45475a;
            border-radius: 4px;
            font-size: 11px;
            padding: 2px 6px;
        }
        QPushButton#offsetBtn:hover {
            background-color: #45475a;
            color: #89b4fa;
        }

        QPushButton#controlBtn {
            background-color: transparent;
            border: none;
            font-size: 18px;
            color: #a6adc8;
        }
        QPushButton#controlBtn:hover {
            background-color: #313244;
            border-radius: 8px;
            color: #89b4fa;
        }

        QPushButton#playButton {
            background-color: #89b4fa;
            color: #1e1e2e;
            border: none;
            border-radius: 24px;
            font-size: 20px;
        }
        QPushButton#playButton:hover {
            background-color: #b4d0fb;
        }
        QPushButton#playButton:pressed {
            background-color: #74a8f7;
        }

        QPushButton#modeBtn {
            background-color: transparent;
            border: none;
            font-size: 18px;
            color: #a6adc8;
        }
        QPushButton#modeBtn:hover {
            background-color: #313244;
            border-radius: 8px;
            color: #89b4fa;
        }

        QPushButton#miniBtn {
            background-color: #313244;
            color: #a6adc8;
            border: 1px solid #45475a;
            border-radius: 4px;
            font-size: 12px;
            padding: 2px 6px;
        }
        QPushButton#miniBtn:hover {
            background-color: #45475a;
            color: #89b4fa;
        }

        /* ===== Combo Box ===== */
        QComboBox#playlistCombo {
            background-color: #313244;
            color: #cdd6f4;
            border: 1px solid #45475a;
            border-radius: 4px;
            padding: 4px 8px;
            font-size: 12px;
        }
        QComboBox#playlistCombo:hover {
            border-color: #585b70;
        }
        QComboBox#playlistCombo::drop-down {
            border: none;
            width: 20px;
        }
        QComboBox QAbstractItemView {
            background-color: #313244;
            color: #cdd6f4;
            selection-background-color: #45475a;
            selection-color: #89b4fa;
        }

        /* ===== Labels ===== */
        QLabel {
            color: #cdd6f4;
        }

        QLabel#modeLabel {
            color: #6c7086;
            font-size: 12px;
            padding: 0 8px;
        }

        QLabel#timeLabel {
            color: #a6adc8;
            font-size: 12px;
        }

        /* ===== Sliders ===== */
        QSlider#progressSlider::groove:horizontal {
            height: 4px;
            background-color: #45475a;
            border-radius: 2px;
        }
        QSlider#progressSlider::handle:horizontal {
            background-color: #89b4fa;
            width: 14px;
            height: 14px;
            margin: -5px 0;
            border-radius: 7px;
        }
        QSlider#progressSlider::sub-page:horizontal {
            background-color: #89b4fa;
            border-radius: 2px;
        }

        QSlider#volumeSlider::groove:horizontal {
            height: 3px;
            background-color: #45475a;
            border-radius: 2px;
        }
        QSlider#volumeSlider::handle:horizontal {
            background-color: #a6adc8;
            width: 12px;
            height: 12px;
            margin: -4px 0;
            border-radius: 6px;
        }
        QSlider#volumeSlider::sub-page:horizontal {
            background-color: #a6adc8;
            border-radius: 2px;
        }

        /* ===== Search Box ===== */
        QLineEdit#searchBox {
            background-color: #313244;
            color: #cdd6f4;
            border: 1px solid #45475a;
            border-radius: 16px;
            padding: 6px 14px;
            font-size: 13px;
        }
        QLineEdit#searchBox:focus {
            border-color: #89b4fa;
            background-color: #1e1e2e;
        }

        /* ===== Toolbar ===== */
        QToolBar#mainToolBar {
            background-color: #252536;
            border: none;
            border-bottom: 1px solid #3e3e5e;
            spacing: 8px;
            padding: 4px 12px;
        }
        QToolBar#mainToolBar QToolButton {
            color: #a6adc8;
            padding: 4px 10px;
            border-radius: 4px;
        }
        QToolBar#mainToolBar QToolButton:hover {
            background-color: #313244;
        }
        QToolBar#mainToolBar QToolButton:checked {
            background-color: #313244;
            color: #89b4fa;
            border-radius: 4px;
        }
        QToolBar#mainToolBar::separator {
            color: #3e3e5e;
            width: 2px;
            margin: 4px 4px;
        }

        /* ===== Spectrum Widget ===== */
        QWidget#spectrumWidget {
            background-color: #1e1e2e;
            border-radius: 6px;
        }

        /* ===== Status Bar ===== */
        QStatusBar {
            background-color: #1e1e2e;
            color: #6c7086;
            border-top: 1px solid #3e3e5e;
            font-size: 12px;
            padding: 2px 8px;
        }
    )");
}
