#include "DockWindow.h"
#include "DockItem.h"
#include "ApplicationScanner.h"
#include "SettingsDialog.h"
#include "SystemPanel.h"
#include "AppMenu.h"
#include "VentanaPopup.h"
#include "DesktopPopup.h"
#include "TrayHost.h"
#include "PerfilVisual.h"
#include "x11util.h"
#include "x11windowutils.h"

#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QApplication>
#include <QMouseEvent>
#include <QMenu>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>
#include <QGuiApplication>
#include <QTimer>
#include <QIcon>
#include <QCursor>
#include <QProcess>
#include <QCoreApplication>
#include <unistd.h>
#include <cstdlib>
#include <signal.h>
#include <cmath>

// ─── Constructor ────────────────────────────────────────────────────────────

DockWindow::DockWindow(QWidget* parent) : QWidget(parent) {
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint |
                   Qt::WindowStaysOnTopHint | Qt::Tool | Qt::WindowDoesNotAcceptFocus);
    // Perfil ligero: sin transparencia, la ventana es opaca (visual X11 sin
    // canal alfa) y Qt puede saltarse el borrado de fondo. Ahorra la
    // composición con alfa, que es la parte más costosa para la GPU.
    if (PerfilVisual::ligero()) {
        setAttribute(Qt::WA_OpaquePaintEvent);
    } else {
        setAttribute(Qt::WA_TranslucentBackground);
    }
    setAttribute(Qt::WA_NoSystemBackground);
    // Un dock/panel nunca debe robar el foco de teclado: si al hacer clic el
    // WM moviera _NET_ACTIVE_WINDOW al dock, el toggle minimizar/activar de
    // X11WindowUtils compararía contra una ventana equivocada.
    setFocusPolicy(Qt::NoFocus);
    setMouseTracking(true);

    // Forzar ya la ventana nativa para poder limpiar WM_TRANSIENT_FOR antes de
    // que el WM la vea. Qt marca las ventanas Qt::Tool como transitorias del
    // grupo líder; al relanzar el proceso ese id puede pertenecer a una ventana
    // muerta y el WM ocultaría el panel. Limpiándolo antes del mapeo se evita.
    winId();
    x11_clear_transient_for((unsigned long)winId());

    QString cfgPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
                      + "/sgp/sgp.conf";
    QDir().mkpath(QFileInfo(cfgPath).dir().absolutePath());
    m_settings = new QSettings(cfgPath, QSettings::IniFormat, this);

    m_scanner = new ApplicationScanner(this);

    m_timerWM = new QTimer(this);
    m_timerWM->setInterval(5000);    // red de seguridad; los cambios llegan por eventos X11
    connect(m_timerWM, &QTimer::timeout, this, &DockWindow::actualizarVentanas);

    m_timerAutoOcultar = new QTimer(this);
    m_timerAutoOcultar->setSingleShot(true);
    m_timerAutoOcultar->setInterval(600);
    connect(m_timerAutoOcultar, &QTimer::timeout, this, &DockWindow::ocultarDock);

    m_timerResetMag = new QTimer(this);
    m_timerResetMag->setSingleShot(true);
    m_timerResetMag->setInterval(80);
    connect(m_timerResetMag, &QTimer::timeout, this, &DockWindow::resetMagnificacion);

    // Perfil ligero: la magnificación se refresca como mucho cada ~33 ms. El
    // evento de movimiento solo guarda la posición; este timer aplica la última.
    m_timerMag = new QTimer(this);
    m_timerMag->setSingleShot(true);
    m_timerMag->setInterval(PerfilVisual::MAG_INTERVALO_MS);
    connect(m_timerMag, &QTimer::timeout, this, [this](){
        if (PerfilVisual::ligero()) actualizarMagnificacion(m_ultimoMouse);
    });

    // Posicionado por eventos: la geometría del dock se recalcula cuando
    // cambia la pantalla (hotplug/rotación/resolución) y en cada cambio de
    // configuración, ventanas o bandeja. Sin polling periódico de 2 s.
    connect(qApp, &QGuiApplication::screenAdded, this, [this](QScreen* s){
        conectarPantalla(s); posicionarDock(); establecerEstrut(); });
    connect(qApp, &QGuiApplication::screenRemoved, this, [this](QScreen* s){
        m_pantallasConectadas.remove(s); posicionarDock(); establecerEstrut(); });
    conectarPantallas();

    m_animSlide = new QPropertyAnimation(this, "posOffset", this);
    m_animSlide->setDuration(220);
    m_animSlide->setEasingCurve(QEasingCurve::InOutCubic);

    // Al salir el puntero de un item, se espera un instante para decidir si
    // cerrar el popup de ventanas (si el puntero ya está dentro del popup, se
    // mantiene abierto para poder elegir una ventana).
    m_timerCerrarPopup = new QTimer(this);
    m_timerCerrarPopup->setSingleShot(true);
    m_timerCerrarPopup->setInterval(150);
    connect(m_timerCerrarPopup, &QTimer::timeout, this, &DockWindow::cerrarPopupVentanas);

    // Cambios del WM por eventos (PropertyNotify en la root) en vez de sondear:
    // abrir/cerrar ventanas y cambio de ventana activa se reflejan al instante
    // y en reposo el dock no hace trabajo periódico de ventanas.
    m_watchDpy = x11_wm_watch_open();
    if (m_watchDpy) {
        m_watchFd = x11_wm_watch_fd(m_watchDpy);
        if (m_watchFd >= 0) {
            m_notifWM = new QSocketNotifier(m_watchFd, QSocketNotifier::Read, this);
            connect(m_notifWM, &QSocketNotifier::activated,
                    this, &DockWindow::alEventoWM);
        }
    }

    cargarConfig();

    // Botón del lanzador de la punta izquierda: abre el menú de aplicaciones
    // ordenadas por categoría. En dock compacto queda siempre primero.
    auto* lanzador = new DockItem(DockItem::Tipo::Lanzador, this, this);
    lanzador->setNombre(QStringLiteral("Lanzador"));
    lanzador->setIcono(QIcon(QStringLiteral(":/launcher/lanzador.png")));
    conectarItem(lanzador);
    lanzador->show();
    m_items.append(lanzador);

    // Botón de escritorios virtuales: junto al lanzador, abre un popup con
    // puntos para cambiar de escritorio (el activo se marca en azul).
    auto* escritorios = new DockItem(DockItem::Tipo::Escritorios, this, this);
    escritorios->setNombre(QStringLiteral("Escritorios"));
    escritorios->setIcono(QIcon(QStringLiteral(":/launcher/desktop.png")));
    conectarItem(escritorios);
    escritorios->show();
    m_items.append(escritorios);

    cargarAppsAncladas();

    // Bloque fijo del final del dock: separador + reloj.
    auto* sep = new DockItem(DockItem::Tipo::Separador, this, this);
    conectarItem(sep);
    sep->show();
    m_items.append(sep);

    auto* reloj = new DockItem(DockItem::Tipo::Reloj, this, this);
    conectarItem(reloj);
    reloj->show();
    m_items.append(reloj);

    // Bandeja del sistema X11 (XEmbed): se coloca al final del dock y solo
    // se muestra cuando hay iconos. setup() se invoca en showEvent (cuando la
    // ventana nativa existe) para adquirir _NET_SYSTEM_TRAY_S0.
    const bool dockHorizontal = (m_cfg.posicion == DockPosition::Abajo ||
                                 m_cfg.posicion == DockPosition::Arriba);
    m_trayHost = new TrayHost(m_cfg.tamanoTray, !dockHorizontal, this);
    m_trayHost->hide();
    connect(m_trayHost, &TrayHost::geometryChanged, this, &DockWindow::layoutItems);

    reconstruirDock();
    m_timerWM->start();
}

