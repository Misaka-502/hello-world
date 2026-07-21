#include "transcoder.h"
#include <QCoreApplication>
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
    return !ffmpegPath().isEmpty();
}

QString Transcoder::ffmpegPath()
{
    // Cache the result — searching + verifying is expensive
    static QString s_cachedPath;
    static bool s_checked = false;

    if (s_checked)
        return s_cachedPath;

    s_checked = true;

    // Build candidate list in priority order
    QStringList candidates;

    // 1. Check PATH (bare executable name)
    candidates << "ffmpeg" << "ffmpeg.exe";

    // 2. Check relative to application directory
    const QString appDir = QCoreApplication::applicationDirPath();
    candidates << appDir + "/ffmpeg.exe";
    candidates << appDir + "/ffmpeg/bin/ffmpeg.exe";

    // 3. Check relative to current working directory
    const QString cwd = QDir::currentPath();
    candidates << cwd + "/ffmpeg.exe";
    candidates << cwd + "/ffmpeg/bin/ffmpeg.exe";

    // 4. Gyan FFmpeg winget install (bundled with the project)
    // Search relative to cwd, app dir, and their parents (for build/ subdir case)
    const QString gyanRelPath =
        "Gyan.FFmpeg_Microsoft.Winget.Source_8wekyb3d8bbwe/"
        "ffmpeg-8.1.2-full_build/bin/ffmpeg.exe";
    candidates << cwd + "/" + gyanRelPath;
    candidates << appDir + "/" + gyanRelPath;
    {
        QDir parent(cwd);
        parent.cdUp();
        candidates << parent.absoluteFilePath(gyanRelPath);
    }
    {
        QDir parent(appDir);
        parent.cdUp();
        candidates << parent.absoluteFilePath(gyanRelPath);
    }

    // 5. Common absolute install paths on Windows
    candidates << "C:/ffmpeg/bin/ffmpeg.exe";

    for (const auto &path : candidates) {
        // Quick file-existence check first (avoid spawning QProcess for nothing)
        if (!path.contains('/') && !path.contains('\\')) {
            // Bare name like "ffmpeg" — skip file check, go straight to QProcess
        } else if (!QFileInfo::exists(path)) {
            continue;
        }

        QProcess test;
        test.start(path, {"-version"});
        if (test.waitForFinished(3000) && test.exitCode() == 0) {
            s_cachedPath = path;
            return s_cachedPath;
        }
    }

    s_cachedPath = QString();  // empty signals "not found"
    return s_cachedPath;
}

void Transcoder::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    bool success = (exitStatus == QProcess::NormalExit && exitCode == 0);
    emit finished(success, m_outputFile);
}

void Transcoder::onReadyReadStderr()
{
    // ffmpeg writes progress to stderr, using \r to overwrite the same line.
    // Split on both \n and \r so we process each progress update individually.
    QString output = QString::fromUtf8(m_process->readAllStandardError());
    QStringList lines = output.split(QRegularExpression("[\r\n]"), Qt::SkipEmptyParts);
    for (const auto &line : lines) {
        parseProgress(line.trimmed());
    }
}

void Transcoder::parseProgress(const QString &line)
{
    // --- 1. Try to extract total duration ---
    // Duration line looks like: "  Duration: 00:03:45.12, start: 0.000000, ..."
    // This appears early in ffmpeg output, BEFORE the progress lines.
    if (m_inputDurationSecs == 0) {
        static QRegularExpression durRx(
            R"(Duration:\s*(\d+):(\d+):(\d+)\.(\d+))");
        auto durMatch = durRx.match(line);
        if (durMatch.hasMatch()) {
            int dh = durMatch.captured(1).toInt();
            int dm = durMatch.captured(2).toInt();
            int ds = durMatch.captured(3).toInt();
            m_inputDurationSecs = dh * 3600 + dm * 60 + ds;
        }
    }

    // --- 2. Try to extract current progress time ---
    // Progress line looks like: "size=  1234kB time=00:01:23.45 bitrate=..."
    static QRegularExpression timeRx(
        R"(time=(\d+):(\d+):(\d+)\.(\d+))");
    auto match = timeRx.match(line);
    if (match.hasMatch()) {
        int h = match.captured(1).toInt();
        int m = match.captured(2).toInt();
        int s = match.captured(3).toInt();
        qint64 currentSecs = h * 3600 + m * 60 + s;

        if (m_inputDurationSecs > 0) {
            int percent = static_cast<int>(
                currentSecs * 100 / m_inputDurationSecs);
            percent = qBound(0, percent, 100);
            emit progressChanged(percent);
        }
    }
}
