#include "TrayHost.h"

#include <QCoreApplication>
#include <QDebug>
#include <QGuiApplication>
#include <QMetaObject>
#include <QTimer>
#include <QtGui/qguiapplication_platform.h> // QNativeInterface::QX11Application

#include <cstring>

#include <xcb/xcb.h>

// ─── Constantes del protocolo XEmbed / System Tray ──────────────────────────

static const uint32_t XEMBED_EMBEDDED_NOTIFY = 0;
static const uint32_t XEMBED_EMBEDDED_VERSION = 0;
static const uint32_t SYSTEM_TRAY_REQUEST_DOCK = 0;

TrayHost::TrayHost(int iconSize, bool vertical, QWidget *parent)
    : QWidget(parent)
    , m_vertical(vertical)
    , m_iconSize(qBound(16, iconSize, 48))
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);
    hide();

    m_retry = new QTimer(this);
    m_retry->setInterval(2500);
    connect(m_retry, &QTimer::timeout, this, &TrayHost::retryAcquire);
}

TrayHost::~TrayHost()
{
    shutdown();
}

void TrayHost::setIconSize(int s)
{
    const int v = qBound(16, s, 48);
    if (v == m_iconSize)
        return;
    m_iconSize = v;
    markDirty();
}

void TrayHost::setVertical(bool v)
{
    if (v == m_vertical)
        return;
    m_vertical = v;
    markDirty();
}

void TrayHost::setup()
{
    if (m_started)
        return;
    m_started = true;

    // Obliga a crear la ventana nativa de este widget: será el contenedor
    // donde se re-parentan los iconos y el dueño de la selección.
    m_trayWin = static_cast<uint32_t>(winId());

    auto *x11 = qGuiApp->nativeInterface<QNativeInterface::QX11Application>();
    if (!x11 || !x11->connection()) {
        qWarning("TrayHost: sin conexión QX11Application; bandeja desactivada");
        return;
    }
    m_c = x11->connection();

    const xcb_setup_t *setup = xcb_get_setup(m_c);
    const xcb_screen_iterator_t it = xcb_setup_roots_iterator(setup);
    m_root = it.data->root;

    internAtoms();

    // Suscribirse a los ClientMessage del protocolo que las apps envían al
    // root (con event_mask SubstructureNotifyMask). IMPORTANTE: no sobrescribir
    // la máscara del WM: se consulta y se añade nuestro bit.
    {
        xcb_get_window_attributes_cookie_t ck = xcb_get_window_attributes(m_c, m_root);
        xcb_get_window_attributes_reply_t *r  =
            xcb_get_window_attributes_reply(m_c, ck, nullptr);
        uint32_t ev = r ? r->your_event_mask : 0;
        if (r) free(r);
        ev |= XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY;
        xcb_change_window_attributes(m_c, m_root, XCB_CW_EVENT_MASK, &ev);
    }

    // En nuestra ventana: notificaciones de los hijos embebidos.
    {
        const uint32_t ev = XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY;
        xcb_change_window_attributes(m_c, m_trayWin, XCB_CW_EVENT_MASK, &ev);
    }

    qApp->installNativeEventFilter(this);
    acquire();
    xcb_flush(m_c);
}

void TrayHost::shutdown()
{
    if (m_c && m_trayWin) {
        release();
        for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it)
            xcb_destroy_window(m_c, it.key());
        m_entries.clear();
        xcb_flush(m_c);
        m_c = nullptr;
    }
    if (m_started)
        qApp->removeNativeEventFilter(this);
    m_started = false;
}

void TrayHost::internAtoms()
{
    auto intern = [this](const char *name) -> uint32_t {
        xcb_intern_atom_cookie_t ck = xcb_intern_atom(m_c, 0, strlen(name), name);
        xcb_intern_atom_reply_t *r  = xcb_intern_atom_reply(m_c, ck, nullptr);
        if (!r)
            return XCB_ATOM_NONE;
        const uint32_t a = r->atom;
        free(r);
        return a;
    };
    m_trayAtom    = intern("_NET_SYSTEM_TRAY_S0");
    m_opcodeAtom  = intern("_NET_SYSTEM_TRAY_OPCODE");
    m_managerAtom = intern("MANAGER");
    m_xembedAtom  = intern("XEMBED");
}

