#ifndef LRCPARSER_H
#define LRCPARSER_H

#include <QString>
#include <QList>
#include <QPair>

// Represents one timed line of lyrics
struct LrcLine {
    qint64 timeMs;   // timestamp in milliseconds
    QString text;    // the lyrics text for this line
};

// Parses .lrc lyrics files and provides time-to-line lookup
class LrcParser
{
public:
    LrcParser() = default;

    // Load and parse an .lrc file. Returns true on success.
    // Handles both UTF-8 and GBK encoding automatically.
    bool loadFromFile(const QString &filePath);

    // Given playback position (ms) and an offset (ms), return the index
    // of the current lyrics line. Returns -1 if no match or empty.
    int lineAtTime(qint64 positionMs, int offsetMs = 0) const;

    // Number of parsed lines
    int lineCount() const { return m_lines.size(); }

    // Get a specific line
    const LrcLine &lineAt(int index) const { return m_lines.at(index); }

    // All parsed lines (sorted by time)
    const QList<LrcLine> &lines() const { return m_lines; }

    // Check if any lyrics were parsed
    bool isEmpty() const { return m_lines.isEmpty(); }

    // Get the [offset:] tag value from the file (milliseconds)
    int fileOffset() const { return m_fileOffset; }

    // Get metadata tags
    QString title() const { return m_title; }
    QString artist() const { return m_artist; }
    QString album() const { return m_album; }

private:
    // Parse a raw line from the file
    void parseLine(const QString &line);

    // Try to decode raw bytes as UTF-8 first, then GBK
    static QString decodeText(const QByteArray &data);

    QList<LrcLine> m_lines;
    int m_fileOffset = 0;        // [offset:] tag, milliseconds offset (overall adjustment)
    QString m_title;
    QString m_artist;
    QString m_album;
};

#endif // LRCPARSER_H
