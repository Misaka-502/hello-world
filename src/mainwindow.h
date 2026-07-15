#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class QListWidget;
class QLabel;
class QPushButton;
class QSlider;
class QLineEdit;
class QWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    // ========== UI Setup ==========
    void setupUI();
    void setupLeftPanel();
    void setupRightPanel();
    QWidget* setupControlBar();
    void setupToolBar();
    void applyStylesheet();

    // ========== UI Components ==========
    // Left panel - playlist
    QListWidget* m_playlistWidget;

    // Right panel - track info
    QWidget* m_rightPanel;
    QLabel* m_placeholderLabel;
    QLabel* m_titleLabel;
    QLabel* m_artistLabel;
    QLabel* m_albumLabel;

    // Control bar
    QPushButton* m_prevButton;
    QPushButton* m_playButton;
    QPushButton* m_stopButton;
    QPushButton* m_nextButton;
    QPushButton* m_modeButton;
    QSlider* m_progressSlider;
    QSlider* m_volumeSlider;
    QLabel* m_timeLabel;

    // Toolbar
    QLineEdit* m_searchEdit;
};

#endif // MAINWINDOW_H