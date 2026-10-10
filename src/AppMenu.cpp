#include "AppMenu.h"
#include "PerfilVisual.h"

#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QShortcut>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QScrollArea>
#include <QProcess>
#include <QIcon>
#include <QScreen>
#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>
#include <QFrame>
#include <QStringList>
#include <algorithm>

// ─── Categorías: etiqueta mostrada → categorías freedesktop de origen ────────
// El orden de la lista define la prioridad de clasificación: la primera cuyas
// categorías coincidan con las del .desktop gana.

namespace {

struct CatDef {
    const char*     etiqueta;
    QStringList     origenes;
};

const QList<CatDef> CATEGORIAS = {
    { "Accesorios",    {"Utility", "Accessibility"} },
    { "Internet",      {"Network", "WebBrowser", "Chat", "InstantMessaging", "Email"} },
    { "Gráficos",      {"Graphics", "Photography", "Viewer"} },
    { "Multimedia",    {"AudioVideo", "Audio", "Video", "Player", "Music", "Recorder"} },
    { "Oficina",       {"Office", "WordProcessor", "Spreadsheet", "Presentation",
                        "Calendar", "ContactManagement", "Finance"} },
    { "Desarrollo",    {"Development", "IDE", "TextEditor", "Building"} },
    { "Juegos",        {"Game"} },
    { "Educación",     {"Education", "Languages", "Art", "Geology"} },
    { "Ciencia",       {"Science", "Math", "Chemistry", "Physics", "Biology", "Astronomy"} },
    { "Sistema",       {"System", "Monitor", "TerminalEmulator",
                        "HardwareSettings", "Security"} },
    { "Preferencias",  {"Settings", "SystemSettings", "DesktopSettings", "ControlCenter"} },
};

// Categoría especial de acciones de sesión/energía (siempre al final).
const QString CAT_APAGAR = QStringLiteral("Apagar y salir");

// Acciones de sesión y energía.
struct AccionSistema {
    const char* etiqueta;
    QString     icono;
    QString     comando;
};

const QList<AccionSistema> ACCIONES_SISTEMA = {
    { "Bloquear sesión", QStringLiteral("system-lock-screen"),
      QStringLiteral("loginctl lock-session || xdg-screensaver lock") },
    { "Cerrar sesión", QStringLiteral("system-log-out"),
      QStringLiteral("cinnamon-session-quit --logout || gnome-session-quit --logout || "
                     "cinnamon-session-quit --exit || loginctl terminate-session") },
    { "Suspender", QStringLiteral("system-suspend"),
      QStringLiteral("systemctl suspend || pm-suspend") },
    { "Reiniciar", QStringLiteral("system-reboot"),
      QStringLiteral("systemctl reboot") },
    { "Apagar", QStringLiteral("system-shutdown"),
      QStringLiteral("systemctl poweroff") },
};

} // namespace

AppMenu::AppMenu(const QList<InfoApp>& apps, QWidget* parent)
    : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint)
{
    if (PerfilVisual::ligero()) {
        setAttribute(Qt::WA_OpaquePaintEvent);
    } else {
        setAttribute(Qt::WA_TranslucentBackground);
    }

    // Agrupar aplicaciones por categoría.
    for (const InfoApp& a : apps)
        m_grupos[etiquetaCategoria(a.categorias)].append(a);

    // Orden de aparición de las categorías: fijo de CATEGORIAS, luego el resto
    // alfabético, "Otros" al final y por último la categoría de sistema.
    QList<QString> orden;
    for (const CatDef& c : CATEGORIAS)
        if (m_grupos.contains(QString::fromUtf8(c.etiqueta)) &&
            !m_grupos.value(QString::fromUtf8(c.etiqueta)).isEmpty())
            orden << QString::fromUtf8(c.etiqueta);
    QStringList restantes;
    for (auto it = m_grupos.constBegin(); it != m_grupos.constEnd(); ++it)
        if (!orden.contains(it.key()) && it.key() != QStringLiteral("Otros"))
            restantes << it.key();
    restantes.sort();
    for (const QString& r : restantes) orden << r;
    if (m_grupos.contains(QStringLiteral("Otros")) &&
        !m_grupos.value(QStringLiteral("Otros")).isEmpty())
        orden << QStringLiteral("Otros");
    orden << CAT_APAGAR;

    m_ordenCategorias = orden;
    construirUI();

    if (!m_ordenCategorias.isEmpty())
        m_listaCat->setCurrentRow(0);   // muestra la primera categoría
}

// ─── Mapeo de categorías ─────────────────────────────────────────────────────

QString AppMenu::etiquetaCategoria(const QString& rawCats) const {
    if (rawCats.isEmpty()) return QStringLiteral("Otros");

    QStringList tokens;
    for (const QString& t : rawCats.split(';'))
        if (!t.trimmed().isEmpty()) tokens << t.trimmed();

    for (const CatDef& c : CATEGORIAS)
        for (const QString& token : c.origenes)
            if (tokens.contains(token))
                return QString::fromUtf8(c.etiqueta);
    return QStringLiteral("Otros");
}

