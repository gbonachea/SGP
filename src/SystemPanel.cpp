#include "SystemPanel.h"
#include "DockConfig.h"
#include "PerfilVisual.h"

#include <QProcess>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QScrollArea>
#include <QScrollBar>
#include <QFile>
#include <QDir>
#include <QTimer>
#include <QHash>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QGuiApplication>
#include <QScreen>
#include <QPainter>
#include <QPainterPath>
#include <QDialog>
#include <QLineEdit>
#include <QKeyEvent>
#include <QAction>
#include <QIcon>
#include <QPixmap>
#include <algorithm>

// ─── Helpers ─────────────────────────────────────────────────────────────────

static QFrame* separador() {
    auto* f = new QFrame;
    f->setObjectName("sep");
    f->setFixedHeight(1);
    return f;
}

SystemPanel::SystemPanel(QWidget* parent)
    : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint)
{
    if (PerfilVisual::ligero()) {
        setAttribute(Qt::WA_OpaquePaintEvent);
    } else {
        setAttribute(Qt::WA_TranslucentBackground);
    }
    setFixedWidth(320);
    // Altura tope para que el panel completo quepa en resoluciones pequeñas
    // (800x600): la lista de redes se desplaza internamente con su scroll.
    setMaximumHeight(520);
    construirUI();

    setStyleSheet(R"(
QLabel { color: #eee; background: transparent; border: none; }
QLabel[section="true"] {
    color: #89b4fa; font-weight: bold; font-size: 12px;
}
QLabel#estado { color: #9aa0ab; font-size: 11px; }
QPushButton {
    color: #eee; background-color: rgba(255,255,255,18);
    border: none; border-radius: 6px; padding: 5px 9px; font-size: 12px;
}
QPushButton:hover  { background-color: rgba(255,255,255,34); }
QPushButton:pressed{ background-color: rgba(137,180,250,70); }
QPushButton:disabled { color: #666; background-color: transparent; }
QFrame#sep { background-color: rgba(255,255,255,35); border: none; }
QSlider::groove:horizontal {
    height: 4px; background: rgba(255,255,255,45); border-radius: 2px;
}
QSlider::sub-page:horizontal { background: #89b4fa; border-radius: 2px; }
QSlider::handle:horizontal {
    width: 13px; margin: -5px 0; border-radius: 7px; background: #89b4fa;
}
/* Zona de redes: sin marco, fondo transparente para conservar el fondo del panel */
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
}

// ─── UI ──────────────────────────────────────────────────────────────────────

void SystemPanel::construirUI() {
    m_contenedor = new QVBoxLayout(this);
    m_contenedor->setContentsMargins(12, 12, 12, 10);
    m_contenedor->setSpacing(8);

    // ── Red ──
    auto* hdrRed = new QHBoxLayout();
    hdrRed->setSpacing(4);
    auto* tituloRed = new QLabel("Red");
    tituloRed->setProperty("section", true);
    hdrRed->addWidget(tituloRed);
    hdrRed->addStretch(1);
    auto* btnRefresh = new QPushButton("↻");
    btnRefresh->setToolTip("Buscar redes");
    btnRefresh->setFixedSize(26, 22);
    connect(btnRefresh, &QPushButton::clicked, this, &SystemPanel::rescanearRed);
    hdrRed->addWidget(btnRefresh);
    m_contenedor->addLayout(hdrRed);

    // Zona de redes dentro de un QScrollArea: si hay muchas redes, el panel
    // mantiene su tamaño y el resto se ve con la barra de desplazamiento.
    // Altura ≈ 6 filas visibles (cada botón ~27px + separación 4px).
    m_zonaRed = new QWidget(this);
    m_layoutRed = new QVBoxLayout(m_zonaRed);
    m_layoutRed->setContentsMargins(0, 0, 0, 0);
    m_layoutRed->setSpacing(4);
    m_scrollRed = new QScrollArea(this);
    m_scrollRed->setWidget(m_zonaRed);
    m_scrollRed->setWidgetResizable(true);
    m_scrollRed->setFrameShape(QFrame::NoFrame);
    m_scrollRed->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollRed->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollRed->setFixedHeight(6 * 27 + 5 * 4);   // ~182px
    m_contenedor->addWidget(m_scrollRed);

    auto* btnConfigRed = new QPushButton("Configurar red…");
    connect(btnConfigRed, &QPushButton::clicked, this, &SystemPanel::abrirConfigRed);
    m_contenedor->addWidget(btnConfigRed);

    m_contenedor->addWidget(separador());

    // ── Volumen ──
    auto* tituloVol = new QLabel("Volumen");
    tituloVol->setProperty("section", true);
    m_contenedor->addWidget(tituloVol);

    auto* rowVol = new QHBoxLayout();
    rowVol->setSpacing(6);
    auto* menosV = new QPushButton("−");
    menosV->setFixedSize(28, 26);
    connect(menosV, &QPushButton::clicked, this, [this](){ cambiarVolumen(-5); });
    m_labelVol = new QLabel("—%");
    m_labelVol->setFixedWidth(42);
    m_labelVol->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_sliderVol = new QSlider(Qt::Horizontal);
    m_sliderVol->setRange(0, 100);
    connect(m_sliderVol, &QSlider::valueChanged, this, [this](int v){
        m_labelVol->setText(QString("%1%").arg(v));
    });
    connect(m_sliderVol, &QSlider::sliderReleased, this, [this](){
        setVolumen(m_sliderVol->value());
    });
    auto* masV = new QPushButton("+");
    masV->setFixedSize(28, 26);
    connect(masV, &QPushButton::clicked, this, [this](){ cambiarVolumen(5); });
    rowVol->addWidget(menosV);
    rowVol->addWidget(m_sliderVol, 1);
    rowVol->addWidget(m_labelVol);
    rowVol->addWidget(masV);
    m_contenedor->addLayout(rowVol);

    m_btnMute = new QPushButton("Silenciar");
    connect(m_btnMute, &QPushButton::clicked, this, &SystemPanel::alternarMute);
    m_contenedor->addWidget(m_btnMute);

    m_contenedor->addWidget(separador());

    // ── Brillo ──
    auto* tituloBrillo = new QLabel("Brillo");
    tituloBrillo->setProperty("section", true);
    m_contenedor->addWidget(tituloBrillo);

    auto* rowBri = new QHBoxLayout();
    rowBri->setSpacing(6);
    auto* menosB = new QPushButton("−");
    menosB->setFixedSize(28, 26);
    connect(menosB, &QPushButton::clicked, this, [this](){ cambiarBrillo(-5); });
    m_labelBrillo = new QLabel("—%");
    m_labelBrillo->setFixedWidth(42);
    m_labelBrillo->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_sliderBrillo = new QSlider(Qt::Horizontal);
    m_sliderBrillo->setRange(0, 100);
    connect(m_sliderBrillo, &QSlider::valueChanged, this, [this](int v){
        m_labelBrillo->setText(QString("%1%").arg(v));
    });
    connect(m_sliderBrillo, &QSlider::sliderReleased, this, [this](){
        setBrillo(m_sliderBrillo->value());
    });
    auto* masB = new QPushButton("+");
    masB->setFixedSize(28, 26);
    connect(masB, &QPushButton::clicked, this, [this](){ cambiarBrillo(5); });
    rowBri->addWidget(menosB);
    rowBri->addWidget(m_sliderBrillo, 1);
    rowBri->addWidget(m_labelBrillo);
    rowBri->addWidget(masB);
    m_contenedor->addLayout(rowBri);

    m_contenedor->addWidget(separador());

    m_estado = new QLabel("");
    m_estado->setObjectName("estado");
    m_estado->setWordWrap(true);
    m_contenedor->addWidget(m_estado);
}

// ─── Pintado ─────────────────────────────────────────────────────────────────
// Fondo del panel: relleno opaco con esquinas redondeadas y borde sutil,
// pintado a mano (más fiable que la hoja de estilos sobre ventana translúcida)
// para que redes, volumen y brillo se vean con nitidez sobre cualquier fondo.

void SystemPanel::paintEvent(QPaintEvent*) {
    QPainter p(this);

    // Perfil ligero: fondo plano opaco, sin esquinas ni borde.
    if (PerfilVisual::ligero()) {
        p.fillRect(rect(), QColor(20, 20, 26));
        return;
    }

    p.setRenderHint(QPainter::Antialiasing, true);

    QPainterPath path;
    path.addRoundedRect(rect().adjusted(0, 0, -1, -1), 12, 12);

    // Fondo totalmente opaco
    p.fillPath(path, QColor(20, 20, 26));

    // Borde sutil para despegar el panel del escritorio
    p.setPen(QPen(QColor(255, 255, 255, 42), 1));
    p.drawPath(path);
}

// ─── Apertura ────────────────────────────────────────────────────────────────

void SystemPanel::abrirSobre(const QRect& ancla, DockPosition pos) {
    // Primero poblar los datos para que el layout tenga su tamaño final
    // y el anclaje se calcule con la altura real del panel.
    actualizarVolumen();
    actualizarBrillo();
    actualizarRed();

    adjustSize();
    QSize s = size();
    const int gap = 8;
    QPoint p;
    switch (pos) {
    case DockPosition::Abajo:     p = QPoint(ancla.right() - s.width(),  ancla.top()     - s.height() - gap); break;
    case DockPosition::Arriba:    p = QPoint(ancla.right() - s.width(),  ancla.bottom()  + gap);               break;
    case DockPosition::Izquierda: p = QPoint(ancla.right()  + gap,        ancla.bottom()  - s.height());       break;
    case DockPosition::Derecha:   p = QPoint(ancla.left()   - s.width() - gap, ancla.bottom() - s.height());  break;
    }

    if (QScreen* scr = QGuiApplication::screenAt(ancla.center())) {
        QRect sg = scr->availableGeometry();
        p.setX(qBound(sg.left(),  p.x(), sg.right()  - s.width()  + 1));
        p.setY(qBound(sg.top(),   p.y(), sg.bottom() - s.height() + 1));
    }

    move(p);
    show();
    raise();
}

QString SystemPanel::ejecutar(const QString& cmd, int timeoutMs) const {
    QProcess p;
    p.start(QStringLiteral("/bin/sh"), {QStringLiteral("-c"), cmd});
    if (!p.waitForStarted(800)) return {};
    if (!p.waitForFinished(timeoutMs)) { p.kill(); p.waitForFinished(300); return {}; }
    return QString::fromLocal8Bit(p.readAllStandardOutput());
}

void SystemPanel::setEstado(const QString& txt) {
    if (m_estado) m_estado->setText(txt);
}

// ─── Red ─────────────────────────────────────────────────────────────────────

void SystemPanel::actualizarRed() {
    if (QStandardPaths::findExecutable(QStringLiteral("nmcli")).isEmpty()) {
        setEstado(QStringLiteral("NetworkManager no disponible (falta nmcli)"));
        poblarRed(QString());
        return;
    }
    setEstado(QString());
    poblarRed(ejecutar(QStringLiteral("nmcli -t -f IN-USE,SSID,SIGNAL,SECURITY dev wifi list --rescan no"), 2500));
    refrescarPerfilesWifi();
}

void SystemPanel::refrescarPerfilesWifi() {
    if (QStandardPaths::findExecutable(QStringLiteral("nmcli")).isEmpty()) return;
    QProcess* p = new QProcess(this);
    connect(p, &QProcess::finished, this, [this, p](int, QProcess::ExitStatus){
        QSet<QString> ssids;
        const QString out = QString::fromLocal8Bit(p->readAllStandardOutput());
        p->deleteLater();
        for (const QString& linea : out.split('\n', Qt::SkipEmptyParts)) {
            QString s = linea.trimmed();
            if (!s.isEmpty()) ssids.insert(s);
        }
        m_perfilesWifi = ssids;
        m_perfilesWifiListos = true;
    });
    // Lista los perfiles guardados; para cada uno de tipo wifi obtiene su SSID
    // (el SSID de un perfil puede no coincidir con su nombre). Asíncrono.
    p->start(QStringLiteral("/bin/sh"), {QStringLiteral("-c"),
        QStringLiteral("nmcli -t -f NAME,TYPE connection show | "
                       "while IFS= read -r linea; do "
                       "t=\"${linea##*:}\"; n=\"${linea%:*}\"; "
                       "[ \"$t\" = \"802-11-wireless\" ] && "
                       "nmcli -g 802-11-wireless.ssid connection show \"$n\" 2>/dev/null; "
                       "done")});
}

// Consulta síncrona de los SSIDs con perfil guardado (mismo comando que la
// versión asíncrona). Se usa como respaldo si el usuario hace clic antes de
// que termine la consulta en segundo plano.
QSet<QString> SystemPanel::consultarPerfilesWifiSync() const {
    QSet<QString> ssids;
    const QString out = ejecutar(
        QStringLiteral("nmcli -t -f NAME,TYPE connection show | "
                       "while IFS= read -r linea; do "
                       "t=\"${linea##*:}\"; n=\"${linea%:*}\"; "
                       "[ \"$t\" = \"802-11-wireless\" ] && "
                       "nmcli -g 802-11-wireless.ssid connection show \"$n\" 2>/dev/null; "
                       "done"), 6000);
    for (const QString& linea : out.split('\n', Qt::SkipEmptyParts)) {
        QString s = linea.trimmed();
        if (!s.isEmpty()) ssids.insert(s);
    }
    return ssids;
}

void SystemPanel::rescanearRed() {
    if (QStandardPaths::findExecutable(QStringLiteral("nmcli")).isEmpty()) return;
    setEstado(QStringLiteral("Escaneando redes…"));
    QProcess* p = new QProcess(this);
    connect(p, &QProcess::finished, this, [this, p](int, QProcess::ExitStatus){
        QString out = QString::fromLocal8Bit(p->readAllStandardOutput());
        p->deleteLater();
        poblarRed(out);
        setEstado(QString());
        refrescarPerfilesWifi();
    });
    p->start(QStringLiteral("nmcli"),
             {QStringLiteral("-t"), QStringLiteral("-f"), QStringLiteral("IN-USE,SSID,SIGNAL,SECURITY"),
              QStringLiteral("dev"), QStringLiteral("wifi"), QStringLiteral("list"),
              QStringLiteral("--rescan"), QStringLiteral("yes")});
}

void SystemPanel::poblarRed(const QString& out) {
    // Limpiar la lista anterior
    while (QLayoutItem* child = m_layoutRed->takeAt(0)) {
        if (child->widget()) child->widget()->deleteLater();
        delete child;
    }
    // Al repoblar, empezar viendo las redes desde arriba.
    if (m_scrollRed) m_scrollRed->verticalScrollBar()->setValue(0);

    struct Red { QString ssid; int signal; bool secure; bool activa; };
    QHash<QString, Red> mejor;
    for (const QString& linea : out.split('\n')) {
        QString l = linea.trimmed();
        if (l.isEmpty()) continue;
        // Formato terse: IN-USE:SSID:SIGNAL:SECURITY. El SSID puede contener
        // ':', así que se parsea desde el final (SIGNAL y SECURITY no llevan ':').
        int iSec = l.lastIndexOf(':');
        if (iSec <= 0) continue;                    // sin SSID (red oculta)
        QString seguridad = l.mid(iSec + 1);
        QString pre = l.left(iSec);
        int iSig = pre.lastIndexOf(':');
        if (iSig <= 0) continue;
        // IN-USE es '*' si es la red activa, vacío si no. Quitar ambos del SSID.
        bool activa = pre.startsWith('*');
        QString ssid = pre.left(iSig);
        if (activa)      ssid.remove(0, 1);        // quitar '*'
        if (ssid.startsWith(':')) ssid.remove(0, 1);// quitar el separador vacío
        bool ok = false;
        int sig = pre.mid(iSig + 1).toInt(&ok);
        if (!ok) continue;
        bool secure = !seguridad.isEmpty();
        if (ssid.isEmpty()) continue;              // campo IN-USE vacío + SSID vacío
        // Se prefiere la misma red marcada como activa (aunque tenga peor señal).
        bool ya = mejor.contains(ssid);
        if (!ya || (mejor.value(ssid).signal < sig) || (activa && !mejor.value(ssid).activa))
            mejor.insert(ssid, {ssid, sig, secure, activa});
    }

    QList<Red> redes;
    for (auto it = mejor.cbegin(); it != mejor.cend(); ++it)
        redes.append(it.value());
    std::sort(redes.begin(), redes.end(),
              [](const Red& a, const Red& b){ return a.signal > b.signal; });

    if (redes.isEmpty()) {
        auto* l = new QLabel(QStringLiteral("Sin redes detectadas"));
        l->setStyleSheet(QStringLiteral("color: #888; padding: 4px;"));
        m_layoutRed->addWidget(l);
        return;
    }

    for (const Red& r : redes) {
        QString etiqueta;
        if (r.activa)
            etiqueta = QStringLiteral("● %1   %2%").arg(r.ssid).arg(r.signal);
        else
            etiqueta = r.secure
                ? QStringLiteral("%1 🔒   %2%").arg(r.ssid).arg(r.signal)
                : QStringLiteral("%1   %2%").arg(r.ssid).arg(r.signal);
        auto* b = new QPushButton(etiqueta);
        b->setStyleSheet(QStringLiteral(
            "text-align:left; padding:5px 8px; font-size:12px; color:#eee;"
            "background-color:rgba(255,255,255,14); border:none; border-radius:6px;"));
        connect(b, &QPushButton::clicked, this, [this, r](){ conectarWifi(r.ssid, r.secure, r.activa); });
        m_layoutRed->addWidget(b);
    }
}

// ─── Diálogo oscuro de contraseña ───────────────────────────────────────────

// Icono de "ojo" dibujado a mano (sin depender de emojis/temas del sistema):
// abierto para "ver la clave" y con una diagonal para "ocultarla".
static QIcon iconoOjo(bool visible) {
    QPixmap pm(20, 14);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    QPen pen(QColor(0xd4, 0xd8, 0xe0), 1.5);
    pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);

    const QPointF c(10, 7);
    // Ojo: elipse (contorno) con pupila en el centro.
    p.drawEllipse(QRectF(2.5, 3.0, 15.0, 8.5));
    p.drawEllipse(c, 2.2, 2.2);
    if (!visible) {
        // Diagonal "oculto".
        p.drawLine(QPointF(3.5, 2.5), QPointF(16.5, 11.5));
    }
    p.end();
    return QIcon(pm);
}

QString SystemPanel::pedirClave(QWidget* parent,
                                const QString& titulo, const QString& mensaje) {
    // Dialogo sin marco con el aspecto del dock: fondo oscuro y redondeado,
    // texto claro y botones bien diferenciados (Conectar en azul acento),
    // para que no se confundan en pantallas claras como pasaba con el de Qt.
    QDialog dlg(parent);
    dlg.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint
                       | Qt::WindowStaysOnTopHint);
    dlg.setAttribute(Qt::WA_TranslucentBackground);
    dlg.setFixedWidth(300);
    dlg.setStyleSheet(R"(
QFrame#fondo {
    background-color: #17181d;
    border: 1px solid rgba(137,180,250,80);
    border-radius: 12px;
}
QLabel#titulo  { color: #89b4fa; background: transparent; font-size: 14px; font-weight: bold; }
QLabel#mensaje { color: #d4d8e0; background: transparent; font-size: 12px; }
QLineEdit {
    color: #ffffff; background-color: #23242b;
    border: 1px solid #3a3d47; border-radius: 6px;
    padding: 7px 9px; font-size: 13px;
    selection-background-color: #89b4fa; selection-color: #0e0f12;
}
QLineEdit:focus { border-color: #89b4fa; }
QPushButton {
    color: #ffffff; background-color: #2b2d36;
    border: 1px solid #4a4e5a; border-radius: 6px;
    padding: 7px 16px; font-size: 12px;
}
QPushButton:hover { background-color: #383b47; }
QPushButton#conectar {
    color: #0e0f12; background-color: #89b4fa;
    border: 1px solid #89b4fa; font-weight: bold;
}
QPushButton#conectar:hover  { background-color: #9fc2fc; }
QPushButton#conectar:pressed{ background-color: #74a0e8; }
)");

    auto* cont = new QVBoxLayout(&dlg);
    cont->setContentsMargins(0, 0, 0, 0);

    auto* fondo = new QFrame;
    fondo->setObjectName("fondo");
    auto* lay = new QVBoxLayout(fondo);
    lay->setContentsMargins(16, 14, 16, 14);
    lay->setSpacing(10);

    auto* lblTitulo = new QLabel(titulo);
    lblTitulo->setObjectName("titulo");
    auto* lblMensaje = new QLabel(mensaje);
    lblMensaje->setObjectName("mensaje");
    lblMensaje->setWordWrap(true);

    auto* edt = new QLineEdit;
    edt->setEchoMode(QLineEdit::Password);
    edt->setPlaceholderText(QStringLiteral("Contraseña de la red"));

    // Ojo para mostrar/ocultar la contraseña mientras se escribe.
    auto* accionOjo = edt->addAction(iconoOjo(false), QLineEdit::TrailingPosition);
    accionOjo->setCheckable(true);
    accionOjo->setToolTip(QStringLiteral("Mostrar contraseña"));
    connect(accionOjo, &QAction::toggled, edt, [edt, accionOjo](bool on) {
        edt->setEchoMode(on ? QLineEdit::Normal : QLineEdit::Password);
        accionOjo->setIcon(iconoOjo(on));
        accionOjo->setToolTip(on ? QStringLiteral("Ocultar contraseña")
                                 : QStringLiteral("Mostrar contraseña"));
    });

    auto* row = new QHBoxLayout;
    row->setSpacing(8);
    auto* btnCancelar = new QPushButton(QStringLiteral("Cancelar"));
    auto* btnConectar = new QPushButton(QStringLiteral("Conectar"));
    btnConectar->setObjectName("conectar");
    btnConectar->setDefault(true);   // Enter confirma en el diálogo

    lay->addWidget(lblTitulo);
    lay->addWidget(lblMensaje);
    lay->addWidget(edt);
    row->addStretch(1);
    row->addWidget(btnCancelar);
    row->addWidget(btnConectar);
    lay->addLayout(row);
    cont->addWidget(fondo);

    connect(btnCancelar, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(btnConectar, &QPushButton::clicked, &dlg, &QDialog::accept);
    // Enter dentro del campo de clave también confirma.
    connect(edt, &QLineEdit::returnPressed, &dlg, &QDialog::accept);

    edt->setFocus();
    dlg.adjustSize();
    if (parent) {
        // Centrar sobre la ventana del dock (o la pantalla si no hay padre).
        QPoint c = parent->mapToGlobal(parent->rect().center())
                   - QPoint(dlg.width() / 2, dlg.height() / 2);
        dlg.move(c);
    }
    dlg.exec();

    if (dlg.result() != QDialog::Accepted) return QString();
    return edt->text();
}

void SystemPanel::conectarWifi(const QString& ssid, bool secure, bool activa) {
    // Si ya estamos conectados a esa red, no hay nada que hacer ni pedir:
    // el propio menú ya la marca como conectada (●).
    if (activa) {
        setEstado(QStringLiteral("Ya conectado a «%1»").arg(ssid));
        return;
    }
    // ¿Este SSID ya tiene un perfil guardado en NetworkManager? Si es así, la
    // contraseña ya está almacenada y NM conectará directo sin pedir nada.
    // Si la consulta en segundo plano aún no terminó, se consulta al momento
    // (solo esa vez: luego la lista ya queda cacheada).
    QSet<QString> guardadas = m_perfilesWifi;
    if (!m_perfilesWifiListos) guardadas = consultarPerfilesWifiSync();
    const bool guardada = guardadas.contains(ssid.trimmed());
    // Red con seguridad sin perfil guardado: pedir la contraseña AHORA,
    // directamente. No se puede "probar sin contraseña y esperar el fallo":
    // nmcli se queda esperando al agente de secretos del escritorio
    // (org.cinnamon.NetworkAgent u otro) y el diálogo nunca llegaría a aparecer.
    if (secure && !guardada) {
        QString pw = pedirClave(
            window(),
            QStringLiteral("Red segura"),
            QStringLiteral("Contraseña de «%1»:").arg(ssid));
        if (pw.isEmpty()) {
            setEstado(QStringLiteral("Conexión a «%1» cancelada").arg(ssid));
            return;
        }
        lanzarConexionWifi(ssid, pw, true);
        return;
    }
    // Red abierta, o red segura con perfil guardado (NM usará la clave almacenada).
    lanzarConexionWifi(ssid, QString(), secure);
}

void SystemPanel::lanzarConexionWifi(const QString& ssid, const QString& password, bool secure) {
    setEstado(QStringLiteral("Conectando a «%1»…").arg(ssid));
    QProcess* p = new QProcess(this);
    // Watchdog: si nmcli se queda esperando al agente de secretos (o a lo que
    // sea), se mata el proceso para no dejar el estado "Conectando…" colgado.
    QTimer* watchdog = new QTimer(p);
    watchdog->setSingleShot(true);
    watchdog->setInterval(12000);
    connect(watchdog, &QTimer::timeout, p, &QProcess::kill);
    watchdog->start();
    connect(p, &QProcess::finished, this, [this, p, watchdog, ssid, password, secure](int, QProcess::ExitStatus){
        watchdog->stop();
        QString out = QString::fromLocal8Bit(p->readAllStandardOutput()
                                             + p->readAllStandardError()).trimmed();
        bool exito = (p->exitStatus() == QProcess::NormalExit && p->exitCode() == 0);
        p->deleteLater();

        if (exito) {
            setEstado(QStringLiteral("Conectado a «%1»").arg(ssid));
            return;
        }

        // ¿La red pedía contraseña (o la clave fue rechazada) y aún no la dimos?
        bool necesitaClave = secure || out.contains("Secrets were required", Qt::CaseInsensitive)
                                   || out.contains("passphrase", Qt::CaseInsensitive)
                                   || out.contains("password", Qt::CaseInsensitive);
        if (necesitaClave && password.isEmpty()) {
            QString nuevaPw = pedirClave(
                window(),
                QStringLiteral("Red segura"),
                QStringLiteral("Contraseña de «%1»:").arg(ssid));
            if (!nuevaPw.isEmpty()) {
                lanzarConexionWifi(ssid, nuevaPw, secure);   // reintentar con clave
                return;
            }
            setEstado(QStringLiteral("Conexión a «%1» cancelada").arg(ssid));
            return;
        }

        QString detalle = out.split('\n').last();
        if (password.isEmpty())
            setEstado(QStringLiteral("Error en «%1»: %2")
                          .arg(ssid, detalle.isEmpty() ? QStringLiteral("sin detalle") : detalle));
        else
            setEstado(QStringLiteral("«%1»: no se pudo conectar (clave incorrecta o error)")
                          .arg(ssid));
    });

    QStringList args{QStringLiteral("device"), QStringLiteral("wifi"),
                     QStringLiteral("connect"), ssid};
    if (!password.isEmpty()) {
        args << QStringLiteral("password") << password;
    }
    p->start(QStringLiteral("nmcli"), args);
}

void SystemPanel::abrirConfigRed() {
    struct Candidato { QString exe; QStringList args; };
    static const QList<Candidato> candidatos = {
        {QStringLiteral("nm-connection-editor"), {}},
        {QStringLiteral("gnome-control-center"), {QStringLiteral("network")}},
        {QStringLiteral("xfce4-settings-manager"), {}},
        {QStringLiteral("systemsettings"), {}},
    };
    for (const auto& c : candidatos) {
        QString exe = QStandardPaths::findExecutable(c.exe);
        if (!exe.isEmpty()) {
            QProcess::startDetached(exe, c.args);
            return;
        }
    }
    setEstado(QStringLiteral("No se encontró un configurador de red."));
}

// ─── Volumen ─────────────────────────────────────────────────────────────────

void SystemPanel::actualizarVolumen() {
    m_volPactl = !QStandardPaths::findExecutable(QStringLiteral("pactl")).isEmpty();

    bool muted = false;
    int  v = -1;
    if (m_volPactl) {
        QString out = ejecutar(QStringLiteral("pactl get-sink-volume @DEFAULT_SINK@"));
        QRegularExpression re(QStringLiteral("(\\d+)%"));
        QRegularExpressionMatch m = re.match(out);
        if (m.hasMatch()) v = m.captured(1).toInt();
        muted = ejecutar(QStringLiteral("pactl get-sink-mute @DEFAULT_SINK@")).contains(QStringLiteral("yes"));
    } else {
        QString out = ejecutar(QStringLiteral("amixer -M get Master"));
        QRegularExpression re(QStringLiteral("\\[(\\d+)%\\]"));
        QRegularExpressionMatch m = re.match(out);
        if (m.hasMatch()) v = m.captured(1).toInt();
        muted = out.contains(QStringLiteral("[off]"));
    }

    if (v >= 0) {
        m_sliderVol->setValue(v);
        m_labelVol->setText(QStringLiteral("%1%").arg(v));
    }
    bool ok = (v >= 0);
    m_sliderVol->setEnabled(ok);
    m_btnMute->setEnabled(ok);
    m_btnMute->setText(muted ? QStringLiteral("Activar sonido")
                             : QStringLiteral("Silenciar"));
}

void SystemPanel::setVolumen(int v) {
    v = qBound(0, v, 100);
    if (m_volPactl) {
        QProcess::startDetached(QStringLiteral("pactl"),
                                {QStringLiteral("set-sink-volume"),
                                 QStringLiteral("@DEFAULT_SINK@"),
                                 QStringLiteral("%1%").arg(v)});
    } else {
        QProcess::startDetached(QStringLiteral("amixer"),
                                {QStringLiteral("-q"), QStringLiteral("-M"), QStringLiteral("set"),
                                 QStringLiteral("Master"), QStringLiteral("%1%").arg(v)});
    }
}

void SystemPanel::cambiarVolumen(int delta) {
    if (m_volPactl) {
        QProcess::startDetached(QStringLiteral("pactl"),
                                {QStringLiteral("set-sink-volume"),
                                 QStringLiteral("@DEFAULT_SINK@"),
                                 QStringLiteral("%1%2").arg(qAbs(delta)).arg(delta > 0 ? "+" : "-")});
    } else {
        QProcess::startDetached(QStringLiteral("amixer"),
                                {QStringLiteral("-q"), QStringLiteral("-M"), QStringLiteral("set"),
                                 QStringLiteral("Master"),
                                 QStringLiteral("%1%2").arg(qAbs(delta)).arg(delta > 0 ? "+" : "-")});
    }
    QTimer::singleShot(250, this, [this]{ actualizarVolumen(); });
}

void SystemPanel::alternarMute() {
    if (m_volPactl) {
        QProcess::startDetached(QStringLiteral("pactl"),
                                {QStringLiteral("set-sink-mute"),
                                 QStringLiteral("@DEFAULT_SINK@"),
                                 QStringLiteral("toggle")});
    } else {
        QProcess::startDetached(QStringLiteral("amixer"),
                                {QStringLiteral("-q"), QStringLiteral("set"),
                                 QStringLiteral("Master"), QStringLiteral("toggle")});
    }
    QTimer::singleShot(250, this, [this]{ actualizarVolumen(); });
}

// ─── Brillo ──────────────────────────────────────────────────────────────────

QString SystemPanel::rutaBacklight() const {
    QDir d(QStringLiteral("/sys/class/backlight"));
    const QStringList entradas = d.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    return entradas.isEmpty() ? QString() : d.filePath(entradas.first());
}

void SystemPanel::actualizarBrillo() {
    m_brilloCtl = !QStandardPaths::findExecutable(QStringLiteral("brightnessctl")).isEmpty();

    if (m_brilloCtl) {
        bool ok1 = false, ok2 = false;
        int cur = ejecutar(QStringLiteral("brightnessctl get")).trimmed().toInt(&ok1);
        int max = ejecutar(QStringLiteral("brightnessctl max")).trimmed().toInt(&ok2);
        if (ok1 && ok2 && max > 0)
            m_sliderBrillo->setValue(qBound(0, (int)(cur * 100.0 / max + 0.5), 100));
        m_sliderBrillo->setEnabled(ok1 && ok2 && max > 0);
        return;
    }

    m_backlightDir = rutaBacklight();
    m_brilloSysfs  = !m_backlightDir.isEmpty();
    if (m_brilloSysfs) {
        QFile fMax(m_backlightDir + QStringLiteral("/max_brightness"));
        QFile fCur(m_backlightDir + QStringLiteral("/brightness"));
        if (fMax.open(QIODevice::ReadOnly) && fCur.open(QIODevice::ReadOnly)) {
            bool ok1 = false, ok2 = false;
            int max = fMax.readAll().trimmed().toInt(&ok1);
            int cur = fCur.readAll().trimmed().toInt(&ok2);
            if (ok1 && ok2 && max > 0)
                m_sliderBrillo->setValue(qBound(0, (int)(cur * 100.0 / max + 0.5), 100));
            m_sliderBrillo->setEnabled(ok1 && ok2 && max > 0);
        } else {
            m_sliderBrillo->setEnabled(false);
        }
    } else {
        m_sliderBrillo->setEnabled(false);
        m_labelBrillo->setText(QStringLiteral("—"));
        setEstado(QStringLiteral("Brillo no disponible (instala brightnessctl)."));
    }
}

void SystemPanel::setBrillo(int v) {
    v = qBound(0, v, 100);
    if (m_brilloCtl) {
        QProcess::startDetached(QStringLiteral("brightnessctl"),
                                {QStringLiteral("-q"), QStringLiteral("set"),
                                 QStringLiteral("%1%").arg(v)});
        return;
    }
    if (m_brilloSysfs && !m_backlightDir.isEmpty()) {
        QFile fMax(m_backlightDir + QStringLiteral("/max_brightness"));
        QFile fBri(m_backlightDir + QStringLiteral("/brightness"));
        if (fMax.open(QIODevice::ReadOnly) && fBri.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            int max = fMax.readAll().trimmed().toInt();
            if (max > 0) {
                fBri.write(QByteArray::number(max * v / 100));
                fBri.close();
            }
        } else {
            setEstado(QStringLiteral("Brillo: permiso denegado (requiere root)."));
            QTimer::singleShot(200, this, [this]{ actualizarBrillo(); });
        }
    }
}

void SystemPanel::cambiarBrillo(int delta) {
    setBrillo(m_sliderBrillo->value() + delta);
    QTimer::singleShot(250, this, [this]{ actualizarBrillo(); });
}