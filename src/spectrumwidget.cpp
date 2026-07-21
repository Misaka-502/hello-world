#include "spectrumwidget.h"
#include <QPainter>
#include <QAudioDecoder>
#include <QAudioBuffer>
#include <QFileInfo>
#include <QUrl>
#include <algorithm>
#include <cmath>

SpectrumWidget::SpectrumWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(MIN_HEIGHT);
    setMinimumWidth(200);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setAttribute(Qt::WA_StyledBackground, true);  // let QSS set background
}

void SpectrumWidget::loadAudioFile(const QString &filePath)
{
    m_energyLevels.clear();
    m_samples.clear();
    m_currentPos = 0;
    m_duration = 0;
    m_loaded = false;

    if (!QFileInfo::exists(filePath))
        return;

    // Decode audio to PCM samples
    auto *decoder = new QAudioDecoder(this);
    decoder->setSource(QUrl::fromLocalFile(filePath));

    // Collect PCM sample buffers
    connect(decoder, &QAudioDecoder::bufferReady, this, [this, decoder]() {
        QAudioBuffer buf = decoder->read();
        if (!buf.isValid())
            return;

        // Convert to mono float samples
        const QAudioFormat &fmt = buf.format();
        int channels = fmt.channelCount();
        int sampleCount = buf.sampleCount();
        int bytesPerSample = fmt.bytesPerSample();

        if (bytesPerSample <= 0 || channels <= 0)
            return;

        const char *data = buf.constData<char>();
        // Determine sample scale based on format
        float scale = 1.0f;
        if (fmt.sampleFormat() == QAudioFormat::Int16)
            scale = 1.0f / 32768.0f;
        else if (fmt.sampleFormat() == QAudioFormat::Int32)
            scale = 1.0f / 2147483648.0f;
        else if (fmt.sampleFormat() == QAudioFormat::UInt8)
            scale = 1.0f / 128.0f;

        for (int i = 0; i < sampleCount; i++) {
            // Average across channels -> mono
            float monoVal = 0.0f;
            for (int ch = 0; ch < channels; ch++) {
                int offset = (i * channels + ch) * bytesPerSample;
                if (offset + bytesPerSample > static_cast<int>(buf.byteCount()))
                    continue;
                float val = 0.0f;
                if (fmt.sampleFormat() == QAudioFormat::Int16) {
                    const auto *p = reinterpret_cast<const int16_t *>(data + offset);
                    val = (*p) * scale;
                } else if (fmt.sampleFormat() == QAudioFormat::Int32) {
                    const auto *p = reinterpret_cast<const int32_t *>(data + offset);
                    val = (*p) * scale;
                } else if (fmt.sampleFormat() == QAudioFormat::UInt8) {
                    const auto *p = reinterpret_cast<const uint8_t *>(data + offset);
                    val = (static_cast<int>(*p) - 128) * scale;
                } else if (fmt.sampleFormat() == QAudioFormat::Float) {
                    const auto *p = reinterpret_cast<const float *>(data + offset);
                    val = *p;
                }
                monoVal += val;
            }
            monoVal /= static_cast<float>(channels);
            m_samples.append(monoVal);
        }
    });

    // Compute energy levels when decoding completes
    connect(decoder, &QAudioDecoder::finished, this, [this, decoder]() {
        if (!m_samples.isEmpty())
            computeEnergyLevels();
        decoder->deleteLater();
    });

    decoder->start();
}

void SpectrumWidget::clear()
{
    m_energyLevels.clear();
    m_samples.clear();
    m_currentPos = 0;
    m_duration = 0;
    m_loaded = false;
    update();
}

void SpectrumWidget::updatePosition(qint64 positionMs, qint64 durationMs)
{
    m_currentPos = positionMs;
    if (durationMs > 0)
        m_duration = durationMs;
    update();
}

