#include "DockItem.h"
#include "DockWindow.h"
#include "PerfilVisual.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QProcess>
#include <QFontMetrics>
#include <QTime>
#include <QFile>
#include <QDir>
#include <cmath>

// ─── Constructor ────────────────────────────────────────────────────────────

DockItem::DockItem(const InfoApp& app, DockWindow* dock, QWidget* parent)
    : QWidget(parent), m_app(app), m_tipo(Tipo::Aplicacion), m_dock(dock)
{
    // Los items son ventanas hijas del panel: en perfil ligero también son
    // opacas (sin canal alfa) y rellenan su fondo con el color del panel.
    if (PerfilVisual::ligero()) {
        setAttribute(Qt::WA_OpaquePaintEvent);
    } else {
        setAttribute(Qt::WA_TranslucentBackground);
    }
    setMouseTracking(true);
    setFocusPolicy(Qt::NoFocus);

    m_animEscala = new QPropertyAnimation(this, "escala", this);
    m_animEscala->setDuration(140);
    m_animEscala->setEasingCurve(QEasingCurve::OutCubic);

    m_animRebote = new QPropertyAnimation(this, "rebote", this);
    m_animRebote->setDuration(180);
    m_animRebote->setEasingCurve(QEasingCurve::OutQuad);

    m_timerEtiqueta = new QTimer(this);
    m_timerEtiqueta->setSingleShot(true);
    m_timerEtiqueta->setInterval(600);
    connect(m_timerEtiqueta, &QTimer::timeout, [this](){ m_mostrarEtiqueta = true; update(); });

    m_timerRebote = new QTimer(this);
    connect(m_timerRebote, &QTimer::timeout, this, &DockItem::animarRebote);

    // Hover prolongado sobre un item con varias ventanas: el dock muestra el
    // popup de ventanas para elegir cuál activar/minimizar.
    m_timerPopupVentanas = new QTimer(this);
    m_timerPopupVentanas->setSingleShot(true);
    m_timerPopupVentanas->setInterval(380);
    connect(m_timerPopupVentanas, &QTimer::timeout,
            this, &DockItem::solicitudPopupVentanas);
}

DockItem::DockItem(Tipo tipo, DockWindow* dock, QWidget* parent)
    : QWidget(parent), m_tipo(tipo), m_dock(dock)
{
    // Igual que el ctor de aplicaciones: en perfil ligero los items son
    // ventanas opacas que rellenan su fondo con el color del panel.
    if (PerfilVisual::ligero()) {
        setAttribute(Qt::WA_OpaquePaintEvent);
    } else {
        setAttribute(Qt::WA_TranslucentBackground);
    }
    setFocusPolicy(Qt::NoFocus);
    m_animEscala = new QPropertyAnimation(this, "escala", this);
    m_animEscala->setDuration(140);
    m_animRebote = new QPropertyAnimation(this, "rebote", this);
    m_timerEtiqueta = new QTimer(this);
    // IMPORTANTE: configurar SIEMPRE el timer. Con los valores por defecto
    // (intervalo 0, repetidor) enterEvent→start() introduce un timer de 0 ms
    // que hace girar el event-loop al 100% de CPU. Single-shot de 600 ms como
    // en el ctor de apps; sin connect: los items fijos no muestran etiqueta
    // (comportamiento original).
    m_timerEtiqueta->setSingleShot(true);
    m_timerEtiqueta->setInterval(600);
    m_timerRebote   = new QTimer(this);
    m_timerPopupVentanas = new QTimer(this);
    m_timerPopupVentanas->setSingleShot(true);
    m_timerPopupVentanas->setInterval(380);
    connect(m_timerPopupVentanas, &QTimer::timeout,
            this, &DockItem::solicitudPopupVentanas);

    // El botón del lanzador es un item fijo: se comporta como una aplicación
    // común (magnificación, etiqueta) pero no tiene ventanas ni .desktop.
    if (tipo == Tipo::Lanzador) {
        m_app.nombre = QStringLiteral("Lanzador");
        m_app.icono  = QIcon(QStringLiteral(":/launcher/lanzador.png"));
        m_app.estaAnclada = true;
    }

    // Botón de escritorios virtuales: se coloca justo después del lanzador y
    // abre un popup con puntos para cambiar de escritorio.
    if (tipo == Tipo::Escritorios) {
        m_app.nombre = QStringLiteral("Escritorios");
        m_app.icono  = QIcon(QStringLiteral(":/launcher/desktop.png"));
        m_app.estaAnclada = true;
    }

    // Reloj del dock: item fijo al final con hora/fecha propia. El contorno
    // del cuadro actúa como indicador de batería (se refresca en cada tick).
    if (tipo == Tipo::Reloj) {
        m_app.nombre = QStringLiteral("Reloj");
        m_timerReloj = new QTimer(this);
        m_timerReloj->setInterval(5000);
        connect(m_timerReloj, &QTimer::timeout, [this](){
            // La hora se repinta cada 5 s; la batería (lectura de sysfs) se
            // relee cada ~12 ticks (60 s): suficiente y casi gratis.
            if (++m_ticksBateria % 12 == 0) actualizarBateria();
            update();
        });
        m_timerReloj->start();
        actualizarBateria();
    }
}

