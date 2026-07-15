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

// ============================================================
// Constructor / Destructor
// ============================================================

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUI();
    applyStylesheet();

    // Show welcome message
    m_placeholderLabel->setText("🎵\n\n从播放列表中选择一首歌曲");
}

MainWindow::~MainWindow() = default;

// ============================================================
// Main UI Setup
// ============================================================

void MainWindow::setupUI()
{
    setWindowTitle("音乐播放器");
    resize(1100, 720);
    setMinimumSize(900, 600);

    // ----- Central widget -----
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    // Root vertical layout: main area + control bar
    QVBoxLayout* rootLayout = new QVBoxLayout(centralWidget);
    rootLayout->setSpacing(8);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    // Main horizontal layout (left panel + right panel)
    QHBoxLayout* mainLayout = new QHBoxLayout();
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(12, 12, 12, 12);

    // ----- Left panel -----
    setupLeftPanel();
    mainLayout->addWidget(m_playlistWidget->parentWidget());

    // ----- Right panel -----
    setupRightPanel();
    mainLayout->addWidget(m_rightPanel);
    mainLayout->setStretchFactor(m_rightPanel, 2);

    rootLayout->addLayout(mainLayout);

    QWidget* controlBar = setupControlBar();
    rootLayout->addWidget(controlBar);

    // ----- Toolbar -----
    setupToolBar();

    // ----- Status bar -----
    statusBar()->showMessage("就绪");
}

// ============================================================
// Left Panel - Playlist
// ============================================================

void MainWindow::setupLeftPanel()
{
    QGroupBox* playlistGroup = new QGroupBox("播放列表", this);
    playlistGroup->setMinimumWidth(280);
    playlistGroup->setMaximumWidth(350);

    QVBoxLayout* layout = new QVBoxLayout(playlistGroup);
    layout->setContentsMargins(8, 20, 8, 8);
    layout->setSpacing(4);

    m_playlistWidget = new QListWidget(this);
    m_playlistWidget->setSelectionMode(QAbstractItemView::SingleSelection);

    // Add some dummy items for preview
    QStringList dummySongs = {
      
    };
    m_playlistWidget->addItems(dummySongs);

    layout->addWidget(m_playlistWidget);
}

// ============================================================
// Right Panel - Track Info Display
// ============================================================

void MainWindow::setupRightPanel()
{
    m_rightPanel = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(m_rightPanel);
    layout->setSpacing(12);
    layout->setContentsMargins(24, 24, 24, 24);

    // Push content to the lower portion of the panel
    layout->addStretch(2);

    // ----- Track title (hidden initially) -----
    m_titleLabel = new QLabel("歌曲标题", this);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(22);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    m_titleLabel->hide();
    layout->addWidget(m_titleLabel);

    // ----- Artist (hidden initially) -----
    m_artistLabel = new QLabel("歌手名称", this);
    m_artistLabel->setAlignment(Qt::AlignCenter);
    QFont artistFont = m_artistLabel->font();
    artistFont.setPointSize(14);
    m_artistLabel->setFont(artistFont);
    m_artistLabel->hide();
    layout->addWidget(m_artistLabel);

    // ----- Album (hidden initially) -----
    m_albumLabel = new QLabel("专辑名称", this);
    m_albumLabel->setAlignment(Qt::AlignCenter);
    QFont albumFont = m_albumLabel->font();
    albumFont.setPointSize(12);
    m_albumLabel->setFont(albumFont);
    m_albumLabel->hide();
    layout->addWidget(m_albumLabel);

    // ----- Placeholder (shown when no track selected) -----
    m_placeholderLabel = new QLabel(this);
    m_placeholderLabel->setAlignment(Qt::AlignCenter);
    QFont placeholderFont = m_placeholderLabel->font();
    placeholderFont.setPointSize(18);
    m_placeholderLabel->setFont(placeholderFont);
    layout->addWidget(m_placeholderLabel);

    // Push content up, keeping it in the lower half
    layout->addStretch(3);
}

// ============================================================
// Control Bar - returns QWidget*
// ============================================================