// ─── UI ──────────────────────────────────────────────────────────────────────

void AppMenu::construirUI() {
    setStyleSheet(R"(
QLabel { color: #eee; background: transparent; border: none; }
QLabel[section="true"] {
    color: #89b4fa; font-weight: bold; font-size: 12px;
    padding: 6px 4px 2px 4px;
}
QLineEdit {
    background-color: rgba(255,255,255,25); color: #eee;
    border: 1px solid rgba(255,255,255,45); border-radius: 8px;
    padding: 4px 10px; font-size: 13px;
    selection-background-color: #7aa2f7; selection-color: #111;
}
QLineEdit:focus { border-color: #89b4fa; background-color: rgba(255,255,255,35); }
QPushButton {
    color: #eee; background-color: transparent;
    border: none; border-radius: 6px;
    padding: 8px 10px; font-size: 13px;
    text-align: left; icon-size: 24px;
}
QPushButton:hover   { background-color: rgba(137,180,250,45); }
QPushButton:pressed { background-color: rgba(137,180,250,80); }
QListWidget#categorias {
    background: transparent; border: none; color: #ddd;
    font-size: 13px; outline: none;
}
QListWidget#categorias::item { padding: 8px 10px; border-radius: 6px; }
QListWidget#categorias::item:hover     { background: rgba(137,180,250,40); }
QListWidget#categorias::item:selected  { background: rgba(137,180,250,70); color: #fff; }
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
    root->setSpacing(8);
    setFixedWidth(480);

    // Barra de búsqueda (filtra el panel derecho mientras se escribe).
    m_busqueda = new QLineEdit(this);
    m_busqueda->setPlaceholderText(QStringLiteral("Buscar aplicaciones…"));
    m_busqueda->setClearButtonEnabled(true);
    m_busqueda->setMinimumHeight(34);
    connect(m_busqueda, &QLineEdit::textChanged, this, &AppMenu::buscarCambio);
    connect(m_busqueda, &QLineEdit::returnPressed, this, &AppMenu::alPulsarEnter);
    root->addWidget(m_busqueda);

    auto* fila = new QHBoxLayout;
    fila->setSpacing(10);

    // Columna izquierda: categorías.
    m_listaCat = new QListWidget(this);
    m_listaCat->setObjectName(QStringLiteral("categorias"));
    m_listaCat->setFixedWidth(160);
    m_listaCat->setFrameShape(QFrame::NoFrame);
    m_listaCat->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    for (const QString& cat : m_ordenCategorias) {
        auto* it = new QListWidgetItem(cat);
        it->setSizeHint(QSize(0, 36));
        m_listaCat->addItem(it);
    }
    connect(m_listaCat, &QListWidget::currentTextChanged,
            this, &AppMenu::mostrarCategoria);
    fila->addWidget(m_listaCat);

    // Columna derecha: aplicativos o resultados de búsqueda.
    m_areaApps = new QScrollArea(this);
    m_areaApps->setWidgetResizable(true);
    m_areaApps->setFrameShape(QFrame::NoFrame);
    m_areaApps->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_areaApps->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    m_contenedorApps = new QWidget;
    m_layoutApps     = new QVBoxLayout(m_contenedorApps);
    m_layoutApps->setContentsMargins(0, 0, 0, 0);
    m_layoutApps->setSpacing(1);
    m_areaApps->setWidget(m_contenedorApps);
    fila->addWidget(m_areaApps, 1);
    root->addLayout(fila, 1);

    // Escape: limpia la búsqueda; si ya está vacía, cierra el menú.
    auto* esc = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(esc, &QShortcut::activated, this, &AppMenu::alPulsarEscape);

    setMaximumHeight(560);
}

void AppMenu::mostrarCategoria(const QString& categoria) {
    m_categoriaActual = categoria;
    if (!m_busqueda->text().trimmed().isEmpty()) {
        m_busqueda->clear();              // textChanged → poblarDerecha()
        return;
    }
    poblarDerecha();
}

void AppMenu::buscarCambio(const QString&) {
    poblarDerecha();
}

void AppMenu::poblarDerecha() {
    // Limpiar el contenedor de apps.
    while (QLayoutItem* item = m_layoutApps->takeAt(0)) {
        if (QWidget* w = item->widget()) w->deleteLater();
        delete item;
    }

    // Modo búsqueda: filtra aplicaciones de todas las categorías.
    const QString q = m_busqueda->text().trimmed();
    if (!q.isEmpty()) {
        QList<InfoApp> resultados;
        for (auto it = m_grupos.constBegin(); it != m_grupos.constEnd(); ++it)
            for (const InfoApp& a : it.value())
                if (a.nombre.contains(q, Qt::CaseInsensitive))
                    resultados << a;

        if (resultados.isEmpty()) {
            auto* hdr = new QLabel(QStringLiteral("Sin resultados"));
            hdr->setProperty("section", true);
            m_layoutApps->addWidget(hdr);
            m_layoutApps->addStretch();
            return;
        }

        auto* hdr = new QLabel(QStringLiteral("Resultados (%1)").arg(resultados.size()));
        hdr->setProperty("section", true);
        m_layoutApps->addWidget(hdr);
        for (const InfoApp& a : resultados)
            anadirBotonApp(a);
        m_layoutApps->addStretch();
        return;
    }

    // Modo categoría.
    auto* hdr = new QLabel(m_categoriaActual);
    hdr->setProperty("section", true);
    m_layoutApps->addWidget(hdr);

    // Categoría especial: acciones de sesión y energía.
    if (m_categoriaActual == CAT_APAGAR) {
        for (const AccionSistema& a : ACCIONES_SISTEMA) {
            auto* btn = new QPushButton(QIcon::fromTheme(a.icono, QIcon()),
                                        QString::fromUtf8(a.etiqueta));
            btn->setCursor(Qt::PointingHandCursor);
            btn->setMinimumHeight(36);
            connect(btn, &QPushButton::clicked, this,
                    [this, a]() { lanzarComando(a.comando); });
            m_layoutApps->addWidget(btn);
        }
        m_layoutApps->addStretch();
        return;
    }

    for (const InfoApp& a : m_grupos.value(m_categoriaActual))
        anadirBotonApp(a);
    m_layoutApps->addStretch();
}

void AppMenu::anadirBotonApp(const InfoApp& a) {
    auto* btn = new QPushButton(a.icono, a.nombre);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setMinimumHeight(36);
    connect(btn, &QPushButton::clicked, this, [this, a]() { lanzarApp(a); });
    m_layoutApps->addWidget(btn);
}

void AppMenu::lanzarApp(const InfoApp& app) {
    QProcess::startDetached(QStringLiteral("/bin/sh"),
                            {QStringLiteral("-c"), app.comandoExec});
    close();
}

void AppMenu::lanzarComando(const QString& comando) {
    QProcess::startDetached(QStringLiteral("/bin/sh"),
                            {QStringLiteral("-c"), comando});
    close();
}

void AppMenu::alPulsarEnter() {
    const QString q = m_busqueda->text().trimmed();
    if (q.isEmpty()) return;
    for (auto it = m_grupos.constBegin(); it != m_grupos.constEnd(); ++it)
        for (const InfoApp& a : it.value())
            if (a.nombre.contains(q, Qt::CaseInsensitive)) {
                lanzarApp(a);
                return;
            }
}

void AppMenu::alPulsarEscape() {
    if (!m_busqueda->text().isEmpty()) {
        m_busqueda->clear();
        m_busqueda->setFocus();
    } else {
        close();
    }
}

// ─── Pintado ─────────────────────────────────────────────────────────────────
// Fondo opaco con esquinas redondeadas y borde sutil (igual que SystemPanel).

void AppMenu::paintEvent(QPaintEvent*) {
    QPainter p(this);

    // Perfil ligero: fondo plano opaco, sin esquinas ni borde.
    if (PerfilVisual::ligero()) {
        p.fillRect(rect(), QColor(20, 20, 26));
        return;
    }

    p.setRenderHint(QPainter::Antialiasing, true);

    QPainterPath path;
    path.addRoundedRect(rect().adjusted(0, 0, -1, -1), 12, 12);

    p.fillPath(path, QColor(20, 20, 26));
    p.setPen(QPen(QColor(255, 255, 255, 42), 1));
    p.drawPath(path);
}

// ─── Apertura ────────────────────────────────────────────────────────────────

void AppMenu::abrirSobre(const QRect& ancla, DockPosition pos) {
    adjustSize();
    const int gap = 8;
    QPoint p;

    // Anclaje alineado a la izquierda del botón del lanzador.
    switch (pos) {
    case DockPosition::Abajo:     p = QPoint(ancla.left(), ancla.top() - height() - gap);        break;
    case DockPosition::Arriba:    p = QPoint(ancla.left(), ancla.bottom() + gap);                break;
    case DockPosition::Izquierda: p = QPoint(ancla.right() + gap, ancla.top());                  break;
    case DockPosition::Derecha:   p = QPoint(ancla.left() - width() - gap, ancla.top());         break;
    }

    if (QScreen* scr = QGuiApplication::screenAt(ancla.center())) {
        QRect sg = scr->availableGeometry();
        p.setX(qBound(sg.left(), p.x(), sg.right() - width()  + 1));
        p.setY(qBound(sg.top(),  p.y(), sg.bottom() - height() + 1));
    }

    move(p);
    show();
    raise();
    m_busqueda->setFocus();
}