DockWindow::~DockWindow() {
    if (m_notifWM) { delete m_notifWM; m_notifWM = nullptr; }
    if (m_watchDpy) { x11_wm_watch_close(m_watchDpy); m_watchDpy = nullptr; }
    guardarConfig();
    guardarAppsAncladas();
    x11_clear_strut((unsigned long)winId());
}

// ─── Config ─────────────────────────────────────────────────────────────────

void DockWindow::cargarConfig() {
    m_settings->beginGroup("Dock");
    m_cfg.posicion           = (DockPosition)m_settings->value("posicion", 0).toInt();
    m_cfg.comportamiento     = (DockComportamiento)m_settings->value("comportamiento", 0).toInt();
    m_cfg.tamanoIcono        = m_settings->value("tamanoIcono", 32).toInt();
    m_cfg.magnificacion      = m_settings->value("magnificacion", true).toBool();
    m_cfg.multiplicadorMag   = m_settings->value("multiplicadorMag", 160).toInt();
    m_cfg.fondoTranslucido   = m_settings->value("fondoTranslucido", true).toBool();
    m_cfg.opacidadFondo      = m_settings->value("opacidadFondo", 0.80).toDouble();
    m_cfg.mostrarIndicadores = m_settings->value("mostrarIndicadores", true).toBool();
    m_cfg.mostrarEtiquetas   = m_settings->value("mostrarEtiquetas", true).toBool();
    m_cfg.rebotarAlAbrir     = m_settings->value("rebotarAlAbrir", true).toBool();
    m_cfg.indiceMonitor      = m_settings->value("indiceMonitor", 0).toInt();
    m_cfg.radioEsquina       = m_settings->value("radioEsquina", 16).toInt();
    m_cfg.espaciadoItems     = m_settings->value("espaciadoItems", 6).toInt();
    m_cfg.mostrarTray        = m_settings->value("mostrarTray", true).toBool();
    m_cfg.tamanoTray         = m_settings->value("tamanoTray", 24).toInt();
    m_cfg.modoPanel          = m_settings->value("modoPanel", true).toBool();
    m_cfg.altoPanel          = m_settings->value("altoPanel", 42).toInt();
    m_settings->endGroup();
}

void DockWindow::guardarConfig() {
    m_settings->beginGroup("Dock");
    m_settings->setValue("posicion",           (int)m_cfg.posicion);
    m_settings->setValue("comportamiento",     (int)m_cfg.comportamiento);
    m_settings->setValue("tamanoIcono",        m_cfg.tamanoIcono);
    m_settings->setValue("magnificacion",      m_cfg.magnificacion);
    m_settings->setValue("multiplicadorMag",   m_cfg.multiplicadorMag);
    m_settings->setValue("fondoTranslucido",   m_cfg.fondoTranslucido);
    m_settings->setValue("opacidadFondo",      m_cfg.opacidadFondo);
    m_settings->setValue("mostrarIndicadores", m_cfg.mostrarIndicadores);
    m_settings->setValue("mostrarEtiquetas",   m_cfg.mostrarEtiquetas);
    m_settings->setValue("rebotarAlAbrir",     m_cfg.rebotarAlAbrir);
    m_settings->setValue("indiceMonitor",      m_cfg.indiceMonitor);
    m_settings->setValue("radioEsquina",       m_cfg.radioEsquina);
    m_settings->setValue("espaciadoItems",     m_cfg.espaciadoItems);
    m_settings->setValue("mostrarTray",        m_cfg.mostrarTray);
    m_settings->setValue("tamanoTray",         m_cfg.tamanoTray);
    m_settings->setValue("modoPanel",          m_cfg.modoPanel);
    m_settings->setValue("altoPanel",          m_cfg.altoPanel);
    m_settings->endGroup();
    m_settings->sync();
}

void DockWindow::cargarAppsAncladas() {
    QStringList defaults;
    QStringList candidatos = {
        "/usr/share/applications/org.gnome.Nautilus.desktop",
        "/usr/share/applications/nautilus.desktop",
        "/usr/share/applications/nemo.desktop",
        "/usr/share/applications/thunar.desktop",
        "/usr/share/applications/firefox.desktop",
        "/usr/share/applications/firefox-esr.desktop",
        "/usr/share/applications/chromium-browser.desktop",
        "/usr/share/applications/google-chrome.desktop",
        "/usr/share/applications/org.gnome.Terminal.desktop",
        "/usr/share/applications/gnome-terminal.desktop",
        "/usr/share/applications/konsole.desktop",
        "/usr/share/applications/org.gnome.gedit.desktop",
        "/usr/share/applications/gedit.desktop",
        "/usr/share/applications/org.gnome.Settings.desktop",
        "/usr/share/applications/gnome-control-center.desktop",
    };
    for (const QString& c : candidatos) {
        if (QFile::exists(c)) defaults << c;
        if (defaults.size() >= 5) break;
    }

    m_settings->beginGroup("AppsAncladas");
    QStringList ancladas = m_settings->value("lista", defaults).toStringList();
    m_settings->endGroup();

    for (const QString& df : ancladas) {
        InfoApp info = m_scanner->buscarPorDesktop(df);
        if (info.nombre.isEmpty()) {
            if (!QFile::exists(df)) continue;
            info.archivoDesktop = df;
            info.nombre = QFileInfo(df).baseName();
            info.icono  = QIcon::fromTheme("application-x-executable");
        }
        info.estaAnclada = true;
        auto* item = new DockItem(info, this, this);
        conectarItem(item);
        item->show();
        insertarItem(item);
    }
}

