#include "Widgets.h"
#include "UndoCommands.h"
#include "app/Engine.h"
#include <QPainter>
#include <QPainterPath>
#include <QSlider>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QWheelEvent>
#include <QColorDialog>
#include <QGestureEvent>
#include <QPinchGesture>
#include <QUndoStack>
#include <QFile>

namespace lf {

QIcon appIcon() { return QIcon(":/lumaflow/icons/lumaflow.png"); }

QString darkStyleSheet() {
    QFile f(":/lumaflow/themes/dark.qss");
    return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString();
}

// ---------------------------------------------------------------- ImageView
ImageView::ImageView(const QString& title, QWidget* parent) : QWidget(parent), m_title(title) {
    setMinimumSize(160, 90);
    setAttribute(Qt::WA_OpaquePaintEvent);
    grabGesture(Qt::PinchGesture);
    setMouseTracking(false);
}

void ImageView::setImage(const QImage& img) { m_img = img; update(); }

QRectF ImageView::imageRect() const {
    const QSizeF is = m_img.isNull() ? QSizeF(16, 9) : QSizeF(m_img.size());
    const QRectF area = QRectF(rect()).adjusted(4, 22, -4, -4);
    QSizeF s = is.scaled(area.size(), Qt::KeepAspectRatio) * m_zoom;
    return QRectF(area.center() - QPointF(s.width() / 2, s.height() / 2) + m_pan, s);
}

void ImageView::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), QColor(14, 14, 20));
    const QRectF r = imageRect();
    p.save(); p.setClipRect(QRectF(rect()).adjusted(4, 22, -4, -4));
    if (!m_img.isNull()) { p.setRenderHint(QPainter::SmoothPixmapTransform, m_zoom < 3); p.drawImage(r, m_img); }
    else p.fillRect(r, Qt::black);
    if (m_grid && !m_img.isNull()) {
        p.setPen(QColor(255, 255, 255, 40));
        const int step = std::max(4, int(m_img.width() / 32));
        for (int x = 0; x <= m_img.width(); x += step) { const double xx = r.left() + r.width() * x / m_img.width(); p.drawLine(QPointF(xx, r.top()), QPointF(xx, r.bottom())); }
        for (int y = 0; y <= m_img.height(); y += step) { const double yy = r.top() + r.height() * y / m_img.height(); p.drawLine(QPointF(r.left(), yy), QPointF(r.right(), yy)); }
    }
    if (m_safe) {
        p.setPen(QPen(QColor(255, 200, 0, 160), 1, Qt::DashLine));
        p.drawRect(r.adjusted(r.width() * 0.05, r.height() * 0.05, -r.width() * 0.05, -r.height() * 0.05));
        p.drawRect(r.adjusted(r.width() * 0.1, r.height() * 0.1, -r.width() * 0.1, -r.height() * 0.1));
    }
    if (m_overlay) m_overlay(p, r);
    p.restore();
    p.setPen(QPen(m_border, 2)); p.drawRect(QRectF(rect()).adjusted(1, 1, -1, -1));
    p.setPen(QColor(200, 210, 255)); QFont f = font(); f.setBold(true); p.setFont(f);
    p.drawText(QRect(8, 2, width() - 16, 18), Qt::AlignLeft | Qt::AlignVCenter, m_title);
    if (m_zoom != 1.0) p.drawText(QRect(8, 2, width() - 16, 18), Qt::AlignRight | Qt::AlignVCenter, QString("%1%").arg(int(m_zoom * 100)));
}

void ImageView::wheelEvent(QWheelEvent* e) { m_zoom = std::clamp(m_zoom * (e->angleDelta().y() > 0 ? 1.15 : 1 / 1.15), 0.25, 16.0); update(); }
void ImageView::mousePressEvent(QMouseEvent* e) {
    m_last = e->pos();
    const QRectF r = imageRect();
    if (r.contains(e->position())) emit clicked(QPointF((e->position().x() - r.left()) / r.width(), (e->position().y() - r.top()) / r.height()));
}
void ImageView::mouseMoveEvent(QMouseEvent* e) { if (e->buttons() & (Qt::MiddleButton | Qt::RightButton | Qt::LeftButton) && m_zoom > 1.0) { m_pan += e->pos() - m_last; m_last = e->pos(); update(); } }
void ImageView::mouseDoubleClickEvent(QMouseEvent*) { resetView(); }
bool ImageView::event(QEvent* e) {
    if (e->type() == QEvent::Gesture) {
        if (auto* g = static_cast<QPinchGesture*>(static_cast<QGestureEvent*>(e)->gesture(Qt::PinchGesture))) { m_zoom = std::clamp(m_zoom * g->scaleFactor(), 0.25, 16.0); update(); }
        return true;
    }
    return QWidget::event(e);
}

