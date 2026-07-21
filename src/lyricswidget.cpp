#include "lyricswidget.h"
#include "lyricsrenderer.h"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QEvent>
#include <algorithm>

LyricsWidget::LyricsWidget(QWidget *parent)
    : QWidget(parent)
    , m_hoverTimer(new QTimer(this))
    , m_autoReturn(new QTimer(this))
    , m_simpleRenderer(new SimpleLyricsRenderer)
    , m_karaokeRenderer(new KaraokeLyricsRenderer)
{
    m_renderer = m_simpleRenderer;  // default
    setMouseTracking(true);
    setMinimumHeight(LINE_H * 5);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAttribute(Qt::WA_StyledBackground, true);  // let QSS set background

    m_hoverTimer->setSingleShot(true);
    m_hoverTimer->setInterval(500);
    connect(m_hoverTimer, &QTimer::timeout, this, &LyricsWidget::onHoverTimeout);

    m_autoReturn->setSingleShot(true);
    m_autoReturn->setInterval(AUTO_RETURN_MS);
    connect(m_autoReturn, &QTimer::timeout, this, &LyricsWidget::onAutoReturn);
}

LyricsWidget::~LyricsWidget()
{
    delete m_simpleRenderer;
    delete m_karaokeRenderer;
}

void LyricsWidget::setKaraokeMode(bool enabled)
{
    m_renderer = enabled ? m_karaokeRenderer : m_simpleRenderer;
    update();
}

bool LyricsWidget::isKaraokeMode() const
{
    return m_renderer == m_karaokeRenderer;
}

void LyricsWidget::setDarkMode(bool dark)
{
    m_simpleRenderer->setDarkMode(dark);
    m_karaokeRenderer->setDarkMode(dark);
    update();
}

void LyricsWidget::setLyrics(const LrcParser &parser)
{
    m_lines = parser.lines();
    m_curIdx = -1;
    m_currentPlaybackPos = 0;
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
    m_currentPlaybackPos = 0;
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
    m_currentPlaybackPos = t;

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
// paintEvent — draw all visible lines via the active renderer
// ============================================================

void LyricsWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Background handled by QSS

    if (m_lines.isEmpty()) {
        QColor placeholderColor = palette().color(QPalette::Text);
        placeholderColor.setAlpha(128);
        p.setPen(placeholderColor);
        p.setFont(QFont("sans-serif", 14));
        p.drawText(rect(), Qt::AlignCenter, "No lyrics loaded");
        return;
    }

    int w = width();
    int h = height();
    const int lineH = m_renderer->lineHeight();

    int topIdx = m_viewTopIdx;
    int offsetPx = m_scrollPx;

    int visibleLines = h / lineH + 2;

    for (int i = 0; i < visibleLines; i++) {
        int lineIdx = topIdx + i;
        if (lineIdx < 0 || lineIdx >= m_lines.size())
            continue;

        int y = i * lineH - offsetPx;
        if (y < -lineH || y > h + lineH)
            continue;

        bool isActive = (lineIdx == m_curIdx) && !m_browsing;

        // Compute character progress for karaoke effect
        float charProgress = 0.0f;
        if (isActive && m_curIdx >= 0) {
            qint64 curTime = m_lines[m_curIdx].timeMs;
            if (m_curIdx + 1 < m_lines.size()) {
                qint64 nextTime = m_lines[m_curIdx + 1].timeMs;
                if (nextTime > curTime) {
                    qint64 elapsed = m_currentPlaybackPos - curTime;
                    charProgress = static_cast<float>(elapsed)
                                   / static_cast<float>(nextTime - curTime);
                    charProgress = std::clamp(charProgress, 0.0f, 1.0f);
                }
            }
        }

        // Elide text if needed
        QString text = m_lines[lineIdx].text;
        QFontMetrics fm(QFont("sans-serif", isActive ? 20 : 13));
        int maxW = w - 60;
        if (fm.horizontalAdvance(text) > maxW)
            text = fm.elidedText(text, Qt::ElideRight, maxW);

        QRect lineRect(30, y, w - 60, lineH);

        // Delegate drawing to the active renderer strategy
        m_renderer->paintLine(p, lineRect, text, isActive, charProgress);

        // Draw highlight border around hovered line (unchanged)
        if (lineIdx == m_highlightedIdx) {
            QRect hr(20, y + 2, w - 40, lineH - 4);
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

    // Otherwise prepare for potential drag (don't enter browsing yet)
    m_dragging = true;
    m_dragY = event->pos().y();
    m_dragStartY = event->pos().y();
    m_highlightedIdx = -1;
    m_hoverTimer->stop();
    m_autoReturn->stop();
    setCursor(Qt::ClosedHandCursor);
}

void LyricsWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging) {
        int dy = event->pos().y() - m_dragY;
        int totalDy = event->pos().y() - m_dragStartY;
        m_scrollPx -= dy;
        m_dragY = event->pos().y();

        // Only enter browsing mode on actual drag (> 5px to avoid click jitter)
        if (!m_browsing && qAbs(totalDy) > 5) {
            m_browsing = true;
            emit browsingStateChanged(true);
        }

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
                // During browsing, hovering a new line resets the 3s countdown
                if (m_browsing)
                    m_autoReturn->start();
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
        if (m_browsing) {
            // Actual drag happened — start 3s auto-return countdown
            m_autoReturn->start();
            setCursor(Qt::OpenHandCursor);
        } else {
            // Simple click without drag — stay in follow mode
            setCursor(Qt::ArrowCursor);
        }
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