void TrayHost::acquire()
{
    if (!m_c)
        return;

    // No robar la bandeja del sistema si ya la posee otro gestor (p. ej. el
    // panel del escritorio). Solo se adquiere si está libre o somos nosotros.
    xcb_get_selection_owner_cookie_t ck0 = xcb_get_selection_owner(m_c, m_trayAtom);
    xcb_get_selection_owner_reply_t *r0  = xcb_get_selection_owner_reply(m_c, ck0, nullptr);
    const uint32_t actual = r0 ? r0->owner : XCB_WINDOW_NONE;
    if (r0) free(r0);

    if (actual != XCB_WINDOW_NONE && actual != m_trayWin) {
        // Hay otra bandeja activa (el panel del sistema): quedarse pasivo,
        // sin robar la selección. Se reintenta solo por si se libera.
        m_owned = false;
        m_retry->start();
        return;
    }

    xcb_set_selection_owner(m_c, m_trayWin, m_trayAtom, XCB_CURRENT_TIME);

    xcb_get_selection_owner_cookie_t ck = xcb_get_selection_owner(m_c, m_trayAtom);
    xcb_get_selection_owner_reply_t *r  = xcb_get_selection_owner_reply(m_c, ck, nullptr);
    const bool ok = r && r->owner == m_trayWin;
    if (r) free(r);

    if (!ok) {
        // No se pudo adquirir: se reintenta por si se libera.
        m_owned = false;
        m_retry->start();
        return;
    }
    m_owned = true;
    m_retry->stop();

    // Anunciar la bandeja al root (MANAGER) conforme a la especificación.
    xcb_client_message_event_t msg{};
    msg.response_type = XCB_CLIENT_MESSAGE;
    msg.format = 32;
    msg.window = m_root;
    msg.type = m_managerAtom;
    msg.data.data32[0] = XCB_CURRENT_TIME;
    msg.data.data32[1] = m_trayAtom;
    msg.data.data32[2] = m_trayWin;
    msg.data.data32[3] = 0;
    msg.data.data32[4] = 0;
    xcb_send_event(m_c, 0, m_root, XCB_EVENT_MASK_STRUCTURE_NOTIFY,
                   reinterpret_cast<const char *>(&msg));
    xcb_flush(m_c);

    qInfo("TrayHost: _NET_SYSTEM_TRAY_S0 adquirida");
}

void TrayHost::release()
{
    if (!m_c || !m_trayWin)
        return;
    xcb_set_selection_owner(m_c, XCB_WINDOW_NONE, m_trayAtom, XCB_CURRENT_TIME);
    m_owned = false;
}

void TrayHost::retryAcquire()
{
    if (!m_owned && m_c)
        acquire();
}

// ─── Filtro de eventos nativos (xcb) ────────────────────────────────────────

bool TrayHost::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *)
{
    if (eventType != QByteArrayLiteral("xcb_generic_event_t") || !m_c)
        return false;

    auto *ev = static_cast<xcb_generic_event_t *>(message);
    const uint8_t type = ev->response_type & ~0x80;

    switch (type) {
    case XCB_CLIENT_MESSAGE: {
        auto *cm = reinterpret_cast<xcb_client_message_event_t *>(ev);
        if (cm->type == m_opcodeAtom && m_owned) {
            const uint32_t op  = cm->data.data32[1];
            const uint32_t win = cm->data.data32[2];
            if (op == SYSTEM_TRAY_REQUEST_DOCK)
                dockWindow(win);
            // Otros opcodes (_NET_SYSTEM_TRAY_BEGIN_MESSAGE …) se ignoran.
        }
        // Mensajes XEMBED del icono hacia nosotros: un tray pasivo no necesita
        // responder salvo para XEMBED_EMBEDDED_NOTIFY (solo lo enviamos nosotros).
        return false;
    }
    case XCB_MAP_NOTIFY: {
        auto *mn = reinterpret_cast<xcb_map_notify_event_t *>(ev);
        auto it = m_entries.find(mn->window);
        if (it != m_entries.end()) {
            it->mapped = true;
            markDirty();
        }
        return false;
    }
    case XCB_UNMAP_NOTIFY: {
        auto *un = reinterpret_cast<xcb_unmap_notify_event_t *>(ev);
        if (m_entries.contains(un->window))
            markDirty();
        return false;
    }
    case XCB_DESTROY_NOTIFY: {
        auto *dn = reinterpret_cast<xcb_destroy_notify_event_t *>(ev);
        if (m_entries.contains(dn->window)) {
            m_entries.remove(dn->window);
            markDirty();
        }
        return false;
    }
    case XCB_CONFIGURE_NOTIFY: {
        auto *cn = reinterpret_cast<xcb_configure_notify_event_t *>(ev);
        // El icono cambió de tamaño (lo renegocia el servidor): re-colocar.
        if (m_entries.contains(cn->window) || cn->event == m_trayWin)
            markDirty();
        return false;
    }
    case XCB_SELECTION_CLEAR: {
        auto *sc = reinterpret_cast<xcb_selection_clear_event_t *>(ev);
        if (sc->selection == m_trayAtom && m_owned) {
            m_owned = false;
            m_retry->start();
            emit geometryChanged();
        }
        return false;
    }
    default:
        return false;
    }
}