// ─── Batería ─────────────────────────────────────────────────────────────────
// Lee el nivel y estado de la primera batería del sistema desde sysfs
// (/sys/class/power_supply/BAT*). El contorno del reloj lo usa como indicador.

void DockItem::actualizarBateria() {
    // Gancho de prueba (SGP_BATERIA_TEST="nivel:estado", p. ej. "37:Charging")
    // para verificar el indicador sin depender de la batería real.
    const QByteArray test = qgetenv("SGP_BATERIA_TEST");
    if (!test.isEmpty()) {
        const QList<QByteArray> partes = test.split(':');
        bool ok = false;
        int v = partes.value(0).trimmed().toInt(&ok);
        if (ok) {
            m_bateriaNivel    = v;
            m_bateriaCargando = (partes.value(1).trimmed() == "Charging");
            return;
        }
    }

    QDir dir(QStringLiteral("/sys/class/power_supply"));

    // Detección robusta: cualquier entrada con type=="Battery" (cubre BAT0,
    // BATC, hidpp_battery_0, etc.), no sólo nombres "BAT*".
    QString base;
    const QStringList entradas = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& e : entradas) {
        QFile fType(dir.filePath(e) + QStringLiteral("/type"));
        if (fType.open(QIODevice::ReadOnly)) {
            if (QString::fromLocal8Bit(fType.readAll()).trimmed() == QLatin1String("Battery")) {
                base = dir.filePath(e);
                break;
            }
        }
    }
    if (base.isEmpty()) { m_bateriaNivel = -1; m_bateriaCargando = false; return; }

    int nivel = -1;

    QFile fCap(base + QStringLiteral("/capacity"));
    if (fCap.open(QIODevice::ReadOnly)) {
        bool ok = false;
        int v = fCap.readAll().trimmed().toInt(&ok);
        if (ok) nivel = v;
    }

    bool cargando = false;
    QFile fSta(base + QStringLiteral("/status"));
    if (fSta.open(QIODevice::ReadOnly)) {
        QString st = QString::fromLocal8Bit(fSta.readAll()).trimmed();
        cargando = (st == QLatin1String("Charging"));
        if (st == QLatin1String("Full")) nivel = 100;
    }

    m_bateriaNivel    = nivel;
    m_bateriaCargando = cargando;
}

QPixmap DockItem::iconoScaled(int sz) {
    // Reutilizar el pixmap ya escalado: en la magnificación el tamaño cambia
    // cada frame y QIcon::pixmap() reescalaba el original en cada llamada
    // (dos veces por frame: sombra + icono).
    if (m_pixIcoSz == sz && !m_pixIco.isNull()) return m_pixIco;
    m_pixIco = m_app.icono.pixmap(sz, sz);
    m_pixIcoSz = m_pixIco.isNull() ? -1 : sz;
    return m_pixIco;
}

// ─── Size ───────────────────────────────────────────────────────────────────

int DockItem::tamanoActual() const {
    return m_tamMag > 0 ? m_tamMag : (m_dock ? m_dock->config().tamanoIcono : 48);
}

