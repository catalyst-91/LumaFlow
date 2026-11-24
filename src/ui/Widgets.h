#pragma once
// Shared UI widgets: zoomable image view with overlays, labelled parameter
// slider bound to the parameter registry, colour button, level meter, graph.
#include <QWidget>
#include <QImage>
#include <QColor>
#include <QPointer>
#include <QVector>
#include <functional>
#include <array>

class QSlider; class QLabel; class QDoubleSpinBox; class QUndoStack;

namespace lf {
class Engine;

class ImageView : public QWidget {
    Q_OBJECT
public:
    explicit ImageView(const QString& title, QWidget* parent = nullptr);
    void setImage(const QImage& img);
    void setOverlayPainter(std::function<void(QPainter&, const QRectF& target)> fn) { m_overlay = std::move(fn); update(); }
    void setShowSafeArea(bool on) { m_safe = on; update(); }
    void setShowGrid(bool on) { m_grid = on; update(); }
    void setBorderColor(const QColor& c) { m_border = c; update(); }
    void resetView() { m_zoom = 1.0; m_pan = {}; update(); }
    QRectF imageRect() const;
    QSize sizeHint() const override { return {480, 270}; }
signals:
    void clicked(QPointF normalized);
protected:
    void paintEvent(QPaintEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    bool event(QEvent*) override;   // pinch gestures
private:
    QString m_title; QImage m_img; QColor m_border{60, 60, 80};
    double m_zoom = 1.0; QPointF m_pan; QPoint m_last; bool m_safe = false, m_grid = false;
    std::function<void(QPainter&, const QRectF&)> m_overlay;
};

// Slider + spin box editing one registry parameter; writes go through the
// mapping engine's ownership policy (source = touch) and the undo stack.
class ParamSlider : public QWidget {
    Q_OBJECT
public:
    ParamSlider(Engine* e, const QString& paramId, const QString& label, QUndoStack* undo, QWidget* parent = nullptr);
    void setParam(const QString& paramId, const QString& label);
    void refresh();
    QString paramId() const { return m_id; }
private:
    Engine* m_e; QString m_id; QUndoStack* m_undo;
    QLabel* m_label; QSlider* m_slider; QDoubleSpinBox* m_spin;
    bool m_updating = false; double m_pressValue = 0;
};

class ColorButton : public QWidget {
    Q_OBJECT
public:
    explicit ColorButton(const QString& label, QWidget* parent = nullptr);
    void setColor(const std::array<double, 3>& c);
    std::array<double, 3> color() const { return m_c; }
signals:
    void colorChanged(std::array<double, 3> c);
protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    QSize sizeHint() const override { return {90, 26}; }
private:
    QString m_label; std::array<double, 3> m_c{1, 1, 1};
};

class LevelMeter : public QWidget {
public:
    explicit LevelMeter(QWidget* parent = nullptr) : QWidget(parent) { setMinimumHeight(60); }
    void setLevels(const QVector<float>& v, const QStringList& labels = {}) { m_v = v; m_labels = labels; update(); }
protected:
    void paintEvent(QPaintEvent*) override;
private:
    QVector<float> m_v; QStringList m_labels;
};

class HistoryGraph : public QWidget {
public:
    explicit HistoryGraph(const QString& title, double maxValue, QWidget* parent = nullptr) : QWidget(parent), m_title(title), m_max(maxValue) { setMinimumHeight(70); }
    void push(double v) { m_v << v; if (m_v.size() > 240) m_v.removeFirst(); update(); }
    void setReference(double r) { m_ref = r; }
protected:
    void paintEvent(QPaintEvent*) override;
private:
    QString m_title; double m_max; double m_ref = -1; QVector<double> m_v;
};

QString darkStyleSheet();
QIcon appIcon();

} // namespace lf
