#include "AudioAnalyzer.h"
#include "core/Log.h"
#include <QFile>
#include <QDataStream>
#include <cmath>
#include <cstring>
#include <thread>
#include <chrono>
#include <map>
#include <algorithm>
#ifdef LF_HAVE_ALSA
#include <alsa/asoundlib.h>
#endif

namespace lf {

void fft(std::vector<std::complex<float>>& a) {
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        const float ang = -2.0f * float(M_PI) / float(len);
        const std::complex<float> wl(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len) {
            std::complex<float> w(1);
            for (size_t k = 0; k < len / 2; ++k) {
                const auto u = a[i + k], v = a[i + k + len / 2] * w;
                a[i + k] = u + v; a[i + k + len / 2] = u - v; w *= wl;
            }
        }
    }
}

static const float kBandEdges[7] = {20, 60, 250, 500, 2000, 6000, 16000};
const char* AudioAnalyzer::bandName(int i) { static const char* n[] = {"Sub", "Bass", "Low-mid", "Mid", "High-mid", "Treble"}; return n[std::clamp(i, 0, 5)]; }

AudioAnalyzer::AudioAnalyzer(int sr, int n) : m_sr(sr), m_n(n), m_buf(n, 0.f), m_window(n), m_prevMag(n / 2, 0.f) {
    for (int i = 0; i < n; ++i) m_window[i] = 0.5f - 0.5f * std::cos(2 * float(M_PI) * i / (n - 1));
    m_f.spectrum.resize(64);
}

void AudioAnalyzer::reset() {
    QMutexLocker l(&m_mutex);
    m_f = AudioFeatures{}; m_f.spectrum.resize(64);
    m_fluxHistory.clear(); m_onsetTimes.clear(); m_fill = 0; m_time = 0; m_lastBeat = -10;
    std::fill(m_prevMag.begin(), m_prevMag.end(), 0.f);
}

void AudioAnalyzer::process(const float* s, int count) {
    // 50% overlap: analyse every n/2 samples
    for (int i = 0; i < count; ++i) {
        m_buf[m_fill++] = s[i] * m_gain;
        if (m_fill == m_n) {
            analyseFrame();
            std::copy(m_buf.begin() + m_n / 2, m_buf.end(), m_buf.begin());
            m_fill = m_n / 2;
            m_time += double(m_n / 2) / m_sr;
        }
    }
}

