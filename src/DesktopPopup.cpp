#include "DesktopPopup.h"
#include "PerfilVisual.h"
#include "x11util.h"

#include <QPushButton>
#include <QHBoxLayout>
#include <QTimer>
#include <QPainter>
#include <QPainterPath>
#include <QButtonGroup>
#include <QScreen>
#include <QGuiApplication>

namespace {
constexpr int DOTS_TAM  = 13;   // diámetro del punto (px)
constexpr int DOTS_ESP  = 12;   // separación entre puntos
constexpr int MARGEN    = 12;   // margen alrededor de la fila de puntos
}

DesktopPopup::DesktopPopup(QWidget* parent)
    : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint)
{
    if (PerfilVisual::ligero()) {
        setAttribute(Qt::WA_OpaquePaintEvent);
    } else {
        setAttribute(Qt::WA_TranslucentBackground);
    }

    setStyleSheet(R"(
QPushButton {
    background: rgba(255,255,255,75);
    border: none;
    border-radius: 6px;
    min-width: 12px; max-width: 12px;
    min-height: 12px; max-height: 12px;
    padding: 0;
}
QPushButton:hover  { background: rgba(255,255,255,160); }
QPushButton:checked { background: #3b82f6; }
)");

    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(MARGEN, MARGEN, MARGEN, MARGEN);
    m_layout->setSpacing(DOTS_ESP);

    // Mientras el popup está abierto se vigila el escritorio activo real
    // (cambios por teclado, clic en el escritorio, etc.) y se mueve el azul.
    m_timer = new QTimer(this);
    m_timer->setInterval(500);
    connect(m_timer, &QTimer::timeout, this, &DesktopPopup::refrescarActual);
}

void DesktopPopup::reconstruir() {
    // Limpia los puntos anteriores.
    while (QLayoutItem* it = m_layout->takeAt(0)) {
        if (QWidget* w = it->widget()) delete w;
        delete it;
    }
    m_botones.clear();

    const int n = qMax(1, x11_get_num_desktops());

    // Puntos autoexclusivos: el marcado (:checked) es el escritorio activo.
    auto* grupo = new QButtonGroup(this);
    grupo->setExclusive(true);
    for (int i = 0; i < n; i++) {
        auto* dot = new QPushButton(this);
        dot->setCursor(Qt::PointingHandCursor);
        dot->setCheckable(true);
        dot->setToolTip(QStringLiteral("Escritorio %1").arg(i + 1));
        grupo->addButton(dot, i);
        m_layout->addWidget(dot);
        m_botones.append(dot);
        connect(dot, &QPushButton::clicked, this, [i](){ x11_set_current_desktop(i); });
    }

    refrescarActual();

    setFixedSize(DOTS_TAM * n + DOTS_ESP * (n - 1) + MARGEN * 2,
                 DOTS_TAM + MARGEN * 2);
}

void DesktopPopup::refrescarActual() {
    if (m_botones.isEmpty()) return;
    const int actual = x11_get_current_desktop();
    for (int i = 0; i < m_botones.size(); i++)
        m_botones[i]->setChecked(i == actual);
}

void DesktopPopup::abrirSobre(const QRect& ancla, DockPosition pos) {
    reconstruir();

    const int gap = 8;
    QPoint p;
    switch (pos) {
    case DockPosition::Abajo:
        p = QPoint(ancla.center().x() - width() / 2, ancla.top() - height() - gap);
        break;
    case DockPosition::Arriba:
        p = QPoint(ancla.center().x() - width() / 2, ancla.bottom() + gap);
        break;
    case DockPosition::Izquierda:
        p = QPoint(ancla.right() + gap, ancla.center().y() - height() / 2);
        break;
    case DockPosition::Derecha:
        p = QPoint(ancla.left() - width() - gap, ancla.center().y() - height() / 2);
        break;
    }

    if (QScreen* scr = QGuiApplication::screenAt(ancla.center())) {
        QRect sg = scr->availableGeometry();
        p.setX(qBound(sg.left(), p.x(), sg.right() - width()  + 1));
        p.setY(qBound(sg.top(),  p.y(), sg.bottom() - height() + 1));
    }

    move(p);
    show();
    raise();
    m_timer->start();
}

void DesktopPopup::hideEvent(QHideEvent*) {
    m_timer->stop();
}

void DesktopPopup::paintEvent(QPaintEvent*) {
    QPainter pt(this);

    // Perfil ligero: fondo plano opaco, sin esquinas ni borde.
    if (PerfilVisual::ligero()) {
        pt.fillRect(rect(), QColor(20, 20, 26));
        return;
    }

    pt.setRenderHint(QPainter::Antialiasing, true);

    QPainterPath path;
    path.addRoundedRect(rect().adjusted(0, 0, -1, -1), 12, 12);

    pt.fillPath(path, QColor(20, 20, 26));
    pt.setPen(QPen(QColor(255, 255, 255, 42), 1));
    pt.drawPath(path);
}