void DockWindow::guardarAppsAncladas() {
    QStringList lista;
    for (DockItem* it : m_items)
        if (it->tipo() == DockItem::Tipo::Aplicacion && it->estaAnclada())
            lista << it->infoApp().archivoDesktop;
    m_settings->beginGroup("AppsAncladas");
    m_settings->setValue("lista", lista);
    m_settings->endGroup();
    m_settings->sync();
}

void DockWindow::aplicarConfig(const DockConfig& cfg) {
    m_cfg = cfg;
    guardarConfig();
    for (DockItem* it : m_items) { it->setTamanoMagnificado(0); it->update(); }
    if (m_trayHost) {
        m_trayHost->setIconSize(cfg.tamanoTray);
        m_trayHost->setVertical(cfg.posicion == DockPosition::Izquierda ||
                                cfg.posicion == DockPosition::Derecha);
    }
    x11_clear_strut((unsigned long)winId());
    layoutItems();
    posicionarDock();
    establecerEstrut();
    update();
}

// ─── Items ──────────────────────────────────────────────────────────────────

void DockWindow::conectarItem(DockItem* item) {
    connect(item, &DockItem::clicIzquierdo, this, [this, item](){ alHacerClicItem(item); });
    connect(item, &DockItem::clicDerecho,   this, [this, item](QPoint pos){ alHacerClicDerechoItem(item, pos); });
    connect(item, &DockItem::clicMedio,     this, [this, item](){
        if (item->tipo() == DockItem::Tipo::Lanzador) abrirMenuApps(item);
        else if (item->tipo() == DockItem::Tipo::Reloj) abrirPanelSistema(item);
        else if (item->tipo() == DockItem::Tipo::Escritorios) abrirPopupEscritorios(item);
        else item->lanzar();
    });
    // Hover sobre un item con varias ventanas: mostrar el popup para elegir.
    connect(item, &DockItem::solicitudPopupVentanas, this, [this, item](){
        m_timerCerrarPopup->stop();
        abrirPopupVentanas(item);
    });
    // Al salir el puntero del item, se cierra el popup si ya no está sobre él.
    connect(item, &DockItem::salidaHover, this, [this, item](){
        if (m_popupVentanas && m_popupVentanas->isVisible())
            m_timerCerrarPopup->start();
    });
}

void DockWindow::reconstruirDock() {
    for (DockItem* it : m_items) { disconnect(it, nullptr, this, nullptr); conectarItem(it); }
    layoutItems();
    posicionarDock();
    establecerEstrut();
}

void DockWindow::anclarApp(const QString& df) {
    for (DockItem* it : m_items)
        if (it->infoApp().archivoDesktop == df) { it->setAnclada(true); return; }
    InfoApp info = m_scanner->buscarPorDesktop(df);
    if (info.nombre.isEmpty()) return;
    info.estaAnclada = true;
    auto* item = new DockItem(info, this, this);
    conectarItem(item);
    item->show();
    insertarItem(item);
    layoutItems();
    posicionarDock();
    guardarAppsAncladas();
}

void DockWindow::desanclarApp(const QString& df) {
    for (int i = 0; i < m_items.size(); i++) {
        DockItem* it = m_items[i];
        if (it->infoApp().archivoDesktop == df) {
            if (it->estaEjecutando()) { it->setAnclada(false); }
            else { m_items.removeAt(i); it->deleteLater(); layoutItems(); posicionarDock(); }
            guardarAppsAncladas();
            return;
        }
    }
}

void DockWindow::insertarItem(DockItem* item) {
    // Inserta un item de aplicación antes del bloque fijo del final
    // (separador + reloj) para que el reloj quede siempre al final del dock.
    int idx = m_items.size();
    for (int i = m_items.size() - 1; i >= 0; i--) {
        DockItem::Tipo t = m_items[i]->tipo();
        if (t == DockItem::Tipo::Reloj || t == DockItem::Tipo::Separador) idx = i;
        else break;
    }
    m_items.insert(idx, item);
}

// ─── Layout ─────────────────────────────────────────────────────────────────

