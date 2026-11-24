#pragma once
#include "UiContext.h"
#include "core/Math.h"
#include <QWidget>
#include <QSet>
class QTableWidget; class QComboBox; class QSpinBox; class QLabel; class QListWidget; class QDoubleSpinBox; class QCheckBox; class QPlainTextEdit;

namespace lf {
class ImageView; class ColorButton;

class FixturePanel : public QWidget {
    Q_OBJECT
public:
    FixturePanel(UiContext* ctx, QWidget* parent = nullptr);
    void refresh();
private:
    UiContext* c; QTableWidget* m_table; QComboBox* m_type; QComboBox* m_mode; QSpinBox* m_count; QSpinBox* m_univ; QLabel* m_caps; QPlainTextEdit* m_issues;
    bool m_updating = false;
};

class PixelMapperPanel : public QWidget {
    Q_OBJECT
public:
    PixelMapperPanel(UiContext* ctx, QWidget* parent = nullptr);
    void refresh();
private:
    UiContext* c; ImageView* m_view; QTableWidget* m_table; QLabel* m_stats; QComboBox* m_test;
    bool m_updating = false;
};

// 3D stage view rendered with a perspective/orthographic software projection.
class StageView : public QWidget {
    Q_OBJECT
public:
    StageView(UiContext* ctx, QWidget* parent = nullptr);
    enum Camera { Perspective, Top, Front, Side };
    void setCamera(Camera cam) { m_cam = cam; update(); }
    QSet<int> selection() const { return m_sel; }
    void setSelection(const QSet<int>& s) { m_sel = s; update(); emit selectionChanged(); }
    bool showLabels = true;
    void orbit(double deg) { m_yaw += deg; update(); }
signals:
    void selectionChanged();
protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
private:
    QPointF project(const Vec3& p, double* depth = nullptr) const;
    UiContext* c; Camera m_cam = Perspective;
    double m_yaw = -35, m_pitch = 25, m_dist = 22; Vec3 m_target{0, 2, 2.5}; QPoint m_last; QSet<int> m_sel;
};

class StagePanel : public QWidget {
    Q_OBJECT
public:
    StagePanel(UiContext* ctx, QWidget* parent = nullptr);
private:
    void loadSelection();
    UiContext* c; StageView* m_view; QDoubleSpinBox* m_pos[3]; QDoubleSpinBox* m_rot[3]; QLabel* m_selLabel; bool m_updating = false;
};

class SpatialPanel : public QWidget {
    Q_OBJECT
public:
    SpatialPanel(UiContext* ctx, QWidget* parent = nullptr);
    void refresh();
private:
    void load(); void store();
    UiContext* c; QListWidget* m_list; QWidget* m_form;
    QComboBox *m_order, *m_wave, *m_group; QDoubleSpinBox *m_speed, *m_spread, *m_width, *m_offset, *m_delay, *m_low, *m_high, *m_level;
    QSpinBox *m_wings, *m_blocks, *m_groups, *m_seed; QCheckBox *m_enabled, *m_reverse, *m_bounce, *m_sweep; ColorButton *m_ca, *m_cb;
    bool m_updating = false;
};

} // namespace lf