QSize DockItem::tamano() const {
    if (m_tipo == Tipo::Separador) {
        bool h = m_dock && (m_dock->config().posicion == DockPosition::Abajo ||
                             m_dock->config().posicion == DockPosition::Arriba);
        int base = m_dock ? m_dock->config().tamanoIcono : 48;
        return h ? QSize(14, base + 20) : QSize(base + 20, 14);
    }
    if (m_tipo == Tipo::Reloj) {
        bool h = m_dock && (m_dock->config().posicion == DockPosition::Abajo ||
                             m_dock->config().posicion == DockPosition::Arriba);
        int base = m_dock ? m_dock->config().tamanoIcono : 48;
        int largo = 66;   // compacto: el cuadro se ajusta al ancho de la hora
        return h ? QSize(largo, base + 20) : QSize(base + 20, largo);
    }
    int sz = tamanoActual() + 20;
    return QSize(sz, sz);
}

// ─── Windows ────────────────────────────────────────────────────────────────

void DockItem::agregarVentana(quint64 id) {
    if (!m_app.ventanas.contains(id)) m_app.ventanas.append(id);
    m_app.estaEjecutando = true;
    update();
}

void DockItem::quitarVentana(quint64 id) {
    m_app.ventanas.removeAll(id);
    m_app.estaEjecutando = !m_app.ventanas.isEmpty();
    update();
}

// ─── Launch ─────────────────────────────────────────────────────────────────

void DockItem::lanzar() {
    if (m_app.comandoExec.isEmpty()) return;
    QProcess::startDetached("/bin/sh", {"-c", m_app.comandoExec});
    if (m_dock && m_dock->config().rebotarAlAbrir) {
        m_contRebote = 0;
        m_timerRebote->start(90);
    }
}

void DockItem::animarRebote() {
    if (m_contRebote >= 6) { m_timerRebote->stop(); setRebote(0); return; }
    qreal amp = (m_contRebote % 2 == 0) ? -10.0 : 0.0;

    // Invert for top dock
    if (m_dock && m_dock->config().posicion == DockPosition::Arriba) amp = -amp;

    m_animRebote->stop();
    m_animRebote->setStartValue(m_rebote);
    m_animRebote->setEndValue(amp);
    m_animRebote->start();
    m_contRebote++;
}

// ─── Paint ──────────────────────────────────────────────────────────────────

