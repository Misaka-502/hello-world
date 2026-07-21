#ifndef LYRICSRENDERER_H
#define LYRICSRENDERER_H

#include <QPainter>
#include <QString>
#include <QRect>

// Abstract strategy for rendering a single lyrics line.
// Polymorphism: SimpleLyricsRenderer (whole-line) vs KaraokeLyricsRenderer (per-char).
class LyricsRenderer
{
public:
    virtual ~LyricsRenderer() = default;

    // Paint one line of lyrics.
    // - p:        active QPainter
    // - rect:     bounding rectangle for this line
    // - text:     lyrics text to display
    // - isActive: true if this is the current playback line
    // - charProgress: 0.0–1.0, how far through this line playback has progressed
    virtual void paintLine(QPainter &p, const QRect &rect,
                           const QString &text, bool isActive,
                           float charProgress) = 0;

    virtual int lineHeight() const = 0;
    virtual QString name() const = 0;
    virtual void setDarkMode(bool dark) = 0;
};

// Simple whole-line rendering — matches the original behavior.
// Active line: bold blue; inactive lines: gray with distance-based fade.
class SimpleLyricsRenderer : public LyricsRenderer
{
public:
    void paintLine(QPainter &p, const QRect &rect,
                   const QString &text, bool isActive,
                   float charProgress) override;
    int lineHeight() const override { return 44; }
    QString name() const override { return "Simple"; }
    void setDarkMode(bool dark) override;
private:
    bool m_dark = false;
};

// Karaoke-style character-by-character highlighting.
// Active line: played chars in bright blue, unplayed chars in light gray,
// rendered character-by-character based on charProgress.
class KaraokeLyricsRenderer : public LyricsRenderer
{
public:
    void paintLine(QPainter &p, const QRect &rect,
                   const QString &text, bool isActive,
                   float charProgress) override;
    int lineHeight() const override { return 44; }
    QString name() const override { return "Karaoke"; }
    void setDarkMode(bool dark) override;
private:
    bool m_dark = false;
};

#endif // LYRICSRENDERER_H