void DockWindow::layoutItems() {
    bool horizontal = (m_cfg.posicion == DockPosition::Abajo ||
                       m_cfg.posicion == DockPosition::Arriba);
    int esp    = m_cfg.espaciadoItems;
    int pad    = m_cfg.padding;
    int grosor = m_cfg.modoPanel ? m_cfg.altoPanel
                                 : m_cfg.tamanoIcono + 20 + pad * 2;
    const bool panel = m_cfg.modoPanel;

    // Bandeja del sistema: visible solo si hay iconos embebidos.
    const bool trayVisible = m_trayHost && m_cfg.mostrarTray &&
                             m_trayHost->isActive() && m_trayHost->iconCount() > 0;

    // Ancho/alto total del contenido (íconos + separador + reloj + bandeja).
    auto contenidoW = [&]() {
        int total = pad;
        for (DockItem* it : m_items) total += it->tamano().width() + esp;
        if (!m_items.isEmpty()) total -= esp;
        if (trayVisible) total += esp + m_trayHost->width();
        return total + pad;
    };
    auto contenidoH = [&]() {
        int total = pad;
        for (DockItem* it : m_items) total += it->tamano().height() + esp;
        if (!m_items.isEmpty()) total -= esp;
        if (trayVisible) total += esp + m_trayHost->height();
        return total + pad;
    };

    if (horizontal) {
        QScreen* s = pantallaObjetivo();
        if (panel) {
            // Modo panel (estilo Chrome OS): el dock ocupa todo el ancho del
            // borde (menos el margen). Los iconos de aplicaciones quedan
            // juntos y centrados; el reloj se ancla a la punta derecha (dentro
            // del panel) con la bandeja y el separador a su izquierda. Las
            // esquinas redondas se conservan (el fondo usa addRoundedRect).
            resize(s->geometry().width() - 2 * m_cfg.margen, grosor);

            DockItem* sepItem       = nullptr;
            DockItem* relojItem     = nullptr;
            DockItem* lanzadorItem  = nullptr;
            DockItem* escritoriosItem = nullptr;
            QList<DockItem*> mainItems;
            for (DockItem* it : m_items) {
                if      (it->tipo() == DockItem::Tipo::Reloj)       relojItem = it;
                else if (it->tipo() == DockItem::Tipo::Separador)   sepItem   = it;
                else if (it->tipo() == DockItem::Tipo::Lanzador)    lanzadorItem = it;
                else if (it->tipo() == DockItem::Tipo::Escritorios) escritoriosItem = it;
                else    mainItems.append(it);
            }

            const int trayW = trayVisible ? m_trayHost->width() : 0;
            int leftW = 0;
            if (lanzadorItem)     leftW += lanzadorItem->tamano().width();
            if (escritoriosItem)  leftW += escritoriosItem->tamano().width() + esp;

            // Ancho del bloque derecho anclado a la derecha (reloj a la punta,
            // bandeja y separador a su izquierda).
            int endW = 0;
            if (relojItem) endW += relojItem->tamano().width();
            if (sepItem)   endW += sepItem->tamano().width() + esp;
            if (trayVisible) endW += esp + trayW;

            // Bloque principal (apps) juntos y centrados en el espacio que
            // dejan el lanzador (punta izquierda) y el bloque derecho.
            int mainW = 0;
            for (DockItem* it : mainItems) mainW += it->tamano().width();
            if (!mainItems.isEmpty()) mainW += (mainItems.size() - 1) * esp;

            int xMain = pad + leftW
                      + qMax(0, (width() - pad - leftW - endW - pad - mainW) / 2);

            // Punta izquierda: lanzador (menú por categorías) y a su derecha el
            // botón de escritorios virtuales.
            int x = pad;
            if (lanzadorItem) {
                int w = lanzadorItem->tamano().width();
                lanzadorItem->setGeometry(x, 0, w, grosor);
                lanzadorItem->show();
                x += w + esp;
            }
            if (escritoriosItem) {
                int w = escritoriosItem->tamano().width();
                escritoriosItem->setGeometry(x, 0, w, grosor);
                escritoriosItem->show();
            }

            // Apps centradas.
            x = xMain;
            for (DockItem* it : mainItems) {
                int w = it->tamano().width();
                it->setGeometry(x, 0, w, grosor);
                it->show();
                x += w + esp;
            }

            // Bloque derecho: reloj en la punta derecha del panel.
            int xRight = width() - pad;
            if (relojItem) {
                int w = relojItem->tamano().width();
                xRight -= w;
                relojItem->setGeometry(xRight, 0, w, grosor);
                relojItem->show();
            }
            if (trayVisible) {
                const int th = m_trayHost->height();
                xRight -= esp + trayW;
                m_trayHost->setGeometry(xRight, (grosor - th) / 2, trayW, th);
                m_trayHost->show();
            } else if (m_trayHost) {
                m_trayHost->hide();
            }
            if (sepItem) {
                int w = sepItem->tamano().width();
                xRight -= esp + w;
                sepItem->setGeometry(xRight, 0, w, grosor);
                sepItem->show();
            }
        } else {
            resize(contenidoW(), grosor);
            int x = pad;
            for (DockItem* it : m_items) {
                int w = it->tamano().width();
                it->setGeometry(x, 0, w, grosor);
                it->show();
                x += w + esp;
            }
            if (trayVisible) {
                const int tw = m_trayHost->width();
                const int th = m_trayHost->height();
                m_trayHost->setGeometry(x + esp, (grosor - th) / 2, tw, th);
                m_trayHost->show();
            } else if (m_trayHost) {
                m_trayHost->hide();
            }
        }
    } else {
        QScreen* s = pantallaObjetivo();
        if (panel) {
            resize(grosor, s->geometry().height() - 2 * m_cfg.margen);

            DockItem* sepItem       = nullptr;
            DockItem* relojItem     = nullptr;
            DockItem* lanzadorItem  = nullptr;
            DockItem* escritoriosItem = nullptr;
            QList<DockItem*> mainItems;
            for (DockItem* it : m_items) {
                if      (it->tipo() == DockItem::Tipo::Reloj)       relojItem = it;
                else if (it->tipo() == DockItem::Tipo::Separador)   sepItem   = it;
                else if (it->tipo() == DockItem::Tipo::Lanzador)    lanzadorItem = it;
                else if (it->tipo() == DockItem::Tipo::Escritorios) escritoriosItem = it;
                else    mainItems.append(it);
            }

            const int trayH = trayVisible ? m_trayHost->height() : 0;
            int topH = 0;
            if (lanzadorItem)    topH += lanzadorItem->tamano().height();
            if (escritoriosItem) topH += escritoriosItem->tamano().height() + esp;

            // Bloque inferior anclado abajo (reloj en la punta, bandeja y
            // separador encima).
            int endH = 0;
            if (relojItem) endH += relojItem->tamano().height();
            if (sepItem)   endH += sepItem->tamano().height() + esp;
            if (trayVisible) endH += esp + trayH;

            // Bloque principal (apps) juntos y centrados entre el lanzador
            // (punta superior) y el bloque inferior.
            int mainH = 0;
            for (DockItem* it : mainItems) mainH += it->tamano().height();
            if (!mainItems.isEmpty()) mainH += (mainItems.size() - 1) * esp;

            int yMain = pad + topH
                      + qMax(0, (height() - pad - topH - endH - pad - mainH) / 2);

            // Punta superior: lanzador (menú por categorías) y debajo el botón de
            // escritorios virtuales.
            int y = pad;
            if (lanzadorItem) {
                int h = lanzadorItem->tamano().height();
                lanzadorItem->setGeometry(0, y, grosor, h);
                lanzadorItem->show();
                y += h + esp;
            }
            if (escritoriosItem) {
                int h = escritoriosItem->tamano().height();
                escritoriosItem->setGeometry(0, y, grosor, h);
                escritoriosItem->show();
            }

            // Apps centradas.
            y = yMain;
            for (DockItem* it : mainItems) {
                int h = it->tamano().height();
                it->setGeometry(0, y, grosor, h);
                it->show();
                y += h + esp;
            }

            // Bloque inferior: reloj en la punta inferior del panel.
            int yBottom = height() - pad;
            if (relojItem) {
                int h = relojItem->tamano().height();
                yBottom -= h;
                relojItem->setGeometry(0, yBottom, grosor, h);
                relojItem->show();
            }
            if (trayVisible) {
                const int tw = m_trayHost->width();
                yBottom -= esp + trayH;
                m_trayHost->setGeometry((grosor - tw) / 2, yBottom, tw, trayH);
                m_trayHost->show();
            } else if (m_trayHost) {
                m_trayHost->hide();
            }
            if (sepItem) {
                int h = sepItem->tamano().height();
                yBottom -= esp + h;
                sepItem->setGeometry(0, yBottom, grosor, h);
                sepItem->show();
            }
        } else {
            resize(grosor, contenidoH());
            int y = pad;
            for (DockItem* it : m_items) {
                int h = it->tamano().height();
                it->setGeometry(0, y, grosor, h);
                it->show();
                y += h + esp;
            }
            if (trayVisible) {
                const int tw = m_trayHost->width();
                const int th = m_trayHost->height();
                m_trayHost->setGeometry((grosor - tw) / 2, y + esp, tw, th);
                m_trayHost->show();
            } else if (m_trayHost) {
                m_trayHost->hide();
            }
        }
    }
}