QWidget* MainWindow::setupControlBar()
{
    QWidget* controlBar = new QWidget(this);
    controlBar->setFixedHeight(72);
    controlBar->setObjectName("controlBar");

    QHBoxLayout* layout = new QHBoxLayout(controlBar);
    layout->setSpacing(6);
    layout->setContentsMargins(16, 6, 16, 6);

    // ----- Previous -----
    m_prevButton = new QPushButton("⏮", this);
    m_prevButton->setFixedSize(42, 42);
    m_prevButton->setObjectName("controlBtn");
    layout->addWidget(m_prevButton);

    // ----- Play/Pause -----
    m_playButton = new QPushButton("▶", this);
    m_playButton->setFixedSize(52, 52);
    m_playButton->setObjectName("playButton");
    layout->addWidget(m_playButton);

    // ----- Stop -----
    m_stopButton = new QPushButton("⏹", this);
    m_stopButton->setFixedSize(42, 42);
    m_stopButton->setObjectName("controlBtn");
    layout->addWidget(m_stopButton);

    // ----- Next -----
    m_nextButton = new QPushButton("⏭", this);
    m_nextButton->setFixedSize(42, 42);
    m_nextButton->setObjectName("controlBtn");
    layout->addWidget(m_nextButton);

    layout->addSpacing(12);

    // ----- Progress slider -----
    m_progressSlider = new QSlider(Qt::Horizontal, this);
    m_progressSlider->setRange(0, 1000);
    m_progressSlider->setValue(350);
    m_progressSlider->setObjectName("progressSlider");
    layout->addWidget(m_progressSlider);

    layout->addSpacing(8);

    // ----- Time label -----
    m_timeLabel = new QLabel("01:23 / 04:05", this);
    m_timeLabel->setMinimumWidth(120);
    m_timeLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_timeLabel);

    layout->addSpacing(8);

    // ----- Volume slider -----
    QWidget* volumeWidget = new QWidget(this);
    QHBoxLayout* volumeLayout = new QHBoxLayout(volumeWidget);
    volumeLayout->setSpacing(6);
    volumeLayout->setContentsMargins(0, 0, 0, 0);

    QLabel* volumeIcon = new QLabel("🔊", this);
    volumeLayout->addWidget(volumeIcon);

    m_volumeSlider = new QSlider(Qt::Horizontal, this);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(80);
    m_volumeSlider->setFixedWidth(80);
    m_volumeSlider->setObjectName("volumeSlider");
    volumeLayout->addWidget(m_volumeSlider);

    layout->addWidget(volumeWidget);

    layout->addSpacing(8);

    // ----- Play mode button -----
    m_modeButton = new QPushButton("🔁", this);
    m_modeButton->setFixedSize(40, 40);
    m_modeButton->setObjectName("modeBtn");
    layout->addWidget(m_modeButton);

    return controlBar;
}

// ============================================================
// Toolbar
// ============================================================

void MainWindow::setupToolBar()
{
    QToolBar* toolbar = addToolBar("工具栏");
    toolbar->setMovable(false);
    toolbar->setObjectName("mainToolBar");

    // Scan folder
    QAction* scanAction = new QAction("📁 扫描", this);
    toolbar->addAction(scanAction);

    // Add files
    QAction* addAction = new QAction("➕ 添加", this);
    toolbar->addAction(addAction);

    toolbar->addSeparator();

    // Search box
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("🔍 搜索歌曲...");
    m_searchEdit->setFixedWidth(220);
    m_searchEdit->setObjectName("searchBox");
    toolbar->addWidget(m_searchEdit);

    // Add stretch to push mode label to the right
    QWidget* spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolbar->addWidget(spacer);

    // Play mode indicator
    QLabel* modeLabel = new QLabel("列表循环", this);
    modeLabel->setObjectName("modeLabel");
    toolbar->addWidget(modeLabel);
}

// ============================================================
// Stylesheet - Light theme, softer colors
// ============================================================