void AudioAnalyzer::analyseFrame() {
    std::vector<std::complex<float>> a(m_n);
    double rms = 0; float peak = 0;
    for (int i = 0; i < m_n; ++i) { a[i] = m_buf[i] * m_window[i]; rms += m_buf[i] * m_buf[i]; peak = std::max(peak, std::abs(m_buf[i])); }
    rms = std::sqrt(rms / m_n);
    const bool gated = rms < m_gate;
    fft(a);
    const int half = m_n / 2;
    std::vector<float> mag(half);
    for (int k = 0; k < half; ++k) mag[k] = gated ? 0.f : std::abs(a[k]) / (m_n / 4.0f);
    // spectral flux (positive differences) for onset detection, weighted toward low end
    double flux = 0;
    const float hz = float(m_sr) / m_n;
    for (int k = 1; k < half; ++k) {
        const float d = mag[k] - m_prevMag[k];
        if (d > 0) flux += d * (k * hz < 250 ? 2.0 : 1.0);
    }
    m_prevMag = mag;
    m_fluxHistory.push_back(flux);
    const size_t hist = size_t(1.0 * m_sr / (m_n / 2));   // ~1 s
    while (m_fluxHistory.size() > hist) m_fluxHistory.erase(m_fluxHistory.begin());
    double mean = 0; for (double f : m_fluxHistory) mean += f; mean /= std::max<size_t>(1, m_fluxHistory.size());
    double var = 0; for (double f : m_fluxHistory) var += (f - mean) * (f - mean); var /= std::max<size_t>(1, m_fluxHistory.size());
    const bool beat = !gated && flux > mean * m_sensitivity + std::sqrt(var) * 0.5 && flux > 1e-3 && (m_time - m_lastBeat) > 0.25;
    if (beat) {
        m_lastBeat = m_time; m_onsetTimes.push_back(m_time);
        while (!m_onsetTimes.empty() && m_time - m_onsetTimes.front() > 8.0) m_onsetTimes.erase(m_onsetTimes.begin());
    }
    // BPM: histogram of inter-onset intervals (incl. 2-step) folded into 70..180 BPM
    double bpm = 0, conf = 0;
    if (m_onsetTimes.size() >= 4) {
        std::map<int, double> votes;
        for (size_t i = 1; i < m_onsetTimes.size(); ++i) for (size_t j = i; j > 0 && j + 2 > i; --j) {
            double ioi = m_onsetTimes[i] - m_onsetTimes[j - 1];
            if (ioi <= 0) continue;
            double b = 60.0 / ioi;
            while (b < 70) b *= 2; while (b > 180) b /= 2;
            const int key = int(std::lround(b));
            votes[key] += 1.0; votes[key - 1] += 0.5; votes[key + 1] += 0.5;
        }
        double best = 0, total = 0; int bk = 0;
        for (auto& [k, v] : votes) { total += v; if (v > best) { best = v; bk = k; } }
        if (best > 0) {
            // refine with weighted mean of neighbours
            const double w0 = votes[bk - 1], w1 = votes[bk], w2 = votes[bk + 1];
            bpm = (bk - 1) * w0 / (w0 + w1 + w2) + bk * w1 / (w0 + w1 + w2) + (bk + 1) * w2 / (w0 + w1 + w2);
            conf = best / total;
        }
    }
    // bands
    float bands[6] = {0};
    for (int b = 0; b < 6; ++b) {
        const int k0 = std::max(1, int(kBandEdges[b] / hz)), k1 = std::min(half - 1, int(kBandEdges[b + 1] / hz));
        float s = 0; for (int k = k0; k <= k1; ++k) s += mag[k] * mag[k];
        bands[b] = std::sqrt(s / std::max(1, k1 - k0 + 1));
        m_bandMax[b] = std::max(m_bandMax[b] * 0.9995f, bands[b]);   // slow auto-gain
        bands[b] = std::min(1.0f, bands[b] / std::max(1e-4f, m_bandMax[b]));
    }
    QVector<float> spec(64);
    for (int i = 0; i < 64; ++i) {
        const float f0 = 20.f * std::pow(800.f, i / 64.f), f1 = 20.f * std::pow(800.f, (i + 1) / 64.f);
        const int k0 = std::max(1, int(f0 / hz)), k1 = std::max(k0, std::min(half - 1, int(f1 / hz)));
        float m = 0; for (int k = k0; k <= k1; ++k) m = std::max(m, mag[k]);
        spec[i] = std::min(1.f, std::log10(1 + 9 * std::min(1.f, m * 4)));
    }
    QMutexLocker l(&m_mutex);
    const float sm = m_smooth;
    m_f.level = float(m_f.level * sm + (gated ? 0 : rms) * (1 - sm));
    m_f.peak = peak;
    for (int b = 0; b < 6; ++b) m_f.bands[b] = m_f.bands[b] * sm + bands[b] * (1 - sm);
    m_f.beat = beat;
    m_f.beatPulse = beat ? 1.f : m_f.beatPulse * 0.85f;
    if (beat) ++m_f.beatCount;
    if (bpm > 0) { m_f.bpm = m_f.bpm > 0 ? m_f.bpm * 0.8 + bpm * 0.2 : bpm; m_f.bpmConfidence = conf; }
    for (int i = 0; i < 64; ++i) m_f.spectrum[i] = m_f.spectrum[i] * sm + spec[i] * (1 - sm);
}

AudioFeatures AudioAnalyzer::features() const { QMutexLocker l(&m_mutex); return m_f; }

