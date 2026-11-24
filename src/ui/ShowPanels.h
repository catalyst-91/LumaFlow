#pragma once
#include "UiContext.h"
#include <QWidget>
#include <QHash>
#include <QPixmap>
class QListWidget; class QComboBox; class QLineEdit; class QTableWidget; class QLabel; class QVBoxLayout; class QSlider; class QPushButton; class QCheckBox; class QDoubleSpinBox;

namespace lf {
class ParamSlider; class ColorButton; class ImageView;

class LibraryPanel : public QWidget {
    Q_OBJECT
public:
    LibraryPanel(UiContext* ctx, QWidget* parent = nullptr);
    void setThumbnails(const QHash<QString, QVector<QImage>>& t);
    void refresh();
private:
    void animate();
    UiContext* c; QListWidget* m_list; QComboBox* m_cat; QLineEdit* m_search;
    QHash<QString, QVector<QImage>> m_thumbs; int m_frame = 0;
};

class LayerPanel : public QWidget {
    Q_OBJECT
public:
    LayerPanel(UiContext* ctx, QWidget* parent = nullptr);
    void refresh();
private:
    UiContext* c; QTableWidget* m_table; bool m_updating = false;
};

class ParameterPanel : public QWidget {
    Q_OBJECT
public:
    ParameterPanel(UiContext* ctx, QWidget* parent = nullptr);
    void rebuild();
    void refreshValues();
private:
    UiContext* c; QVBoxLayout* m_box; QLabel* m_title; QVector<ParamSlider*> m_sliders;
    ColorButton* m_colA = nullptr; ColorButton* m_colB = nullptr; QLineEdit* m_text = nullptr;
    QComboBox* m_mask = nullptr; QDoubleSpinBox* m_maskSize = nullptr; QComboBox* m_presets = nullptr;
    QWidget* m_inner = nullptr;
};

class ShowPanel : public QWidget {
    Q_OBJECT
public:
    ShowPanel(UiContext* ctx, QWidget* parent = nullptr);
    void refresh();
    void refreshStatus();
private:
    UiContext* c; QListWidget* m_scenes; QTableWidget* m_cues; QLabel* m_bpm; QLabel* m_tc; QLabel* m_trans;
    ParamSlider* m_master; ParamSlider* m_transTime; QPushButton* m_blackout;
};

class PreviewPanel : public QWidget {
    Q_OBJECT
public:
    PreviewPanel(UiContext* ctx, QWidget* parent = nullptr);
    void updateFrames();
    ImageView* programView() const { return m_program; }
private:
    UiContext* c; ImageView* m_preview; ImageView* m_program; QLabel* m_stats; QCheckBox* m_overlayPix;
};

} // namespace lf