void DockItem::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);

    // Perfil ligero: el item es opaco, así que rellena su fondo con el color
    // del panel para integrarse (ya no hay transparencia que deje ver el padre).
    if (PerfilVisual::ligero())
        p.fillRect(rect(), PerfilVisual::colorFondoLigero());

    // Reloj del dock (item fijo al final): muestra solo la hora.
    if (m_tipo == Tipo::Reloj) {
        QString tiempo = QTime::currentTime().toString(QStringLiteral("HH:mm"));

        QFont f(QStringLiteral("Sans"), 14, QFont::DemiBold);
        QFontMetrics fm(f);
        const int pad    = 7;   // espacio entre el texto y el borde del cuadro
        const int margen = 7;   // hueco del cuadro respecto al canto del panel

        // Modo panel horizontal: el cuadro azul se ajusta al alto del panel
        // (stretch vertical). En el resto conserva el alto del texto.
        int boxH = fm.height() + 2*pad;
        if (m_dock && m_dock->config().modoPanel &&
            (m_dock->config().posicion == DockPosition::Abajo ||
             m_dock->config().posicion == DockPosition::Arriba))
            boxH = qMax(fm.height() + 2, height() - 2*margen);  // resto mínimo, sin exceder

        QRect box(width()/2  - (fm.horizontalAdvance(tiempo)/2 + pad),
                  height()/2 - boxH/2,
                  fm.horizontalAdvance(tiempo) + 2*pad,
                  boxH);

        // Fondo suave redondeado, ajustado a los números
        QPainterPath bg;
        bg.addRoundedRect(box, 8, 8);

        // El CUADRO de fondo del reloj es el indicador de batería (azul
        // marino oscuro brillante, semitransparente):
        //  - base: relleno navy oscuro suave en todo el cuadro
        //  - batería cargada: el cuadro se ve completamente azul marino
        //  - descargando: el color se reduce de derecha a izquierda
        //  - cargando:    el color aumenta de izquierda a derecha
        p.fillPath(bg, QColor(20, 32, 76, 100));

        if (m_bateriaNivel >= 0) {
            const double frac = qBound(0.0, m_bateriaNivel / 100.0, 1.0);
            if (frac > 0.0) {
                p.save();
                p.setClipRect(QRectF(box.left(), box.top(),
                                     box.width() * frac, box.height()));
                p.fillPath(bg, QColor(42, 70, 158, m_bateriaCargando ? 235 : 210));
                p.restore();
            }
        }

        // Contorno sutil, solo para definir el cuadro (no es el indicador)
        p.setPen(QPen(QColor(255, 255, 255, 26), 1));
        p.drawPath(bg);

        p.setPen(Qt::white);
        p.setFont(f);
        p.drawText(box, Qt::AlignCenter, tiempo);
        return;
    }

    // Separador
    if (m_tipo == Tipo::Separador) {
        bool h = m_dock && (m_dock->config().posicion == DockPosition::Abajo ||
                             m_dock->config().posicion == DockPosition::Arriba);
        p.setPen(QPen(QColor(255,255,255,60), 1.5));
        if (h) p.drawLine(width()/2, height()*0.2, width()/2, height()*0.8);
        else   p.drawLine(width()*0.2, height()/2, width()*0.8, height()/2);
        return;
    }

    bool esHorizontal = m_dock && (m_dock->config().posicion == DockPosition::Abajo ||
                                    m_dock->config().posicion == DockPosition::Arriba);

    int sz     = (int)(tamanoActual() * m_escala);

    // En modo panel el item tiene una altura (o anchura) fija y fina: el icono
    // se limita para que quepa sin salirse del borde (p. ej. 35 px de panel →
    // icono <= 29 px). En el dock compacto no hay cambio visible.
    const int espacio = qMin(width(), height());
    sz = qMin(sz, qMax(10, espacio - 6));

    int cx     = width()  / 2;
    int cy     = height() / 2;

    // Aplica rebote
    if (esHorizontal) cy += (int)m_rebote;
    else              cx += (int)m_rebote;

    QRect rc(cx - sz/2, cy - sz/2, sz, sz);

    // Glow si hover o activa (en perfil ligero se omite: es "vidrio" caro)
    if (!PerfilVisual::ligero() && (m_hover || m_activa)) {
        QPainterPath glow;
        glow.addRoundedRect(rc.adjusted(-3,-3,3,3), sz*0.25, sz*0.25);
        p.fillPath(glow, QColor(255,255,255, m_activa ? 45 : 25));
    }

    // Sombra suave (en perfil ligero se omite)
    if (!PerfilVisual::ligero() && sz > 20) {
        QPixmap shadow = iconoScaled(sz);
        p.save();
        p.setOpacity(0.25);
        p.drawPixmap(rc.translated(0, 3), shadow);
        p.setOpacity(1.0);
        p.restore();
    }

    // Ícono con esquinas redondeadas (el perfil ligero lo dibuja sin recorte)
    if (!m_app.icono.isNull()) {
        QPixmap px = iconoScaled(sz);
        if (!px.isNull()) {
            if (PerfilVisual::ligero()) {
                p.drawPixmap(rc, px);
            } else {
                QPainterPath clip;
                clip.addRoundedRect(rc, sz * 0.22, sz * 0.22);
                p.save();
                p.setClipPath(clip);
                p.drawPixmap(rc, px);
                p.restore();
            }
        }
    } else {
        // Placeholder gris
        p.setBrush(QColor(80,80,80,200));
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(rc, sz*0.22, sz*0.22);
        p.setPen(QColor(200,200,200));
        p.setFont(QFont("Sans", sz/4));
        p.drawText(rc, Qt::AlignCenter, m_app.nombre.left(1).toUpper());
    }

    // Indicadores de ejecución
    if (m_dock && m_dock->config().mostrarIndicadores && m_app.estaEjecutando)
        dibujarIndicadores(p, rc);

    // Etiqueta (no aplica al reloj, que ya muestra su propia información)
    if (m_mostrarEtiqueta && m_hover && m_dock && m_dock->config().mostrarEtiquetas &&
        m_tipo != Tipo::Reloj)
        dibujarEtiqueta(p);
}