// ---------------------------------------------------------------- WAV I/O
bool readWav(const QString& path, std::vector<float>& mono, int& sr, QString* err) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) { if (err) *err = "cannot open"; return false; }
    const QByteArray d = f.readAll();
    if (d.size() < 44 || !d.startsWith("RIFF") || d.mid(8, 4) != "WAVE") { if (err) *err = "not a RIFF/WAVE file"; return false; }
    int pos = 12, channels = 1, bits = 16, fmt = 1;
    auto u16 = [&](int o) { return int(quint8(d[o]) | (quint8(d[o + 1]) << 8)); };
    auto u32 = [&](int o) { return quint32(u16(o)) | (quint32(u16(o + 2)) << 16); };
    while (pos + 8 <= d.size()) {
        const QByteArray id = d.mid(pos, 4); const int sz = int(u32(pos + 4)); const int body = pos + 8;
        if (id == "fmt ") { fmt = u16(body); channels = u16(body + 2); sr = int(u32(body + 4)); bits = u16(body + 14); }
        else if (id == "data") {
            const int bps = bits / 8, frames = std::min(sz, int(d.size()) - body) / (bps * channels);
            mono.resize(frames);
            for (int i = 0; i < frames; ++i) {
                float acc = 0;
                for (int c = 0; c < channels; ++c) {
                    const int o = body + (i * channels + c) * bps;
                    if (fmt == 3 && bits == 32) { float v; std::memcpy(&v, d.constData() + o, 4); acc += v; }
                    else if (bits == 16) acc += qint16(u16(o)) / 32768.f;
                    else if (bits == 24) acc += (qint32((quint8(d[o]) << 8) | (quint8(d[o + 1]) << 16) | (quint8(d[o + 2]) << 24)) >> 8) / 8388608.f;
                    else if (bits == 8) acc += (quint8(d[o]) - 128) / 128.f;
                }
                mono[i] = acc / channels;
            }
            return true;
        }
        pos = body + sz + (sz & 1);
    }
    if (err) *err = "no data chunk";
    return false;
}

bool writeWav(const QString& path, const std::vector<float>& mono, int sr) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    QDataStream s(&f); s.setByteOrder(QDataStream::LittleEndian);
    const quint32 dataBytes = quint32(mono.size() * 2);
    f.write("RIFF"); s << quint32(36 + dataBytes); f.write("WAVEfmt "); s << quint32(16) << quint16(1) << quint16(1) << quint32(sr) << quint32(sr * 2) << quint16(2) << quint16(16);
    f.write("data"); s << dataBytes;
    for (float v : mono) s << qint16(std::clamp(v, -1.f, 1.f) * 32767);
    return true;
}

// ---------------------------------------------------------------- inputs
class ThreadedInput : public AudioInput {
public:
    ~ThreadedInput() override { stop(); }
    bool start(std::function<void(const float*, int)> cb) override {
        stop(); m_cb = std::move(cb); m_run = true;
        m_thread = std::thread([this] { run(); });
        return true;
    }
    void stop() override { m_run = false; if (m_thread.joinable()) m_thread.join(); }
protected:
    virtual void run() = 0;
    std::function<void(const float*, int)> m_cb;
    std::atomic<bool> m_run{false};
    std::thread m_thread;
};

// Synthetic test signal: kick drum at given BPM + hi-hat + bass tone (for demos/tests without hardware).
class TestToneInput : public ThreadedInput {
public:
    explicit TestToneInput(double bpm) : m_bpm(bpm) {}
    QString name() const override { return QString("Test signal (%1 BPM)").arg(m_bpm); }
    int sampleRate() const override { return 48000; }
    static std::vector<float> generate(double bpm, double seconds, int sr, double t0 = 0) {
        std::vector<float> out(size_t(seconds * sr));
        const double beat = 60.0 / bpm;
        for (size_t i = 0; i < out.size(); ++i) {
            const double t = t0 + double(i) / sr, ph = std::fmod(t, beat), ph8 = std::fmod(t, beat / 2);
            double v = 0.8 * std::sin(2 * M_PI * (50 + 100 * std::exp(-ph * 30)) * ph) * std::exp(-ph * 9);   // kick
            v += 0.08 * std::sin(2 * M_PI * 110 * t) * (0.5 + 0.5 * std::sin(2 * M_PI * t / (beat * 4)));        // bass
            v += 0.05 * ((std::rand() / double(RAND_MAX)) * 2 - 1) * std::exp(-ph8 * 60);                         // hat
            out[i] = float(v);
        }
        return out;
    }
protected:
    void run() override {
        const int block = 480; double t = 0;
        auto next = std::chrono::steady_clock::now();
        while (m_run) {
            auto b = generate(m_bpm, block / 48000.0, 48000, t); t += block / 48000.0;
            m_cb(b.data(), int(b.size()));
            next += std::chrono::microseconds(10000);
            std::this_thread::sleep_until(next);
        }
    }
    double m_bpm;
};

