#include "VentanaPopup.h"
#include "PerfilVisual.h"
#include "x11util.h"
#include "x11windowutils.h"

#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QPainter>
#include <QPainterPath>
#include <QFrame>
#include <QScreen>
#include <QGuiApplication>

VentanaPopup::VentanaPopup(QWidget* parent)
    : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint)
{
    if (PerfilVisual::ligero()) {
        setAttribute(Qt::WA_OpaquePaintEvent);
    } else {
        setAttribute(Qt::WA_TranslucentBackground);
    }

    setStyleSheet(R"(
QLabel { color: #eee; background: transparent; border: none; }
QLabel[section="true"] {
    color: #89b4fa; font-weight: bold; font-size: 12px;
    padding: 4px 4px 2px 4px;
}
QPushButton {
    color: #eee; background-color: transparent;
    border: none; border-radius: 6px;
    padding: 6px 10px; font-size: 13px;
    text-align: left; icon-size: 20px;
}
QPushButton:hover   { background-color: rgba(137,180,250,45); }
QPushButton:pressed { background-color: rgba(137,180,250,80); }
QPushButton[activa="true"] { background-color: rgba(255,255,255,32); }
QScrollArea, QScrollArea > QWidget > QWidget { background: transparent; border: none; }
QScrollBar:vertical {
    background: transparent; width: 8px; margin: 0; border: none;
}
QScrollBar::handle:vertical {
    background: rgba(255,255,255,45); border-radius: 4px; min-height: 24px;
}
QScrollBar::handle:vertical:hover { background: rgba(137,180,250,90); }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
)");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(10, 10, 10, 10);
    root->setSpacing(4);
    setFixedWidth(300);

    m_area = new QScrollArea(this);
    m_area->setWidgetResizable(true);
    m_area->setFrameShape(QFrame::NoFrame);
    m_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_area->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    m_contenedor = new QWidget;
    m_layout     = new QVBoxLayout(m_contenedor);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(1);
    m_area->setWidget(m_contenedor);
    root->addWidget(m_area);
}

void VentanaPopup::limpiar() {
    while (QLayoutItem* item = m_layout->takeAt(0)) {
        if (QWidget* w = item->widget()) delete w;   // borrado inmediato: adjustSize() calcula bien el alto
        delete item;
    }
}

void VentanaPopup::abrirSobre(const QRect& ancla, DockPosition pos,
                              const QList<quint64>& ventanas,
                              const QIcon& iconoApp, const QString& nombreApp)
{
    limpiar();

    // Cabecera: nombre de la app + número de ventanas abiertas.
    auto* hdr = new QLabel(QStringLiteral("%1  ·  %2 ventana%3")
                               .arg(nombreApp)
                               .arg(ventanas.size())
                               .arg(ventanas.size() == 1 ? QString() : QStringLiteral("s")));
    hdr->setProperty("section", true);
    hdr->setWordWrap(true);
    m_layout->addWidget(hdr);

    // Una entrada por ventana; la activa queda resaltada.
    const quint64 activa = (quint64)x11_get_active_window();
    int n = 1;
    for (quint64 wid : ventanas) {
        char titulo[512] = {};
        if (!x11_get_window_title((unsigned long)wid, titulo, 512))
            snprintf(titulo, 512, "Ventana %d", n);
        QString t = QString::fromUtf8(titulo).trimmed();
        if (t.isEmpty()) t = QStringLiteral("Ventana %1").arg(n);
        else if (t.size() > 60) t = t.left(58) + QStringLiteral("…");

        auto* btn = new QPushButton(iconoApp, t);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setMinimumHeight(34);
        btn->setToolTip(t);
        btn->setProperty("activa", wid == activa);
        connect(btn, &QPushButton::clicked, this, [this, wid](){
            X11WindowUtils::toggleWindowFocusOrMinimize((WId)wid);
            close();
        });
        m_layout->addWidget(btn);
        n++;
    }
    m_layout->addStretch();

    // Alto limitado: con muchas ventanas aparece el scroll.
    const int altMax = qMin(480, 24 + ventanas.size() * 36 + 46);
    setMaximumHeight(qMax(74, altMax));
    adjustSize();

    // Anclaje junto al item del dock (mismo esquema que AppMenu).
    const int gap = 8;
    QPoint p;
    switch (pos) {
    case DockPosition::Abajo:     p = QPoint(ancla.left(), ancla.top()    - height() - gap); break;
    case DockPosition::Arriba:    p = QPoint(ancla.left(), ancla.bottom() + gap);            break;
    case DockPosition::Izquierda: p = QPoint(ancla.right()  + gap, ancla.top());             break;
    case DockPosition::Derecha:   p = QPoint(ancla.left() - width() - gap, ancla.top());     break;
    }

    if (QScreen* scr = QGuiApplication::screenAt(ancla.center())) {
        QRect sg = scr->availableGeometry();
        p.setX(qBound(sg.left(), p.x(), sg.right() - width()  + 1));
        p.setY(qBound(sg.top(),  p.y(), sg.bottom() - height() + 1));
    }

    move(p);
    show();
    raise();
}

void VentanaPopup::paintEvent(QPaintEvent*) {
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