void DockItem::dibujarIndicadores(QPainter& p, const QRect& rc) {
    auto pos = m_dock->config().posicion;
    int n = qMin((int)m_app.ventanas.size(), 3);
    if (n == 0) n = 1;

    QColor col = m_activa ? QColor(255,255,255,230) : QColor(160,200,255,180);
    p.setBrush(col);
    p.setPen(Qt::NoPen);

    int dot = 4, gap = 7;
    int total = n * dot + (n-1) * (gap - dot);

    for (int i = 0; i < n; i++) {
        int x, y;
        if (pos == DockPosition::Abajo) {
            x = rc.center().x() - total/2 + i*gap;
            y = qBound(0, rc.bottom() + 5, height() - dot);   // en panel fino, dentro del item
        } else if (pos == DockPosition::Arriba) {
            x = rc.center().x() - total/2 + i*gap;
            y = qBound(0, rc.top() - 9, height() - dot);
        } else if (pos == DockPosition::Izquierda) {
            x = qBound(0, rc.left() - 9, width() - dot);
            y = rc.center().y() - total/2 + i*gap;
        } else {
            x = qBound(0, rc.right() + 5, width() - dot);
            y = rc.center().y() - total/2 + i*gap;
        }
        p.drawEllipse(x, y, dot, dot);
    }
}

void DockItem::dibujarEtiqueta(QPainter& p) {
    QString nombre = m_app.nombre;
    if (nombre.length() > 24) nombre = nombre.left(22) + "…";

    QFont f("Sans", 11, QFont::Medium);
    p.setFont(f);
    QFontMetrics fm(f);
    int tw = fm.horizontalAdvance(nombre) + 18;
    int th = fm.height() + 10;

    auto pos = m_dock->config().posicion;
    QRect lr;
    if      (pos == DockPosition::Abajo)     lr = QRect(width()/2-tw/2, -th-8, tw, th);
    else if (pos == DockPosition::Arriba)    lr = QRect(width()/2-tw/2, height()+8, tw, th);
    else if (pos == DockPosition::Izquierda) lr = QRect(width()+8, height()/2-th/2, tw, th);
    else                                     lr = QRect(-tw-8, height()/2-th/2, tw, th);

    p.save();
    QPainterPath bg;
    bg.addRoundedRect(lr, 7, 7);
    p.fillPath(bg, QColor(20,20,20,215));
    p.setPen(QColor(255,255,255,40));
    p.drawPath(bg);
    p.setPen(Qt::white);
    p.drawText(lr, Qt::AlignCenter, nombre);
    p.restore();
}

// ─── Events ─────────────────────────────────────────────────────────────────

void DockItem::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        m_presion = true;
        m_animEscala->stop();
        m_animEscala->setStartValue(m_escala);
        m_animEscala->setEndValue(0.88);
        m_animEscala->start();
    } else if (e->button() == Qt::MiddleButton) {
        emit clicMedio();
    }
    QWidget::mousePressEvent(e);
}

void DockItem::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton && m_presion) {
        m_presion = false;
        m_animEscala->stop();
        m_animEscala->setStartValue(m_escala);
        m_animEscala->setEndValue(m_hover ? 1.08 : 1.0);
        m_animEscala->start();
        if (rect().contains(e->pos())) emit clicIzquierdo();
    } else if (e->button() == Qt::RightButton) {
        emit clicDerecho(e->globalPosition().toPoint());
    }
    QWidget::mouseReleaseEvent(e);
}

void DockItem::enterEvent(QEnterEvent* e) {
    m_hover = true;
    m_timerEtiqueta->start();
    // Aplicación con varias ventanas abiertas: tras un breve hover se muestra
    // el popup para elegir una ventana.
    if (m_tipo == Tipo::Aplicacion && m_app.estaEjecutando &&
        m_app.ventanas.size() > 1 && m_timerPopupVentanas)
        m_timerPopupVentanas->start();
    m_animEscala->stop();
    m_animEscala->setStartValue(m_escala);
    m_animEscala->setEndValue(1.08);
    m_animEscala->start();
    update();
    QWidget::enterEvent(e);
}

void DockItem::leaveEvent(QEvent* e) {
    m_hover = false;
    m_mostrarEtiqueta = false;
    m_timerEtiqueta->stop();
    if (m_timerPopupVentanas) m_timerPopupVentanas->stop();
    emit salidaHover();
    m_animEscala->stop();
    m_animEscala->setStartValue(m_escala);
    m_animEscala->setEndValue(1.0);
    m_animEscala->start();
    update();
    QWidget::leaveEvent(e);
}
