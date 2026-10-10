#pragma once
#include <QWidget>
#include <QList>
#include <QMap>
#include <QSet>
#include <QSettings>
#include <QTimer>
#include <QSocketNotifier>
#include <QPropertyAnimation>
#include "DockConfig.h"
#include "ApplicationScanner.h"

class DockItem;
class SystemPanel;
class TrayHost;
class AppMenu;
class VentanaPopup;
class DesktopPopup;

class DockWindow : public QWidget {
    Q_OBJECT
    Q_PROPERTY(int posOffset READ posOffset WRITE setPosOffset)

public:
    explicit DockWindow(QWidget* parent = nullptr);
    ~DockWindow();

    const DockConfig& config() const { return m_cfg; }
    void aplicarConfig(const DockConfig& cfg);

    // Items
    void anclarApp(const QString& desktopFile);
    void desanclarApp(const QString& desktopFile);
    void reconstruirDock();
    void insertarItem(DockItem* item);

    // Auto-hide property
    int  posOffset() const { return m_posOffset; }
    void setPosOffset(int v);

public slots:
    void abrirPreferencias();

protected:
    void paintEvent(QPaintEvent*) override;
    void enterEvent(QEnterEvent*) override;
    void leaveEvent(QEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void showEvent(QShowEvent*) override;

private:
    // Setup
    void cargarConfig();
    void guardarConfig();
    void cargarAppsAncladas();
    void guardarAppsAncladas();
    void configurarWM();
    void establecerEstrut();
    void limpiarEstrut();

    // Layout
    void layoutItems();
    void posicionarDock();

    // Window tracking
    void actualizarVentanas();
    void alEventoWM();

    // Magnification
    void actualizarMagnificacion(const QPoint& mouse);
    void resetMagnificacion();

    // Auto-hide
    void mostrarDock();
    void ocultarDock();

    // Perfil de rendimiento: cierra el dock y lo relanza con --ligero o
    // --optimizado. El cambio de perfil no se puede aplicar en caliente.
    void reiniciarConPerfil(bool ligero);

    // Menus
    void menuContextualItem(DockItem* item, const QPoint& pos);
    void menuContextualDock(const QPoint& pos);

    // Popup de ventanas de una aplicación agrupada
    void abrirPopupVentanas(DockItem* item);
    void cerrarPopupVentanas();

    // Item signals
    void alHacerClicItem(DockItem* item);
    void alHacerClicDerechoItem(DockItem* item, const QPoint& pos);
    void conectarItem(DockItem* item);

    // Panel de control del sistema (clic sobre el reloj): red, volumen, brillo
    void abrirPanelSistema(DockItem* item);

    // Menú de aplicaciones por categoría (clic sobre el lanzador de la punta)
    void abrirMenuApps(DockItem* item);

    // Popup de escritorios virtuales (clic sobre el botón de escritorios)
    void abrirPopupEscritorios(DockItem* item);

    // Screen geometry helper
    QRect geometriaDock() const;
    QScreen* pantallaObjetivo() const;
    // Posicionado por eventos: reescucha la geometría de las pantallas
    // (hotplug/rotación/resolución) sin polling periódico de 2 s.
    void conectarPantalla(QScreen* s);
    void conectarPantallas();

    DockConfig          m_cfg;
    QList<DockItem*>    m_items;
    ApplicationScanner* m_scanner;
    QSettings*          m_settings;

    // WM tracking
    QMap<quint64, DockItem*> m_ventanaAItem;
    QTimer*             m_timerWM;
    // Vigilador por eventos X11 (escritorio/ventanas): reemplaza el polling.
    void*               m_watchDpy = nullptr;
    int                 m_watchFd  = -1;
    QSocketNotifier*    m_notifWM  = nullptr;

    // Auto-hide
    QTimer*             m_timerAutoOcultar;
    QPropertyAnimation* m_animSlide;
    bool                m_oculto      = false;
    bool                m_mouseAdentro = false;
    int                 m_posOffset   = 0;

    // Pantallas ya conectadas a re-posicionado por eventos (sin polling)
    QSet<const QScreen*> m_pantallasConectadas;

    // Magnification
    QTimer*             m_timerResetMag;

    // Perfil ligero (--ligero): limita la magnificación a ~30 fps guardando la
    // última posición del puntero y aplicándola como mucho cada 33 ms.
    QTimer*             m_timerMag   = nullptr;
    QPoint              m_ultimoMouse;

    // Panel de control del sistema (clic sobre el reloj)
    SystemPanel*        m_panelSistema = nullptr;

    // Menú de aplicaciones por categoría (clic sobre el lanzador)
    AppMenu*            m_menuApps = nullptr;

    // Popup de ventanas de una aplicación agrupada (varias ventanas abiertas)
    VentanaPopup*       m_popupVentanas = nullptr;
    DockItem*           m_itemPopup     = nullptr;   // item al que pertenece el popup
    QTimer*             m_timerCerrarPopup = nullptr;

    // Popup de escritorios virtuales (puntos; clic sobre el botón de escritorios)
    DesktopPopup*       m_popupEscritorios = nullptr;

    // Bandeja del sistema X11 (XEmbed): iconos de terceros embebidos
    TrayHost*           m_trayHost = nullptr;
};