QScreen* DockWindow::pantallaObjetivo() const {
    QList<QScreen*> pantallas = QApplication::screens();
    int idx = qBound(0, m_cfg.indiceMonitor, pantallas.size()-1);
    return pantallas[idx];
}

void DockWindow::conectarPantalla(QScreen* s) {
    if (!s || m_pantallasConectadas.contains(s)) return;
    m_pantallasConectadas.insert(s);
    auto reposition = [this](){ posicionarDock(); establecerEstrut(); };
    connect(s, &QScreen::geometryChanged,         this, reposition);
    connect(s, &QScreen::virtualGeometryChanged,  this, reposition);
    connect(s, &QScreen::availableGeometryChanged, this, reposition);
}

void DockWindow::conectarPantallas() {
    for (QScreen* s : QApplication::screens()) conectarPantalla(s);
}

QRect DockWindow::geometriaDock() const {
    QScreen* s  = pantallaObjetivo();
    QRect    sg = s->geometry();
    int grosor = m_cfg.modoPanel ? m_cfg.altoPanel
                             : m_cfg.tamanoIcono + 20 + m_cfg.padding * 2;
    int mg      = m_cfg.margen;

    // Modo panel: el dock se extiende por todo el borde de la pantalla,
    // conservando un margen en las puntas (donde quedan las esquinas redondas).
    if (m_cfg.modoPanel) {
        switch (m_cfg.posicion) {
        case DockPosition::Abajo:
            return QRect(sg.x() + mg, sg.y() + sg.height() - grosor - mg,
                         sg.width() - 2 * mg, grosor);
        case DockPosition::Arriba:
            return QRect(sg.x() + mg, sg.y() + mg,
                         sg.width() - 2 * mg, grosor);
        case DockPosition::Izquierda:
            return QRect(sg.x() + mg, sg.y() + mg,
                         grosor, sg.height() - 2 * mg);
        case DockPosition::Derecha:
            return QRect(sg.x() + sg.width() - grosor - mg, sg.y() + mg,
                         grosor, sg.height() - 2 * mg);
        }
        return {};
    }

    switch (m_cfg.posicion) {
    case DockPosition::Abajo:
        return QRect(sg.x() + (sg.width()  - width())  / 2,
                     sg.y() + sg.height() - grosor - mg,
                     width(), grosor);
    case DockPosition::Arriba:
        return QRect(sg.x() + (sg.width()  - width())  / 2,
                     sg.y() + mg, width(), grosor);
    case DockPosition::Izquierda:
        return QRect(sg.x() + mg,
                     sg.y() + (sg.height() - height()) / 2,
                     grosor, height());
    case DockPosition::Derecha:
        return QRect(sg.x() + sg.width() - grosor - mg,
                     sg.y() + (sg.height() - height()) / 2,
                     grosor, height());
    }
    return {};
}

void DockWindow::posicionarDock() {
    layoutItems();
    QRect r = geometriaDock();
    move(r.topLeft());
}

void DockWindow::setPosOffset(int v) {
    m_posOffset = v;
    QRect base = geometriaDock();
    switch (m_cfg.posicion) {
    case DockPosition::Abajo:     move(base.x(), base.y() + v); break;
    case DockPosition::Arriba:    move(base.x(), base.y() - v); break;
    case DockPosition::Izquierda: move(base.x() - v, base.y()); break;
    case DockPosition::Derecha:   move(base.x() + v, base.y()); break;
    }
}

// ─── Paint ──────────────────────────────────────────────────────────────────

void DockWindow::paintEvent(QPaintEvent*) {
    QPainter p(this);

    // Perfil ligero: fondo plano, opaco y sin esquinas redondeadas ni bordes.
    // Es la mayor fuente de ahorro para la GPU (nada de alfa por píxel ni
    // antialiasing de bordes).
    if (PerfilVisual::ligero()) {
        p.fillRect(rect(), PerfilVisual::colorFondoLigero());
        return;
    }

    p.setRenderHint(QPainter::Antialiasing);
    QRect bg = rect().adjusted(2, 2, -2, -2);
    int r = m_cfg.radioEsquina;
    QPainterPath path;
    path.addRoundedRect(bg, r, r);
    QColor bgColor(30, 30, 30, (int)(m_cfg.opacidadFondo * 255));
    p.fillPath(path, bgColor);
    p.setPen(QPen(QColor(255,255,255,45), 1.2));
    p.drawPath(path);
    QPainterPath inner;
    inner.addRoundedRect(bg.adjusted(1,1,-1,-1), r-1, r-1);
    p.setPen(QPen(QColor(255,255,255,18), 1));
    p.drawPath(inner);
}

// ─── WM ─────────────────────────────────────────────────────────────────────

void DockWindow::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    // Un dock jamás debe ser ventana "transient" (ver x11_clear_transient_for):
    // al relanzar el proceso el valor puede quedar apuntando a una ventana
    // muerta y el WM no mostraría el panel.
    x11_clear_transient_for((unsigned long)winId());
    // La bandeja necesita la ventana nativa: adquirirla ahora.
    if (m_trayHost)
        m_trayHost->setup();
    QTimer::singleShot(150, this, [this](){
        configurarWM();
        establecerEstrut();
        posicionarDock();
    });
}

void DockWindow::configurarWM() {
    x11_set_dock_type((unsigned long)winId());
    // Reafirmar tras la primera gestión del WM (por si re-aplicó el transient).
    x11_clear_transient_for((unsigned long)winId());
}

void DockWindow::establecerEstrut() {
    unsigned long wid = (unsigned long)winId();
    if (!wid) return;
    QScreen* s  = pantallaObjetivo();
    QRect    sg = s->geometry();
    int grosor  = m_cfg.modoPanel
                  ? m_cfg.altoPanel + m_cfg.margen
                  : m_cfg.tamanoIcono + 20 + m_cfg.padding * 2 + m_cfg.margen;
    x11_set_strut(wid, sg.x(), sg.y(), sg.width(), sg.height(),
                  (int)m_cfg.posicion, grosor);
    // El WM puede "re-construir" la ventana dentro del workarea cuando cambia
    // el strut (nuestro strut recorta el workarea y la ventana queda fuera):
    // reafirmamos la posición tras el ciclo de trabajo del WM.
    QTimer::singleShot(400, this, &DockWindow::posicionarDock);
}

void DockWindow::limpiarEstrut() {
    x11_clear_strut((unsigned long)winId());
}

// ─── Window Tracking ────────────────────────────────────────────────────────

void DockWindow::alEventoWM() {
    // Solo releer cuando el servidor notifica un cambio real.
    static const unsigned long atomClientes = x11_atom("_NET_CLIENT_LIST");
    static const unsigned long atomActiva   = x11_atom("_NET_ACTIVE_WINDOW");
    if (x11_wm_watch_dispatch(m_watchDpy, atomClientes, atomActiva))
        actualizarVentanas();
}

