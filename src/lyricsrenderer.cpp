#include "lyricsrenderer.h"
#include <QFontMetrics>
#include <algorithm>

// ============================================================
// SimpleLyricsRenderer
// ============================================================

void SimpleLyricsRenderer::paintLine(QPainter &p, const QRect &rect,
                                      const QString &text, bool isActive,
                                      float charProgress)
{
    Q_UNUSED(charProgress)

    QColor col;
    QFont font("sans-serif");

    if (isActive) {
        col = m_dark ? QColor(137, 180, 250) : QColor(64, 158, 255);
        font.setPixelSize(17);
        font.setBold(true);
    } else {
        col = m_dark ? QColor(166, 173, 200, 160) : QColor(80, 82, 86, 180);
        font.setPixelSize(13);
    }

    p.setFont(font);
    p.setPen(col);

    QFontMetrics fm(font);
    int maxW = rect.width();
    QString displayText = text;
    if (fm.horizontalAdvance(text) > maxW)
        displayText = fm.elidedText(text, Qt::ElideRight, maxW);

    p.drawText(rect, Qt::AlignVCenter | Qt::AlignHCenter, displayText);
}

void SimpleLyricsRenderer::setDarkMode(bool dark)
{
    m_dark = dark;
}

// ============================================================
// KaraokeLyricsRenderer
// ============================================================

void KaraokeLyricsRenderer::paintLine(QPainter &p, const QRect &rect,
                                       const QString &text, bool isActive,
                                       float charProgress)
{
    if (!isActive || charProgress <= 0.0f) {
        // Inactive line — same as Simple inactive
        QColor col = m_dark ? QColor(166, 173, 200, 160) : QColor(80, 82, 86, 180);
        QFont font("sans-serif");
        font.setPixelSize(13);
        p.setFont(font);
        p.setPen(col);

        QFontMetrics fm(font);
        int maxW = rect.width();
        QString displayText = text;
        if (fm.horizontalAdvance(text) > maxW)
            displayText = fm.elidedText(text, Qt::ElideRight, maxW);

        p.drawText(rect, Qt::AlignVCenter | Qt::AlignHCenter, displayText);
        return;
    }

    // Active line — character-by-character karaoke rendering
    const float progress = std::clamp(charProgress, 0.0f, 1.0f);
    const int totalChars = text.length();

    int splitIdx = static_cast<int>(progress * totalChars);
    if (splitIdx < 0) splitIdx = 0;
    if (splitIdx > totalChars) splitIdx = totalChars;

    QString playedPart = text.left(splitIdx);
    QString unplayedPart = text.mid(splitIdx);

    QFont activeFont("sans-serif");
    activeFont.setPixelSize(20);
    activeFont.setBold(true);

    QFontMetrics fm(activeFont);

    int totalWidth = fm.horizontalAdvance(text);
    int playedWidth = fm.horizontalAdvance(playedPart);

    int startX = rect.left() + (rect.width() - totalWidth) / 2;
    if (startX < rect.left()) startX = rect.left();

    int textY = rect.top() + (rect.height() + fm.ascent() - fm.descent()) / 2;

    // Theme-aware colors
    QColor playedColor = m_dark ? QColor(137, 180, 250) : QColor(64, 158, 255);
    QColor unplayedColor = m_dark ? QColor(100, 102, 120) : QColor(200, 200, 210);

    if (!playedPart.isEmpty()) {
        p.setFont(activeFont);
        p.setPen(playedColor);
        p.drawText(startX, textY, playedPart);
    }

    if (!unplayedPart.isEmpty()) {
        p.setFont(activeFont);
        p.setPen(unplayedColor);
        p.drawText(startX + playedWidth, textY, unplayedPart);
    }
}

void KaraokeLyricsRenderer::setDarkMode(bool dark)
{
    m_dark = dark;
}
