#pragma once
#include <QWidget>
#include <QAbstractNativeEventFilter>
#include <QHash>

#include <xcb/xcb.h>

/**
 * TrayHost — bandeja del sistema X11 real (XEmbed) integrada en el dock.
 *
 * Implementa el protocolo freedesktop:
 *   - se hace dueño de la selección _NET_SYSTEM_TRAY_S0
 *   - anuncia `MANAGER` al root para que las apps sepan dónde embeker
 *   - responde a las peticiones SYSTEM_TRAY_REQUEST_DOCK (_NET_SYSTEM_TRAY_OPCODE)
 *   - embebe los iconos (XEmbed) como ventanas hijas de este widget y los
 *     redimensiona al tamaño indicado (m_iconSize).
 *
 * No interfiere con el event loop de Qt: es un QAbstractNativeEventFilter
 * que solo atiende los eventos xcb del protocolo (ClientMessage, Map/Unmap/
 * Destroy/Configure y SelectionClear).
 *
 * Uso: crear el widget, colocar en el layout del dock, y llamar a setup()
 * cuando la ventana nativa exista (p.ej. en el showEvent del dock).
 */
class TrayHost : public QWidget, public QAbstractNativeEventFilter {
    Q_OBJECT
public:
    explicit TrayHost(int iconSize = 24, bool vertical = false, QWidget *parent = nullptr);
    ~TrayHost() override;

    /// Debe llamarse una vez la ventana nativa exista (dock visible).
    void setup();

    void setIconSize(int s);
    void setVertical(bool v);
    void shutdown();

    int  iconCount() const { return m_entries.size(); }
    bool isActive() const { return m_owned; }

    QSize sizeHint() const override;

    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;

signals:
    /// El número de iconos o su tamaño cambió: el dock debe re-colocar.
    void geometryChanged();

private slots:
    void relayout();
    void retryAcquire();

private:
    void acquire();
    void release();
    void internAtoms();
    void dockWindow(xcb_window_t w);
    void removeWindow(xcb_window_t w);
    void markDirty();
    void sendEmbedNotify(xcb_window_t icon);

    struct Entry {
        bool mapped = false;
    };
    QHash<uint32_t, Entry> m_entries;

    xcb_connection_t *m_c        = nullptr;
    uint32_t          m_trayWin  = 0;
    uint32_t          m_root     = 0;
    uint32_t          m_trayAtom = 0;   // _NET_SYSTEM_TRAY_S0
    uint32_t          m_opcodeAtom = 0; // _NET_SYSTEM_TRAY_OPCODE
    uint32_t          m_managerAtom = 0;// MANAGER
    uint32_t          m_xembedAtom = 0; // XEMBED

    bool    m_owned    = false;
    bool    m_started  = false;
    bool    m_vertical = false;
    int     m_iconSize;

    class QTimer *m_retry = nullptr;
};