// ─── Dibujo / layout de los iconos ──────────────────────────────────────────

void TrayHost::dockWindow(uint32_t w)
{
    if (!m_c || m_entries.contains(w))
        return;

    // Reparentar el icono como hijo de la ventana de la bandeja.
    xcb_reparent_window(m_c, w, m_trayWin, 0, 0);

    // Tamaño de la bandeja.
    const uint32_t vals[2] = { uint32_t(m_iconSize), uint32_t(m_iconSize) };
    xcb_configure_window(m_c, w, XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, vals);

    // Escuchar eventos del icono embebido.
    const uint32_t ev = XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_PROPERTY_CHANGE;
    xcb_change_window_attributes(m_c, w, XCB_CW_EVENT_MASK, &ev);

    sendEmbedNotify(w);

    Entry e;
    m_entries.insert(w, e);
    markDirty();
    xcb_flush(m_c);
}

void TrayHost::sendEmbedNotify(uint32_t icon)
{
    xcb_client_message_event_t msg{};
    msg.response_type = XCB_CLIENT_MESSAGE;
    msg.format = 32;
    msg.window = icon;
    msg.type = m_xembedAtom;
    msg.data.data32[0] = XCB_CURRENT_TIME;        // time
    msg.data.data32[1] = XEMBED_EMBEDDED_NOTIFY;  // message type
    msg.data.data32[2] = XEMBED_EMBEDDED_VERSION; // detail: versión soportada
    msg.data.data32[3] = m_trayWin;               // embedder
    msg.data.data32[4] = 0;
    xcb_send_event(m_c, 0, icon, XCB_EVENT_MASK_NO_EVENT,
                   reinterpret_cast<const char *>(&msg));
}

void TrayHost::removeWindow(uint32_t w)
{
    m_entries.remove(w);
    markDirty();
}

void TrayHost::markDirty()
{
    QMetaObject::invokeMethod(this, "relayout", Qt::QueuedConnection);
}

QSize TrayHost::sizeHint() const
{
    const int pad = 6, gap = 6;
    const int n = m_entries.size();
    if (n == 0)
        return { 0, 0 };
    if (m_vertical)
        return { m_iconSize + pad * 2, n * (m_iconSize + gap) - gap + pad * 2 };
    return { n * (m_iconSize + gap) - gap + pad * 2, m_iconSize + pad * 2 };
}

void TrayHost::relayout()
{
    if (!m_c)
        return;

    const int pad = 6, gap = 6;
    const int n = m_entries.size();

    if (n == 0) {
        hide();
        resize(0, 0);
        emit geometryChanged();
        return;
    }

    show();
    resize(sizeHint());

    int i = 0;
    for (auto it = m_entries.begin(); it != m_entries.end(); ++it, ++i) {
        const int x = m_vertical ? pad : pad + i * (m_iconSize + gap);
        const int y = m_vertical ? pad + i * (m_iconSize + gap) : pad;
        const uint32_t vals[4] = { uint32_t(x), uint32_t(y),
                                   uint32_t(m_iconSize), uint32_t(m_iconSize) };
        xcb_configure_window(m_c, it.key(),
                             XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
                             XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
                             vals);
    }
    xcb_flush(m_c);

    emit geometryChanged();
}