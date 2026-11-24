#pragma once
// Real-time audio analysis: FFT spectrum, six frequency bands, envelope
// followers, noise gate, onset/beat detection and BPM estimation.
// Capture backends feed blocks of mono float samples via process().
#include <QVector>
#include <QString>
#include <QStringList>
#include <QMutex>
#include <complex>
#include <vector>
#include <atomic>
#include <memory>
#include <functional>

namespace lf {

void fft(std::vector<std::complex<float>>& a);   // in-place radix-2, size power of two

struct AudioFeatures {
    float level = 0;               // RMS (post-gate, post-gain) 0..1
    float peak = 0;
    float bands[6] = {0};          // sub, bass, low-mid, mid, high-mid, treble (0..1 normalised, smoothed)
    bool beat = false;             // onset this block
    float beatPulse = 0;           // decays 1 -> 0 after each beat
    double bpm = 0;                // estimated tempo (0 if unknown)
    double bpmConfidence = 0;
    QVector<float> spectrum;       // 64 log-spaced bins 0..1
    quint64 beatCount = 0;
};

class AudioAnalyzer {
public:
    explicit AudioAnalyzer(int sampleRate = 48000, int fftSize = 1024);
    void process(const float* mono, int count);       // any block size
    AudioFeatures features() const;                   // thread-safe snapshot
    void setGain(float g) { m_gain = g; }
    void setNoiseGate(float g) { m_gate = g; }
    void setSmoothing(float s) { m_smooth = s; }
    void setSensitivity(float s) { m_sensitivity = s; }
    float gain() const { return m_gain; }
    int sampleRate() const { return m_sr; }
    static const char* bandName(int i);
    void reset();
private:
    void analyseFrame();
    int m_sr, m_n;
    std::vector<float> m_buf; int m_fill = 0;
    std::vector<float> m_window;
    std::vector<float> m_prevMag;
    std::vector<double> m_fluxHistory;
    std::vector<double> m_onsetTimes;
    double m_time = 0;
    double m_lastBeat = -10;
    float m_gain = 1.0f, m_gate = 0.01f, m_smooth = 0.6f, m_sensitivity = 1.4f;
    float m_bandMax[6] = {1e-3f, 1e-3f, 1e-3f, 1e-3f, 1e-3f, 1e-3f};
    mutable QMutex m_mutex;
    AudioFeatures m_f;
};

// Audio capture backend. Linux: ALSA. Also: WAV file playback and test tone.
class AudioInput {
public:
    virtual ~AudioInput() = default;
    virtual QString name() const = 0;
    virtual bool start(std::function<void(const float*, int)> cb) = 0;
    virtual void stop() = 0;
    virtual int sampleRate() const = 0;
    static QStringList devices();                   // available capture devices
    static std::unique_ptr<AudioInput> create(const QString& device); // "test:120", "file:/path.wav", "alsa:default"
};

bool readWav(const QString& path, std::vector<float>& mono, int& sampleRate, QString* err = nullptr);
bool writeWav(const QString& path, const std::vector<float>& mono, int sampleRate);

} // namespace lf