// ---------------------------------------------------------------- ParamSlider
ParamSlider::ParamSlider(Engine* e, const QString& id, const QString& label, QUndoStack* undo, QWidget* parent)
    : QWidget(parent), m_e(e), m_undo(undo) {
    auto* l = new QHBoxLayout(this); l->setContentsMargins(0, 1, 0, 1); l->setSpacing(6);
    m_label = new QLabel(label); m_label->setMinimumWidth(96);
    m_slider = new QSlider(Qt::Horizontal); m_slider->setRange(0, 1000); m_slider->setMinimumHeight(22);
    m_spin = new QDoubleSpinBox; m_spin->setDecimals(3); m_spin->setFixedWidth(78); m_spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    l->addWidget(m_label); l->addWidget(m_slider, 1); l->addWidget(m_spin);
    connect(m_slider, &QSlider::sliderPressed, this, [this] { m_pressValue = m_e->params().value(m_id); });
    connect(m_slider, &QSlider::valueChanged, this, [this](int v) {
        if (m_updating) return;
        const double before = m_e->params().value(m_id);
        m_e->mapping().write(m_id, v / 1000.0, ControlSource::Touch);
        const double after = m_e->params().value(m_id);
        if (m_undo && before != after) m_undo->push(new ParamCommand(m_e, m_id, before, after));
    });
    connect(m_spin, &QDoubleSpinBox::editingFinished, this, [this] {
        const double before = m_e->params().value(m_id);
        const ParameterDef d = m_e->params().def(m_id);
        m_e->mapping().write(m_id, d.normalize(m_spin->value()), ControlSource::Touch);
        if (m_undo) m_undo->push(new ParamCommand(m_e, m_id, before, m_e->params().value(m_id)));
    });
    setParam(id, label);
}

void ParamSlider::setParam(const QString& id, const QString& label) {
    m_id = id; m_label->setText(label);
    const ParameterDef d = m_e->params().def(id);
    m_updating = true; m_spin->setRange(std::min(d.minimum, d.maximum), std::max(d.minimum, d.maximum)); m_updating = false;
    setToolTip(QString("%1\nid: %2  range %3..%4").arg(d.name, id).arg(d.minimum).arg(d.maximum));
    refresh();
}

void ParamSlider::refresh() {
    if (m_slider->isSliderDown()) return;
    const ParameterDef d = m_e->params().def(m_id);
    const double v = m_e->params().value(m_id);
    m_updating = true;
    m_slider->setValue(int(std::lround(d.normalize(v) * 1000)));
    if (!m_spin->hasFocus()) m_spin->setValue(v);
    m_updating = false;
}

// ---------------------------------------------------------------- ColorButton
ColorButton::ColorButton(const QString& label, QWidget* parent) : QWidget(parent), m_label(label) { setMinimumSize(80, 24); setCursor(Qt::PointingHandCursor); }
void ColorButton::setColor(const std::array<double, 3>& c) { m_c = c; update(); }
void ColorButton::paintEvent(QPaintEvent*) {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    const QColor c = QColor::fromRgbF(float(m_c[0]), float(m_c[1]), float(m_c[2]));
    p.setBrush(c); p.setPen(QPen(QColor(120, 120, 160), 1)); p.drawRoundedRect(QRectF(rect()).adjusted(1, 1, -1, -1), 4, 4);
    p.setPen(c.lightnessF() > 0.55 ? Qt::black : Qt::white); p.drawText(rect(), Qt::AlignCenter, m_label);
}
void ColorButton::mousePressEvent(QMouseEvent*) {
    const QColor c = QColorDialog::getColor(QColor::fromRgbF(float(m_c[0]), float(m_c[1]), float(m_c[2])), this, m_label);
    if (!c.isValid()) return;
    m_c = {c.redF(), c.greenF(), c.blueF()}; update(); emit colorChanged(m_c);
}

void LevelMeter::paintEvent(QPaintEvent*) {
    QPainter p(this); p.fillRect(rect(), QColor(16, 16, 24));
    if (m_v.isEmpty()) return;
    const double w = double(width()) / m_v.size();
    for (int i = 0; i < m_v.size(); ++i) {
        const double h = std::clamp(double(m_v[i]), 0.0, 1.0) * (height() - (m_labels.isEmpty() ? 2 : 14));
        QLinearGradient g(0, height(), 0, 0); g.setColorAt(0, QColor(40, 120, 255)); g.setColorAt(1, QColor(170, 80, 255));
        p.fillRect(QRectF(i * w + 1, height() - (m_labels.isEmpty() ? 0 : 12) - h, w - 2, h), g);
        if (i < m_labels.size()) { p.setPen(QColor(170, 170, 200)); p.drawText(QRectF(i * w, height() - 12, w, 12), Qt::AlignCenter, m_labels[i]); }
    }
}

void HistoryGraph::paintEvent(QPaintEvent*) {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing); p.fillRect(rect(), QColor(16, 16, 24));
    if (m_ref >= 0) { p.setPen(QPen(QColor(255, 180, 0, 120), 1, Qt::DashLine)); const double y = height() - m_ref / m_max * height(); p.drawLine(QPointF(0, y), QPointF(width(), y)); }
    QPainterPath path;
    for (int i = 0; i < m_v.size(); ++i) { const QPointF pt(double(i) / 239 * width(), height() - std::min(1.0, m_v[i] / m_max) * (height() - 2)); if (i == 0) path.moveTo(pt); else path.lineTo(pt); }
    p.setPen(QPen(QColor(80, 160, 255), 1.5)); p.drawPath(path);
    p.setPen(QColor(190, 190, 220));
    p.drawText(rect().adjusted(6, 2, -6, -2), Qt::AlignTop | Qt::AlignLeft, m_title + (m_v.isEmpty() ? QString() : QString(": %1").arg(m_v.last(), 0, 'f', 1)));
}

} // namespace lf