void SpectrumWidget::computeEnergyLevels()
{
    m_energyLevels.clear();
    if (m_samples.isEmpty())
        return;

    int totalSamples = m_samples.size();
    int segSize = totalSamples / BAR_COUNT;
    if (segSize < 4) segSize = 4;

    m_energyLevels.reserve(BAR_COUNT);

    float maxEnergy = 0.0f;

    for (int i = 0; i < BAR_COUNT; i++) {
        int start = i * segSize;
        int end = start + segSize;
        if (end > totalSamples) end = totalSamples;

        // Compute RMS energy for this segment
        float sumSq = 0.0f;
        int count = end - start;
        for (int j = start; j < end; j++) {
            float v = m_samples[j];
            sumSq += v * v;
        }
        float rms = std::sqrt(sumSq / static_cast<float>(count));
        m_energyLevels.append(rms);
        if (rms > maxEnergy) maxEnergy = rms;
    }

    // Normalize
    if (maxEnergy > 0.0001f) {
        for (auto &e : m_energyLevels)
            e /= maxEnergy;
    }

    // Release raw samples — no longer needed
    m_samples.clear();
    m_loaded = true;
    update();
}

// ============================================================
// paintEvent
// ============================================================

void SpectrumWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    int w = width();
    int h = height();
    int margin = 2;
    int drawH = h - margin * 2;

    // Background handled by QSS

    if (m_energyLevels.isEmpty()) {
        // Empty state — draw subtle placeholder bars
        QColor emptyColor = palette().color(QPalette::Mid);
        for (int i = 0; i < BAR_COUNT; i++) {
            int x = static_cast<int>((static_cast<float>(i) / BAR_COUNT) * (w - margin * 2));
            int barW = qMax(1, (w - margin * 2) / BAR_COUNT);
            QRect barR(margin + x, h / 2 - 2, barW - 1, 4);
            p.fillRect(barR, emptyColor);
        }
        return;
    }

    // Determine playhead position
    int playheadX = 0;
    if (m_duration > 0) {
        float ratio = static_cast<float>(m_currentPos) / static_cast<float>(m_duration);
        ratio = std::clamp(ratio, 0.0f, 1.0f);
        playheadX = margin + static_cast<int>(ratio * (w - margin * 2));
    }

    int barW = qMax(1, (w - margin * 2) / BAR_COUNT);
    int gap = qMax(0, barW - 1);  // 1px gap between bars

    for (int i = 0; i < BAR_COUNT; i++) {
        float energy = m_energyLevels[i];
        int x = margin + i * barW;
        if (x + barW > w - margin) break;

        // Bar height: 20% minimum + 80% * energy
        int barH = static_cast<int>(drawH * 0.2f + drawH * 0.8f * energy);
        int y = (h - barH) / 2;

        // Color: proximity to playhead determines brightness
        int barCenterX = x + barW / 2;
        int dist = qAbs(barCenterX - playheadX);
        int maxDist = w / 3;
        float proximity = 1.0f - std::clamp(static_cast<float>(dist) / maxDist, 0.0f, 1.0f);

        QColor col;
        if (proximity > 0.6f) {
            // Near playhead: bright gradient
            int r = static_cast<int>(64 + (245 - 64) * proximity);
            int g = static_cast<int>(158 + (108 - 158) * (1.0f - proximity));
            int b = static_cast<int>(255 + (40 - 255) * (1.0f - proximity));
            col = QColor(r, g, b);
        } else {
            // Far from playhead: dim
            int gray = static_cast<int>(180 + 40 * proximity);
            col = QColor(gray, gray, gray + 10);
        }

        p.fillRect(x, y, barW - gap, barH, col);
    }

    // Draw playhead line
    if (m_duration > 0) {
        p.setPen(QPen(QColor(64, 158, 255, 180), 1));
        p.drawLine(playheadX, margin, playheadX, h - margin);
    }
}

void SpectrumWidget::resizeEvent(QResizeEvent *)
{
    update();
}
