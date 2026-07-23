#ifndef TRANSCODER_H
#define TRANSCODER_H

#include <QObject>
#include <QProcess>
#include <QString>

// Wraps ffmpeg.exe transcoding via QProcess (async, non-blocking).
// The UI creates a Transcoder, connects signals, and calls start().
class Transcoder : public QObject
{
    Q_OBJECT

public:
    explicit Transcoder(QObject *parent = nullptr);
    ~Transcoder() override;

    // Start transcoding. inputFile -> outputFile using ffmpeg.
    // outputFormat: "mp3", "flac", "wav", etc.
    // bitrateKbps: target audio bitrate in kbps (0 = ffmpeg default).
    // sampleRateHz: target sample rate in Hz (0 = keep original).
    // Returns false if ffmpeg.exe is not found or inputs are invalid.
    bool start(const QString &inputFile, const QString &outputFile,
               const QString &outputFormat = "mp3",
               int bitrateKbps = 0, int sampleRateHz = 0);

    // Cancel the current transcode
    void cancel();

    // Check if ffmpeg.exe is available
    static bool isFfmpegAvailable();
    static QString ffmpegPath();

    bool isRunning() const { return m_process->state() == QProcess::Running; }

signals:
    void started(const QString &inputFile);
    void progressChanged(int percent);
    void finished(bool success, const QString &outputFile);
    void errorOccurred(const QString &error);

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onReadyReadStderr();

private:
    QProcess *m_process;
    QString m_outputFile;
    qint64 m_inputDurationSecs = 0;

    // Parse ffmpeg stderr output for progress info ("time=HH:MM:SS.xx")
    void parseProgress(const QString &line);
};

#endif // TRANSCODER_H