void DockWindow::actualizarVentanas() {
    static const int MAX_WINS = 256;
    X11WindowInfo wins[MAX_WINS];
    int n = x11_get_client_list(wins, MAX_WINS);

    QSet<quint64> actuales;
    bool layoutChanged = false;

    for (int i = 0; i < n; i++) {
        quint64 wid = (quint64)wins[i].wid;
        if (wid == (quint64)winId()) continue;
        actuales.insert(wid);

        QString wmClass = QString::fromLocal8Bit(wins[i].wmClass).toLower();
        QString wmName  = QString::fromLocal8Bit(wins[i].wmName).toLower();

        // Ventanas del Stellar Launcher (si se ejecuta manualmente fuera del dock):
        // "sgp" es la clase de las ventanas internas del propio dock (incluidos
        // los popups), que no generan items.
        if (wmClass == "sgp")
            continue;

        DockItem* match = nullptr;
        for (DockItem* it : m_items) {
            if (it->tipo() != DockItem::Tipo::Aplicacion) continue;
            InfoApp inf  = it->infoApp();
            QString iwm  = inf.wmClass.toLower();
            QString iname= inf.nombre.toLower();
            QString iexe = QFileInfo(inf.comandoExec.split(' ').first()).baseName().toLower();
            if ((!iwm.isEmpty() && (wmClass == iwm || wmName == iwm)) ||
                wmClass == iname || wmClass == iexe || wmName == iname || wmName == iexe)
            { match = it; break; }
        }

        if (!match) {
            InfoApp info = m_scanner->buscarPorWmClass(wmClass);
            if (info.nombre.isEmpty()) info = m_scanner->buscarPorWmClass(wmName);
            if (!info.nombre.isEmpty()) {
                bool yaExiste = false;
                for (DockItem* it : m_items)
                    if (it->infoApp().archivoDesktop == info.archivoDesktop)
                        { yaExiste = true; match = it; break; }
                if (!yaExiste) {
                    auto* newItem = new DockItem(info, this, this);
                    conectarItem(newItem);
                    newItem->show();
                    insertarItem(newItem);
                    layoutChanged = true;
                    match = newItem;
                }
            }
        }
        if (match) match->agregarVentana(wid);
    }

    // Limpiar ventanas muertas
    for (int i = m_items.size()-1; i >= 0; i--) {
        DockItem* it = m_items[i];
        if (it->tipo() != DockItem::Tipo::Aplicacion) continue;
        for (quint64 wid : it->ventanas())
            if (!actuales.contains(wid)) it->quitarVentana(wid);
        if (!it->estaAnclada() && !it->estaEjecutando()) {
            m_items.removeAt(i); it->deleteLater(); layoutChanged = true;
        }
    }

    // Ventana activa
    quint64 active = (quint64)x11_get_active_window();
    for (DockItem* it : m_items)
        it->setActiva(active && it->ventanas().contains(active));

    if (layoutChanged) { layoutItems(); posicionarDock(); }
}

// ─── Magnification ──────────────────────────────────────────────────────────

void DockWindow::actualizarMagnificacion(const QPoint& mouse) {
    if (!m_cfg.magnificacion) return;
    bool horizontal = (m_cfg.posicion == DockPosition::Abajo ||
                       m_cfg.posicion == DockPosition::Arriba);
    int base  = m_cfg.tamanoIcono;
    int maxSz = base * m_cfg.multiplicadorMag / 100;
    int rango = base * 2;
    for (DockItem* it : m_items) {
        if (it->tipo() == DockItem::Tipo::Separador ||
            it->tipo() == DockItem::Tipo::Reloj) continue;
        QPoint centro = it->mapToParent(it->rect().center());
        int dist = horizontal ? abs(mouse.x()-centro.x()) : abs(mouse.y()-centro.y());
        int sz;
        if (dist < rango) {
            double t = 1.0 - (double)dist/rango;
            t = t*t*(3-2*t);
            sz = base + (int)((maxSz-base)*t);
        } else sz = base;
        it->setTamanoMagnificado(sz);
    }
    layoutItems();
}

void DockWindow::resetMagnificacion() {
    for (DockItem* it : m_items) it->setTamanoMagnificado(0);
    layoutItems(); posicionarDock();
}

// ─── Auto-hide ──────────────────────────────────────────────────────────────

void DockWindow::mostrarDock() {
    if (!m_oculto) return;
    m_oculto = false;
    m_animSlide->stop();
    m_animSlide->setStartValue(m_posOffset);
    m_animSlide->setEndValue(0);
    m_animSlide->start();
}

void DockWindow::ocultarDock() {
    if (m_oculto || m_mouseAdentro) return;
    m_oculto = true;
    int grosor = m_cfg.modoPanel ? m_cfg.altoPanel
                                 : m_cfg.tamanoIcono + 20 + m_cfg.padding * 2;
    m_animSlide->stop();
    m_animSlide->setStartValue(m_posOffset);
    m_animSlide->setEndValue(grosor - 3);
    m_animSlide->start();
}

// ─── Mouse ──────────────────────────────────────────────────────────────────

void DockWindow::enterEvent(QEnterEvent* e) {
    m_mouseAdentro = true;
    m_timerAutoOcultar->stop();
    if (m_cfg.comportamiento != DockComportamiento::SiempreVisible) mostrarDock();
    QWidget::enterEvent(e);
}

void DockWindow::leaveEvent(QEvent* e) {
    m_mouseAdentro = false;
    if (m_cfg.comportamiento != DockComportamiento::SiempreVisible) m_timerAutoOcultar->start();
    m_timerResetMag->start();
    QWidget::leaveEvent(e);
}

void DockWindow::mouseMoveEvent(QMouseEvent* e) {
    const QPoint pos = e->pos();
    if (PerfilVisual::ligero()) {
        // Magnificación a ~30 fps: guardamos la última posición y aplicamos
        // como mucho una vez cada 33 ms; los eventos intermedios solo actualizan.
        m_ultimoMouse = pos;
        if (!m_timerMag->isActive()) {
            actualizarMagnificacion(pos);
            m_timerMag->start();
        }
    } else {
        actualizarMagnificacion(pos);
    }
    QWidget::mouseMoveEvent(e);
}

void DockWindow::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::RightButton) {
        bool sobreItem = false;
        for (DockItem* it : m_items)
            if (it->geometry().contains(e->pos())) { sobreItem = true; break; }
        if (!sobreItem) menuContextualDock(e->globalPosition().toPoint());
    }
    QWidget::mouseReleaseEvent(e);
}

