#pragma once
#include "UiContext.h"
#include "output/Output.h"
#include "ai/EffectAssistant.h"
#include <QWidget>
#include <QPointer>
class QTableWidget; class QCheckBox; class QLineEdit; class QSpinBox; class QDoubleSpinBox; class QComboBox; class QLabel; class QListWidget; class QPlainTextEdit; class QPushButton;

namespace lf {
class LevelMeter; class HistoryGraph; class ParamSlider;

// Borderless fullscreen (or windowed) video output on a chosen screen.
class OutputWindow : public QWidget {
    Q_OBJECT
public:
    OutputWindow(Engine* e, int index, QWidget* parent = nullptr);
    void setRegion(const OutputRegion& r);
protected:
    void paintEvent(QPaintEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
private:
    Engine* m_e; int m_index; OutputRegion m_region;
};

class OutputPanel : public QWidget {
    Q_OBJECT
public:
    OutputPanel(UiContext* ctx, QWidget* parent = nullptr);
public slots:
    void loadConfig();
public:
    void refreshStats();
private:
    void applyNetwork();
    void loadStreams();
    void applyStreams();
    QTableWidget* m_streams = nullptr; QLabel* m_streamInfo = nullptr;
    UiContext* c;
    QCheckBox *m_anOut, *m_anIn, *m_saOut, *m_saIn, *m_oscIn; QLineEdit *m_anTarget, *m_saTargets, *m_outU, *m_inU, *m_oscFb; QSpinBox *m_oscPort, *m_prio, *m_offset;
    QDoubleSpinBox* m_rate; QTableWidget* m_diag; QTableWidget* m_outputs; QSpinBox *m_cw, *m_ch; QLabel* m_rec;
    QVector<QPointer<OutputWindow>> m_windows;
};

class MediaPanel : public QWidget {
    Q_OBJECT
public:
    MediaPanel(UiContext* ctx, QWidget* parent = nullptr);
    void refresh();
private:
    UiContext* c; QListWidget* m_list; QLabel* m_info;
};

class AudioPanel : public QWidget {
    Q_OBJECT
public:
    AudioPanel(UiContext* ctx, QWidget* parent = nullptr);
private:
    UiContext* c; LevelMeter* m_bands; LevelMeter* m_spec; QLabel* m_bpm; QComboBox* m_dev; QCheckBox* m_en;
    QDoubleSpinBox *m_gain, *m_gate, *m_smooth, *m_sens;
};

class CalibrationPanel : public QWidget {
    Q_OBJECT
public:
    CalibrationPanel(UiContext* ctx, QWidget* parent = nullptr);
private:
    UiContext* c;
};

class RemoteSyncPanel : public QWidget {
    Q_OBJECT
public:
    RemoteSyncPanel(UiContext* ctx, QWidget* parent = nullptr);
private:
    UiContext* c; QTableWidget* m_users; QTableWidget* m_clients; QLabel* m_sync; QLabel* m_url;
};

class PluginPanel : public QWidget {
    Q_OBJECT
public:
    PluginPanel(UiContext* ctx, QWidget* parent = nullptr);
    void refresh();
private:
    UiContext* c; QTableWidget* m_table;
};

class AiPanel : public QWidget {
    Q_OBJECT
public:
    AiPanel(UiContext* ctx, QWidget* parent = nullptr);
private:
    void show(const AiSuggestion& s);
    UiContext* c; QLineEdit* m_prompt; QPlainTextEdit* m_out; QPushButton* m_apply; AiSuggestion m_last; bool m_validated = false;
};

class PerformancePanel : public QWidget {
    Q_OBJECT
public:
    PerformancePanel(UiContext* ctx, QWidget* parent = nullptr);
private:
    UiContext* c; HistoryGraph *m_fps, *m_ms; QLabel* m_info;
};

class RecoveryPanel : public QWidget {
    Q_OBJECT
public:
    RecoveryPanel(UiContext* ctx, QWidget* parent = nullptr);
    void refresh();
private:
    UiContext* c; QTableWidget* m_faults; QListWidget* m_backups; QPlainTextEdit* m_log; quint64 m_rev = 0, m_logSeq = 0;
};

} // namespace lf
