#ifndef LYRICSWIDGET_H
#define LYRICSWIDGET_H

#include <QWidget>
#include <QTimer>
#include "lrcparser.h"

class LyricsRenderer;

class LyricsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LyricsWidget(QWidget *parent = nullptr);
    ~LyricsWidget() override;

    void setLyrics(const LrcParser &parser);
    void clearLyrics();

    // Called every time playback position changes
    void updatePlaybackPosition(qint64 positionMs, int offsetMs = 0);

    // Toggle karaoke (character-by-character) mode
    void setKaraokeMode(bool enabled);
    bool isKaraokeMode() const;

    // Notify renderers of theme change
    void setDarkMode(bool dark);

    static constexpr int LINE_H = 44;
    static constexpr int AUTO_RETURN_MS = 3000;

signals:
    void jumpRequested(qint64 timeMs);
    void browsingStateChanged(bool isBrowsing);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void leaveEvent(QEvent *event) override;

private slots:
    void onAutoReturn();
    void onHoverTimeout();

private:
    // Convert mouse Y to a line index
    int lineAtY(int y) const;

    QList<LrcLine> m_lines;

    int m_curIdx = -1;       // which line playback is at
    qint64 m_currentPlaybackPos = 0;  // current playback position (ms) incl. offset
    int m_viewTopIdx = 0;    // first visible line index (scroll position)
    int m_scrollPx = 0;      // fine scroll offset in pixels

    bool m_browsing = false; // user is manually scrolling
    bool m_dragging = false;
    int m_dragY = 0;
    int m_dragStartY = 0;  // where drag began — to distinguish click vs drag

    // Hover highlight
    int m_hoverLineIdx = -1;     // line mouse is currently over
    int m_highlightedIdx = -1;   // line highlighted after 0.5s hover
    QTimer *m_hoverTimer;
    QTimer *m_autoReturn;

    // Renderer strategy (both owned, m_renderer points to active one)
    LyricsRenderer *m_renderer = nullptr;
    LyricsRenderer *m_simpleRenderer = nullptr;
    LyricsRenderer *m_karaokeRenderer = nullptr;
};

#endif
