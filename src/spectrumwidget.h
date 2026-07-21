#ifndef SPECTRUMWIDGET_H
#define SPECTRUMWIDGET_H

#include <QWidget>
#include <QList>

// Audio waveform visualization widget.
// Decodes an audio file via QAudioDecoder, computes per-segment
// energy levels, and renders bars with a moving playhead.
class SpectrumWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SpectrumWidget(QWidget *parent = nullptr);

    // Load and decode an audio file to compute the waveform.
    void loadAudioFile(const QString &filePath);

    // Clear the current waveform.
    void clear();

    // Update the playback position to move the playhead highlight.
    void updatePosition(qint64 positionMs, qint64 durationMs);

    static constexpr int MIN_HEIGHT = 80;
    static constexpr int BAR_COUNT = 200;

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    // Compute energy levels from decoded PCM data
    void computeEnergyLevels();

    QList<float> m_energyLevels;   // 0.0–1.0 per time segment
    QList<float> m_samples;        // raw PCM samples (mono, accumulated)
    qint64 m_currentPos = 0;
    qint64 m_duration = 0;
    bool m_loaded = false;
};

#endif // SPECTRUMWIDGET_H