class FileInput : public ThreadedInput {
public:
    explicit FileInput(const QString& p) : m_path(p) { QString e; if (!readWav(p, m_data, m_sr, &e)) logWarn("audio", "WAV load failed: " + e); }
    QString name() const override { return "File: " + m_path; }
    int sampleRate() const override { return m_sr; }
protected:
    void run() override {
        if (m_data.empty()) return;
        const int block = std::max(1, m_sr / 100); size_t pos = 0;
        auto next = std::chrono::steady_clock::now();
        while (m_run) {
            const int n = int(std::min<size_t>(block, m_data.size() - pos));
            m_cb(m_data.data() + pos, n);
            pos += n; if (pos >= m_data.size()) pos = 0;      // loop
            next += std::chrono::microseconds(10000);
            std::this_thread::sleep_until(next);
        }
    }
    QString m_path; std::vector<float> m_data; int m_sr = 48000;
};

#ifdef LF_HAVE_ALSA
class AlsaInput : public ThreadedInput {
public:
    explicit AlsaInput(const QString& dev) : m_dev(dev) {}
    QString name() const override { return "ALSA: " + m_dev; }
    int sampleRate() const override { return 48000; }
protected:
    void run() override {
        snd_pcm_t* pcm = nullptr;
        if (snd_pcm_open(&pcm, m_dev.toUtf8().constData(), SND_PCM_STREAM_CAPTURE, 0) < 0) { logWarn("audio", "ALSA: cannot open capture device " + m_dev); return; }
        if (snd_pcm_set_params(pcm, SND_PCM_FORMAT_FLOAT_LE, SND_PCM_ACCESS_RW_INTERLEAVED, 1, 48000, 1, 20000) < 0) { logWarn("audio", "ALSA: unsupported params"); snd_pcm_close(pcm); return; }
        std::vector<float> buf(480);
        while (m_run) {
            snd_pcm_sframes_t n = snd_pcm_readi(pcm, buf.data(), buf.size());
            if (n < 0) { n = snd_pcm_recover(pcm, int(n), 1); if (n < 0) break; continue; }
            m_cb(buf.data(), int(n));
        }
        snd_pcm_close(pcm);
    }
    QString m_dev;
};
#endif

QStringList AudioInput::devices() {
    QStringList d{"test:120", "test:128"};
#ifdef LF_HAVE_ALSA
    void** hints = nullptr;
    if (snd_device_name_hint(-1, "pcm", &hints) == 0) {
        for (void** h = hints; *h; ++h) {
            char* name = snd_device_name_get_hint(*h, "NAME"); char* io = snd_device_name_get_hint(*h, "IOID");
            if (name && (!io || QString(io) == "Input")) d << QString("alsa:%1").arg(name);
            free(name); free(io);
        }
        snd_device_name_free_hint(hints);
    }
#endif
    return d;
}

std::unique_ptr<AudioInput> AudioInput::create(const QString& dev) {
    if (dev.startsWith("test:")) return std::make_unique<TestToneInput>(dev.mid(5).toDouble() > 0 ? dev.mid(5).toDouble() : 120.0);
    if (dev.startsWith("file:")) return std::make_unique<FileInput>(dev.mid(5));
#ifdef LF_HAVE_ALSA
    if (dev.startsWith("alsa:")) return std::make_unique<AlsaInput>(dev.mid(5));
#endif
    return nullptr;
}

} // namespace lf
