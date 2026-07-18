#include "transcoder.h"
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

Transcoder::Transcoder(QObject *parent)
    : QObject(parent)
    , m_process(new QProcess(this))
{
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &Transcoder::onProcessFinished);
    connect(m_process, &QProcess::readyReadStandardError,
            this, &Transcoder::onReadyReadStderr);
}

Transcoder::~Transcoder()
{
    if (m_process->state() == QProcess::Running) {
        m_process->kill();
        m_process->waitForFinished(2000);
    }
}

bool Transcoder::start(const QString &inputFile, const QString &outputFile,
                        const QString &outputFormat)
{
    if (!QFileInfo::exists(inputFile))
        return false;

    if (!isFfmpegAvailable())
        return false;

    m_outputFile = outputFile;
    m_inputDurationSecs = 0;

    QStringList args;
    args << "-y";  // overwrite output
    args << "-i" << inputFile;

    // Codec selection based on format
    if (outputFormat == "mp3") {
        args << "-codec:a" << "libmp3lame" << "-q:a" << "2";
    } else if (outputFormat == "flac") {
        args << "-codec:a" << "flac";
    } else if (outputFormat == "wav") {
        args << "-codec:a" << "pcm_s16le";
    } else {
        // Default: let ffmpeg choose
        args << "-codec:a" << "libmp3lame";
    }

    args << outputFile;

    m_process->start(ffmpegPath(), args);

    if (m_process->waitForStarted(3000)) {
        emit started(inputFile);
        return true;
    }

    emit errorOccurred("Failed to start ffmpeg: " + m_process->errorString());
    return false;
}

void Transcoder::cancel()
{
    if (m_process->state() == QProcess::Running) {
        m_process->kill();
    }
}

bool Transcoder::isFfmpegAvailable()
{
    QProcess test;
    test.start(ffmpegPath(), {"-version"});
    return test.waitForFinished(3000) && test.exitCode() == 0;
}

QString Transcoder::ffmpegPath()
{
    // Check common locations
    QStringList candidates = {
        "ffmpeg",
        "ffmpeg.exe",
#ifdef Q_OS_WIN
        QDir::currentPath() + "/ffmpeg.exe",
        QDir::currentPath() + "/ffmpeg/bin/ffmpeg.exe",
        "C:/ffmpeg/bin/ffmpeg.exe",
#endif
    };

    for (const auto &path : candidates) {
        QProcess test;
        test.start(path, {"-version"});
        if (test.waitForFinished(3000) && test.exitCode() == 0)
            return path;
    }
    return "ffmpeg";  // fallback
}

void Transcoder::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    bool success = (exitStatus == QProcess::NormalExit && exitCode == 0);
    emit finished(success, m_outputFile);
}

void Transcoder::onReadyReadStderr()
{
    QString output = QString::fromUtf8(m_process->readAllStandardError());
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    for (const auto &line : lines) {
        parseProgress(line);
    }
}

void Transcoder::parseProgress(const QString &line)
{
    // Parse "time=HH:MM:SS.xx" from ffmpeg stderr
    static QRegularExpression timeRx(R"(time=(\d+):(\d+):(\d+)\.(\d+))");
    auto match = timeRx.match(line);
    if (match.hasMatch()) {
        int h = match.captured(1).toInt();
        int m = match.captured(2).toInt();
        int s = match.captured(3).toInt();
        qint64 currentSecs = h * 3600 + m * 60 + s;

        // Also parse duration if present
        static QRegularExpression durRx(R"(Duration:\s*(\d+):(\d+):(\d+)\.(\d+))");
        auto durMatch = durRx.match(line);
        if (durMatch.hasMatch() && m_inputDurationSecs == 0) {
            int dh = durMatch.captured(1).toInt();
            int dm = durMatch.captured(2).toInt();
            int ds = durMatch.captured(3).toInt();
            m_inputDurationSecs = dh * 3600 + dm * 60 + ds;
        }

        if (m_inputDurationSecs > 0) {
            int percent = static_cast<int>(currentSecs * 100 / m_inputDurationSecs);
            percent = qBound(0, percent, 100);
            emit progressChanged(percent);
        }
    }
}
