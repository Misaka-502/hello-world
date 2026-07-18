#include "lyricswidget.h"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QEvent>

LyricsWidget::LyricsWidget(QWidget *parent)
    : QWidget(parent)
    , m_hoverTimer(new QTimer(this))
    , m_autoReturn(new QTimer(this))
{
    setMouseTracking(true);
    setMinimumHeight(LINE_H * 5);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_hoverTimer->setSingleShot(true);
    m_hoverTimer->setInterval(500);
    connect(m_hoverTimer, &QTimer::timeout, this, &LyricsWidget::onHoverTimeout);

    m_autoReturn->setSingleShot(true);
    m_autoReturn->setInterval(AUTO_RETURN_MS);
    connect(m_autoReturn, &QTimer::timeout, this, &LyricsWidget::onAutoReturn);
}

void LyricsWidget::setLyrics(const LrcParser &parser)
{
    m_lines = parser.lines();
    m_curIdx = -1;
    m_viewTopIdx = 0;
    m_scrollPx = 0;
    m_browsing = false;
    m_dragging = false;
    m_hoverLineIdx = -1;
    m_highlightedIdx = -1;
    m_hoverTimer->stop();
    m_autoReturn->stop();
    update();
}

void LyricsWidget::clearLyrics()
{
    m_lines.clear();
    m_curIdx = -1;
    m_viewTopIdx = 0;
    m_scrollPx = 0;
    m_browsing = false;
    m_dragging = false;
    m_hoverLineIdx = -1;
    m_highlightedIdx = -1;
    m_hoverTimer->stop();
    m_autoReturn->stop();
    update();
}

void LyricsWidget::updatePlaybackPosition(qint64 positionMs, int offsetMs)
{
    if (m_lines.isEmpty())
        return;

    qint64 t = positionMs + offsetMs;

    // Binary-search style: find last line with timeMs <= t
    int idx = -1;
    for (int i = 0; i < m_lines.size(); i++) {
        if (m_lines[i].timeMs <= t)
            idx = i;
        else
            break;
    }

    m_curIdx = idx;

    // In follow mode, keep current line in the middle of the view
    if (!m_browsing && !m_dragging) {
        if (idx >= 0) {
            // Center the current line: it should be roughly in the middle
            int centerLine = height() / LINE_H / 2; // how many lines fit above center
            m_viewTopIdx = idx - centerLine;
            if (m_viewTopIdx < 0) m_viewTopIdx = 0;
        }
        m_scrollPx = 0;
    }

    update();
}

// ============================================================
// paintEvent — draw all visible lines top-to-bottom
// ============================================================

void LyricsWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Background
    p.fillRect(rect(), QColor(255, 255, 255));

    if (m_lines.isEmpty()) {
        p.setPen(QColor(144, 147, 153));
        p.setFont(QFont("sans-serif", 14));
        p.drawText(rect(), Qt::AlignCenter, "No lyrics loaded");
        return;
    }

    int w = width();
    int h = height();

    // The first visible line index (accounting for fine scroll offset)
    int topIdx = m_viewTopIdx;
    int offsetPx = m_scrollPx;

    // How many lines fit on screen + 1 extra for partial
    int visibleLines = h / LINE_H + 2;

    for (int i = 0; i < visibleLines; i++) {
        int lineIdx = topIdx + i;
        if (lineIdx < 0 || lineIdx >= m_lines.size())
            continue;

        int y = i * LINE_H - offsetPx;
        if (y < -LINE_H || y > h + LINE_H)
            continue;

        bool isActive = (lineIdx == m_curIdx) && !m_browsing;

        // ---- Style ----
        QColor col;
        int fs;
        if (isActive) {
            col = QColor(64, 158, 255);  // bright blue
            fs = 17;
        } else {
            // Fade gently with distance — clear enough to read,
            // but never as prominent as the active line
            int dist = qAbs(lineIdx - m_curIdx);
            int alpha = 210 - dist * 20;
            if (alpha < 100) alpha = 100;
            col = QColor(80, 82, 86, alpha);
            fs = 13;
        }

        QFont font("sans-serif", fs);
        if (isActive) font.setBold(true);
        p.setFont(font);
        p.setPen(col);

        // Draw text centered horizontally, vertically centered in its line slot
        QString text = m_lines[lineIdx].text;
        // Elide if needed
        QFontMetrics fm(font);
        int maxW = w - 60;
        if (fm.horizontalAdvance(text) > maxW)
            text = fm.elidedText(text, Qt::ElideRight, maxW);

        QRect r(30, y, w - 60, LINE_H);
        p.drawText(r, Qt::AlignVCenter | Qt::AlignHCenter, text);

        // Draw highlight border around hovered line
        if (lineIdx == m_highlightedIdx) {
            QRect hr(20, y + 2, w - 40, LINE_H - 4);
            p.setPen(QPen(QColor(64, 158, 255), 2));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(hr, 8, 8);
        }
    }
}