// ─── Item Actions ────────────────────────────────────────────────────────────

void DockWindow::alHacerClicItem(DockItem* item) {
    // El botón del lanzador de la punta izquierda abre el menú de aplicaciones
    // ordenadas por categoría.
    if (item->tipo() == DockItem::Tipo::Lanzador) { abrirMenuApps(item); return; }
    if (item->tipo() == DockItem::Tipo::Reloj)    { abrirPanelSistema(item); return; }
    // El botón de escritorios virtuales abre el popup con los puntos para
    // cambiar de escritorio (el activo en azul).
    if (item->tipo() == DockItem::Tipo::Escritorios) { abrirPopupEscritorios(item); return; }
    if (item->estaEjecutando()) {
        auto wins = item->ventanas();
        // Varias ventanas abiertas (aplicación agrupada): popup para elegir.
        if (wins.size() > 1) { abrirPopupVentanas(item); return; }
        // Comportamiento estándar de dock al hacer clic en el icono:
        // si la ventana ya tiene el foco -> minimizar; si no -> traer al frente.
        if (!wins.isEmpty())
            X11WindowUtils::toggleWindowFocusOrMinimize((WId)wins.first());
    } else {
        item->lanzar();
    }
}

static QString estiloMenu();

void DockWindow::abrirPanelSistema(DockItem* item) {
    // El panel es un Qt::Popup: se crea una vez y se reutiliza. Se ancla junto
    // al rectángulo global del reloj (coordenadas de pantalla).
    if (!m_panelSistema) m_panelSistema = new SystemPanel(this);
    const QRect ancla(item->mapToGlobal(QPoint(0, 0)), item->size());
    m_panelSistema->abrirSobre(ancla, m_cfg.posicion);
}

void DockWindow::abrirPopupVentanas(DockItem* item) {
    if (item->tipo() != DockItem::Tipo::Aplicacion) return;
    if (!m_popupVentanas) m_popupVentanas = new VentanaPopup(this);
    m_itemPopup = item;
    const QRect ancla(item->mapToGlobal(QPoint(0, 0)), item->size());
    m_popupVentanas->abrirSobre(ancla, m_cfg.posicion, item->ventanas(),
                                item->infoApp().icono, item->infoApp().nombre);
}

void DockWindow::cerrarPopupVentanas() {
    if (!m_popupVentanas || !m_popupVentanas->isVisible()) return;
    const QPoint c = QCursor::pos();
    // El popup se mantiene mientras el puntero esté sobre el popup o sobre el
    // item que lo abrió (Qt envía un leave espurio al item cuando el popup se
    // muestra y agarra el mouse; el cursor aún puede estar sobre el item).
    QRect rItem;
    if (m_itemPopup) {
        rItem = QRect(m_itemPopup->mapToGlobal(QPoint(0, 0)), m_itemPopup->size());
        rItem.adjust(-8, -8, 8, 8);   // margen: no parpadear al bordear
    }
    if (!m_popupVentanas->geometry().contains(c) && !rItem.contains(c))
        m_popupVentanas->hide();
}

void DockWindow::abrirMenuApps(DockItem* item) {
    // Menú popup con las aplicaciones ordenadas por categoría, anclado al
    // botón del lanzador de la punta izquierda del panel. Se reconstruye en
    // cada apertura para reflejar aplicaciones instaladas recientemente.
    delete m_menuApps;
    m_menuApps = new AppMenu(m_scanner->todasLasApps(), this);
    const QRect ancla(item->mapToGlobal(QPoint(0, 0)), item->size());
    m_menuApps->abrirSobre(ancla, m_cfg.posicion);
}

void DockWindow::abrirPopupEscritorios(DockItem* item) {
    // Popup de escritorios virtuales: puntos horizontales con el escritorio
    // activo en azul. Se reutiliza una única instancia.
    if (!m_popupEscritorios) m_popupEscritorios = new DesktopPopup(this);
    const QRect ancla(item->mapToGlobal(QPoint(0, 0)), item->size());
    m_popupEscritorios->abrirSobre(ancla, m_cfg.posicion);
}

void DockWindow::alHacerClicDerechoItem(DockItem* item, const QPoint& pos) {
    if (item->tipo() == DockItem::Tipo::Reloj) {
        QMenu menu;
        menu.setStyleSheet(estiloMenu());
        auto* hdr = menu.addAction(QStringLiteral("Reloj"));
        hdr->setEnabled(false);
        QFont f = hdr->font(); f.setBold(true); hdr->setFont(f);
        menu.addSeparator();
        menu.addAction(QStringLiteral("Preferencias del Dock…"), [this](){ abrirPreferencias(); });
        menu.exec(pos);
        return;
    }
    if (item->tipo() == DockItem::Tipo::Lanzador) {
        QMenu menu;
        menu.setStyleSheet(estiloMenu());
        auto* hdr = menu.addAction(QStringLiteral("Menú de aplicaciones"));
        hdr->setEnabled(false);
        QFont f = hdr->font(); f.setBold(true); hdr->setFont(f);
        menu.addSeparator();
        menu.addAction(QStringLiteral("Preferencias del Dock…"), [this](){ abrirPreferencias(); });
        menu.exec(pos);
        return;
    }
    if (item->tipo() == DockItem::Tipo::Escritorios) {
        QMenu menu;
        menu.setStyleSheet(estiloMenu());
        auto* hdr = menu.addAction(QStringLiteral("Escritorios"));
        hdr->setEnabled(false);
        QFont f = hdr->font(); f.setBold(true); hdr->setFont(f);
        menu.addSeparator();
        const int n = x11_get_num_desktops();
        const int actual = x11_get_current_desktop();
        for (int i = 0; i < n; i++) {
            QAction* accion = menu.addAction(QStringLiteral("Ir al escritorio %1").arg(i + 1),
                                             [i](){ x11_set_current_desktop(i); });
            if (i == actual) { QFont fa = accion->font(); fa.setBold(true); accion->setFont(fa); }
        }
        menu.addSeparator();
        menu.addAction(QStringLiteral("Preferencias del Dock…"), [this](){ abrirPreferencias(); });
        menu.exec(pos);
        return;
    }
    menuContextualItem(item, pos);
}

// ─── Context Menus ──────────────────────────────────────────────────────────

