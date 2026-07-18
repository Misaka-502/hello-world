#include "lrcparser.h"
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <algorithm>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QStringDecoder>
#else
#include <QTextCodec>
#endif

bool LrcParser::loadFromFile(const QString &filePath)
{
    m_lines.clear();
    m_fileOffset = 0;
    m_title.clear();
    m_artist.clear();
    m_album.clear();

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QByteArray rawData = file.readAll();
    file.close();

    QString text = decodeText(rawData);

    // Use split() instead of QTextStream for reliability
    const QStringList lines = text.split(QChar('\n'));
    for (const QString &rawLine : lines) {
        QString line = rawLine.trimmed();
        if (!line.isEmpty())
            parseLine(line);
    }

    // Sort by timestamp
    std::sort(m_lines.begin(), m_lines.end(),
              [](const LrcLine &a, const LrcLine &b) {
                  return a.timeMs < b.timeMs;
              });

    return !m_lines.isEmpty();
}

void LrcParser::parseLine(const QString &line)
{
    // Match [mm:ss], [mm:ss.xx], or [mm:ss.xxx] timestamps
    // The fractional part is optional
    static QRegularExpression timeRx(R"(\[(\d{1,3}):(\d{2})(?:[\.:](\d{2,3}))?\])");

    // Check for metadata tags first
    static QRegularExpression tagRx(R"(\[ti:\s*(.+)\])", QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression arRx(R"(\[ar:\s*(.+)\])", QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression alRx(R"(\[al:\s*(.+)\])", QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression offsetRx(R"(\[offset:\s*([+-]?\d+)\])", QRegularExpression::CaseInsensitiveOption);

    auto tagMatch = tagRx.match(line);
    if (tagMatch.hasMatch()) {
        m_title = tagMatch.captured(1).trimmed();
        return;
    }
    auto arMatch = arRx.match(line);
    if (arMatch.hasMatch()) {
        m_artist = arMatch.captured(1).trimmed();
        return;
    }
    auto alMatch = alRx.match(line);
    if (alMatch.hasMatch()) {
        m_album = alMatch.captured(1).trimmed();
        return;
    }
    auto offsetMatch = offsetRx.match(line);
    if (offsetMatch.hasMatch()) {
        m_fileOffset = offsetMatch.captured(1).toInt();
        return;
    }

    // Find all timestamps in the line
    QList<qint64> timestamps;
    auto it = timeRx.globalMatch(line);
    while (it.hasNext()) {
        auto match = it.next();
        int minutes = match.captured(1).toInt();
        int seconds = match.captured(2).toInt();
        QString fracStr = match.captured(3);
        qint64 ms = minutes * 60000LL + seconds * 1000LL;
        if (!fracStr.isEmpty()) {
            if (fracStr.length() == 2)
                ms += fracStr.toInt() * 10;   // centiseconds -> ms
            else
                ms += fracStr.toInt();         // already milliseconds
        }
        timestamps.append(ms);
    }

    if (timestamps.isEmpty())
        return;

    // Extract text after the last timestamp bracket
    int lastBracket = line.lastIndexOf(']');
    QString text = line.mid(lastBracket + 1).trimmed();

    // Create a line entry for each timestamp
    for (qint64 ts : timestamps) {
        LrcLine lrcline;
        lrcline.timeMs = ts;
        lrcline.text = text;
        m_lines.append(lrcline);
    }
}

int LrcParser::lineAtTime(qint64 positionMs, int offsetMs) const
{
    if (m_lines.isEmpty())
        return -1;

    qint64 effectiveTime = positionMs + offsetMs;

    // Find the last line whose timestamp <= effectiveTime
    int result = -1;
    for (int i = 0; i < m_lines.size(); ++i) {
        if (m_lines[i].timeMs <= effectiveTime)
            result = i;
        else
            break;
    }
    return result;
}

QString LrcParser::decodeText(const QByteArray &data)
{
    // Try UTF-8 first
    QString text = QString::fromUtf8(data);

    // Heuristic: if the result contains replacement characters or looks garbled,
    // try GBK (commonly used for Chinese .lrc files)
    if (text.contains(QChar(0xFFFD))) {
        // Try GBK (codec name varies by Qt version)
        // In Qt6, we use QStringConverter
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        auto decoder = QStringDecoder(QStringDecoder::System); // use system locale for GBK
        // Explicitly try GBK
        QString gbkText = QString::fromLocal8Bit(data);
        // If GBK decoding produces fewer replacement chars, use it
        if (!gbkText.contains(QChar(0xFFFD)))
            return gbkText;
#else
        QTextCodec *codec = QTextCodec::codecForName("GBK");
        if (!codec)
            codec = QTextCodec::codecForLocale();
        if (codec) {
            QString gbkText = codec->toUnicode(data);
            return gbkText;
        }
#endif
    }

    return text;
}