// ============================================================
// Mouse — drag to scroll + hover highlight + click to jump
// ============================================================

void LyricsWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || m_lines.isEmpty()) {
        QWidget::mousePressEvent(event);
        return;
    }

    // If a line is highlighted (hovered for 0.5s), click jumps to it
    if (m_highlightedIdx >= 0 && m_highlightedIdx < m_lines.size()) {
        emit jumpRequested(m_lines[m_highlightedIdx].timeMs);
        // Clear highlight and return to follow mode
        m_highlightedIdx = -1;
        m_hoverLineIdx = -1;
        m_hoverTimer->stop();
        onAutoReturn();
        return;
    }

    // Otherwise start drag
    m_dragging = true;
    m_browsing = true;
    m_dragY = event->pos().y();
    m_highlightedIdx = -1;
    m_hoverTimer->stop();
    m_autoReturn->stop();
    emit browsingStateChanged(true);
    setCursor(Qt::ClosedHandCursor);
}

void LyricsWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging) {
        int dy = event->pos().y() - m_dragY;
        m_scrollPx -= dy;
        m_dragY = event->pos().y();

        // Normalize: convert pixel scroll to line changes
        while (m_scrollPx >= LINE_H) {
            m_scrollPx -= LINE_H;
            m_viewTopIdx++;
        }
        while (m_scrollPx <= -LINE_H) {
            m_scrollPx += LINE_H;
            m_viewTopIdx--;
        }
        // Clamp
        if (m_viewTopIdx < 0) { m_viewTopIdx = 0; m_scrollPx = 0; }
        int maxTop = m_lines.size() - 1;
        if (m_viewTopIdx > maxTop) { m_viewTopIdx = maxTop; m_scrollPx = 0; }

        update();
    } else {
        // Hover detection: which line is the mouse over?
        int idx = lineAtY(event->pos().y());
        if (idx != m_hoverLineIdx) {
            m_hoverLineIdx = idx;
            m_highlightedIdx = -1;  // clear old highlight
            if (idx >= 0) {
                m_hoverTimer->start();  // restart 0.5s timer
            } else {
                m_hoverTimer->stop();
            }
            setCursor(idx >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
            update();
        }
    }
}

void LyricsWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_dragging) {
        m_dragging = false;
        m_autoReturn->start();
        setCursor(Qt::OpenHandCursor);
    }
}

void LyricsWidget::wheelEvent(QWheelEvent *event)
{
    if (m_lines.isEmpty())
        return;

    m_browsing = true;
    m_dragging = false;
    m_highlightedIdx = -1;
    m_hoverTimer->stop();
    m_autoReturn->stop();
    m_autoReturn->start();
    emit browsingStateChanged(true);

    int steps = event->angleDelta().y() / 120;
    m_scrollPx -= steps * LINE_H;

    while (m_scrollPx >= LINE_H) {
        m_scrollPx -= LINE_H;
        m_viewTopIdx++;
    }
    while (m_scrollPx <= -LINE_H) {
        m_scrollPx += LINE_H;
        m_viewTopIdx--;
    }
    if (m_viewTopIdx < 0) { m_viewTopIdx = 0; m_scrollPx = 0; }
    int maxTop = m_lines.size() - 1;
    if (m_viewTopIdx > maxTop) { m_viewTopIdx = maxTop; m_scrollPx = 0; }

    setCursor(Qt::OpenHandCursor);
    update();
}

void LyricsWidget::leaveEvent(QEvent *)
{
    m_hoverLineIdx = -1;
    m_highlightedIdx = -1;
    m_hoverTimer->stop();
    setCursor(Qt::ArrowCursor);
    update();
}

void LyricsWidget::onHoverTimeout()
{
    if (m_hoverLineIdx >= 0 && m_hoverLineIdx < m_lines.size()) {
        m_highlightedIdx = m_hoverLineIdx;
        update();
    }
}

void LyricsWidget::onAutoReturn()
{
    m_browsing = false;
    m_dragging = false;
    m_scrollPx = 0;
    m_hoverLineIdx = -1;
    m_highlightedIdx = -1;
    m_hoverTimer->stop();

    // Re-center on current playback line
    if (m_curIdx >= 0) {
        int centerLine = height() / LINE_H / 2;
        m_viewTopIdx = m_curIdx - centerLine;
        if (m_viewTopIdx < 0) m_viewTopIdx = 0;
    }

    setCursor(Qt::ArrowCursor);
    emit browsingStateChanged(false);
    update();
}

int LyricsWidget::lineAtY(int y) const
{
    if (m_lines.isEmpty())
        return -1;
    // y = (lineIdx - m_viewTopIdx) * LINE_H - m_scrollPx
    // => lineIdx = (y + m_scrollPx) / LINE_H + m_viewTopIdx
    int idx = (y + m_scrollPx) / LINE_H + m_viewTopIdx;
    if (idx < 0 || idx >= m_lines.size())
        return -1;
    return idx;
}