static QString estiloMenu() {
    // Perfil ligero: menús opacos y sin esquinas redondeadas.
    if (PerfilVisual::ligero()) {
        return R"(
        QMenu {
            background-color: rgb(28,28,28);
            border: none;
            border-radius: 0;
            padding: 5px;
            color: white;
            font-size: 13px;
        }
        QMenu::item { padding: 7px 22px; }
        QMenu::item:selected { background: rgb(60,60,60); }
        QMenu::item:disabled { color: rgba(255,255,255,100); }
        QMenu::separator { height:1px; background:rgba(255,255,255,25); margin:4px 10px; }
    )";
    }
    return R"(
        QMenu {
            background-color: rgba(28,28,28,235);
            border: 1px solid rgba(255,255,255,50);
            border-radius: 10px;
            padding: 5px;
            color: white;
            font-size: 13px;
        }
        QMenu::item { padding: 7px 22px; border-radius: 5px; }
        QMenu::item:selected { background: rgba(255,255,255,35); }
        QMenu::item:disabled { color: rgba(255,255,255,100); }
        QMenu::separator { height:1px; background:rgba(255,255,255,25); margin:4px 10px; }
    )";
}

void DockWindow::menuContextualItem(DockItem* item, const QPoint& pos) {
    QMenu menu;
    menu.setStyleSheet(estiloMenu());

    auto* hdr = menu.addAction(item->infoApp().nombre);
    hdr->setEnabled(false);
    QFont f = hdr->font(); f.setBold(true); hdr->setFont(f);
    menu.addSeparator();

    if (item->estaAnclada())
        menu.addAction("Quitar del Dock", [this,item](){ desanclarApp(item->infoApp().archivoDesktop); });
    else
        menu.addAction("Mantener en el Dock", [this,item](){ anclarApp(item->infoApp().archivoDesktop); });

    if (!item->estaEjecutando()) {
        menu.addAction("Abrir", [item](){ item->lanzar(); });
    } else {
        menu.addAction("Nueva Ventana", [item](){ item->lanzar(); });

        auto wins = item->ventanas();
        if (wins.size() > 1) {
            menu.addSeparator();
            int n = 1;
            for (quint64 wid : wins) {
                char titulo[512] = {};
                if (!x11_get_window_title((unsigned long)wid, titulo, 512))
                    snprintf(titulo, 512, "Ventana %d", n);
                QString t = QString::fromUtf8(titulo);
                if (t.length() > 40) t = t.left(38) + "…";
                menu.addAction(t, [wid](){ x11_activate_window((unsigned long)wid); });
                n++;
            }
        }

        menu.addSeparator();
        menu.addAction("Cerrar", [item](){
            for (quint64 wid : item->ventanas())
                x11_close_window((unsigned long)wid);
        });
    }

    menu.addSeparator();
    menu.addAction("Preferencias del Dock…", [this](){ abrirPreferencias(); });
    menu.exec(pos);
}

void DockWindow::menuContextualDock(const QPoint& pos) {
    QMenu menu;
    menu.setStyleSheet(estiloMenu());
    menu.addAction("Preferencias del Dock…", [this](){ abrirPreferencias(); });
    QMenu* posMenu = menu.addMenu("Posición");
    posMenu->setStyleSheet(estiloMenu());
    posMenu->addAction("Abajo",     [this](){ DockConfig c=m_cfg; c.posicion=DockPosition::Abajo;     aplicarConfig(c); });
    posMenu->addAction("Arriba",    [this](){ DockConfig c=m_cfg; c.posicion=DockPosition::Arriba;    aplicarConfig(c); });
    posMenu->addAction("Izquierda", [this](){ DockConfig c=m_cfg; c.posicion=DockPosition::Izquierda; aplicarConfig(c); });
    posMenu->addAction("Derecha",   [this](){ DockConfig c=m_cfg; c.posicion=DockPosition::Derecha;   aplicarConfig(c); });
    menu.addSeparator();
    // Modo panel: alargar el dock a todo el borde manteniendo las puntas redondas.
    auto* actPanel = menu.addAction("Modo panel (ancho completo)");
    actPanel->setCheckable(true);
    actPanel->setChecked(m_cfg.modoPanel);
    connect(actPanel, &QAction::toggled, this, [this](bool on){
        DockConfig c = m_cfg;
        c.modoPanel = on;
        aplicarConfig(c);
    });
    menu.exec(pos);
}

void DockWindow::abrirPreferencias() {
    m_timerWM->stop();
    SettingsDialog dlg(m_cfg, m_scanner->todasLasApps(), m_items, this);
    // El perfil visual vive fuera de DockConfig: se le pasa el estado real en
    // curso (pudo activarse con --ligero/SGP_LIGERO sin estar en la config).
    const bool ligeroAntes = PerfilVisual::ligero();
    dlg.setModoLigero(ligeroAntes);
    if (dlg.exec() == QDialog::Accepted) {
        aplicarConfig(dlg.config());
        QStringList nuevasAncladas = dlg.appsAncladas();

        // Quitar desancladas
        for (DockItem* it : m_items) {
            if (it->tipo() != DockItem::Tipo::Aplicacion || !it->estaAnclada()) continue;
            if (!nuevasAncladas.contains(it->infoApp().archivoDesktop)) {
                it->setAnclada(false);
                if (!it->estaEjecutando()) { m_items.removeAll(it); it->deleteLater(); }
            }
        }
        // Agregar nuevas
        for (const QString& df : nuevasAncladas) {
            bool ya = false;
            for (DockItem* it : m_items)
                if (it->infoApp().archivoDesktop == df) { ya=true; it->setAnclada(true); break; }
            if (!ya) anclarApp(df);
        }
        layoutItems(); posicionarDock(); guardarAppsAncladas();

        // Cambio de perfil: persistir la preferencia y relanzar el proceso.
        if (dlg.modoLigero() != ligeroAntes) {
            m_settings->beginGroup("Dock");
            m_settings->setValue("modoLigero", dlg.modoLigero());
            m_settings->endGroup();
            m_settings->sync();
            reiniciarConPerfil(dlg.modoLigero());
            return;
        }
    }
    m_timerWM->start();
}

void DockWindow::reiniciarConPerfil(bool ligero) {
    const QString prog = QCoreApplication::applicationFilePath();
    const QString flag = ligero ? QStringLiteral("--ligero")
                                : QStringLiteral("--optimizado");
    const qint64  pid  = QCoreApplication::applicationPid();

    // Se elimina 'exec' para corregir la sintaxis de la shell con '&' y setsid
    const QString cmd  = QStringLiteral(
        "for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25; do "
        "  kill -0 %1 2>/dev/null || { setsid \"%2\" %3 >/dev/null 2>&1 & exit 0; }; "
        "  sleep 0.2; "
        "done; "
        "setsid \"%2\" %3 >/dev/null 2>&1 &")
        .arg(pid).arg(prog, flag);

    QProcess::startDetached(QStringLiteral("/bin/sh"),
                            { QStringLiteral("-c"), cmd });
    x11_clear_strut((unsigned long)winId());
    
    qApp->quit();
    kill(QCoreApplication::applicationPid(), SIGKILL);
    _exit(0);
}