void MainWindow::applyStylesheet()
{
    QString style = R"(
        /* ===== Main Window ===== */
        QMainWindow {
            background-color: #f0f2f5;
        }

        QWidget#controlBar {
            background-color: #ffffff;
            border-top: 1px solid #e4e7ed;
        }

        /* ===== Group Box ===== */
        QGroupBox {
            color: #2c3e50;
            border: 1px solid #dce1e8;
            border-radius: 8px;
            margin-top: 12px;
            background-color: #ffffff;
            font-weight: bold;
            font-size: 13px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 12px;
            padding: 0 8px 0 8px;
            color: #2c3e50;
        }

        /* ===== Playlist ===== */
        QListWidget {
            background-color: #ffffff;
            border: none;
            border-radius: 4px;
            color: #2c3e50;
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
            background-color: #eef2f7;
        }
        QListWidget::item:selected {
            background-color: #d6e4f0;
            color: #1a3a5c;
        }

        /* ===== Buttons ===== */
        QPushButton {
            background-color: #f5f7fa;
            color: #2c3e50;
            border: 1px solid #dce1e8;
            border-radius: 6px;
            padding: 6px 12px;
            font-size: 14px;
        }
        QPushButton:hover {
            background-color: #e8ecf2;
            border-color: #b8c4d0;
        }
        QPushButton:pressed {
            background-color: #d6dce6;
        }

        QPushButton#controlBtn {
            background-color: transparent;
            border: none;
            font-size: 18px;
            color: #4a5a6a;
        }
        QPushButton#controlBtn:hover {
            background-color: #eef2f7;
            border-radius: 8px;
        }

        QPushButton#playButton {
            background-color: #4a90d9;
            color: white;
            border: none;
            border-radius: 26px;
            font-size: 22px;
        }
        QPushButton#playButton:hover {
            background-color: #5b9fe6;
        }
        QPushButton#playButton:pressed {
            background-color: #3a7bc8;
        }

        QPushButton#modeBtn {
            background-color: transparent;
            border: none;
            font-size: 18px;
            color: #4a5a6a;
        }
        QPushButton#modeBtn:hover {
            background-color: #eef2f7;
            border-radius: 8px;
        }

        /* ===== Labels ===== */
        QLabel {
            color: #2c3e50;
        }

        QLabel#modeLabel {
            color: #6a7a8a;
            font-size: 13px;
            padding: 0 8px;
        }

        /* ===== Sliders ===== */
        QSlider#progressSlider::groove:horizontal {
            height: 4px;
            background-color: #e4e7ed;
            border-radius: 2px;
        }
        QSlider#progressSlider::handle:horizontal {
            background-color: #4a90d9;
            width: 14px;
            height: 14px;
            margin: -5px 0;
            border-radius: 7px;
        }
        QSlider#progressSlider::sub-page:horizontal {
            background-color: #4a90d9;
            border-radius: 2px;
        }

        QSlider#volumeSlider::groove:horizontal {
            height: 3px;
            background-color: #dce1e8;
            border-radius: 2px;
        }
        QSlider#volumeSlider::handle:horizontal {
            background-color: #6a8a9a;
            width: 12px;
            height: 12px;
            margin: -4px 0;
            border-radius: 6px;
        }
        QSlider#volumeSlider::sub-page:horizontal {
            background-color: #6a8a9a;
            border-radius: 2px;
        }

        /* ===== Search Box ===== */
        QLineEdit#searchBox {
            background-color: #f5f7fa;
            color: #2c3e50;
            border: 1px solid #dce1e8;
            border-radius: 18px;
            padding: 6px 16px;
            font-size: 13px;
        }
        QLineEdit#searchBox:focus {
            border-color: #4a90d9;
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
        QToolBar#mainToolBar::separator {
            color: #dce1e8;
            width: 2px;
        }

        /* ===== Status Bar ===== */
        QStatusBar {
            background-color: #f5f7fa;
            color: #6a7a8a;
            border-top: 1px solid #e4e7ed;
            font-size: 12px;
            padding: 2px 8px;
        }
    )";

    setStyleSheet(style);
}