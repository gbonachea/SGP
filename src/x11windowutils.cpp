#include "x11windowutils.h"

#include <QtGlobal>
#include <QGuiApplication>

// Obtención de la conexión XCB:
//   - Qt5 expone QX11Info::connection()/appRootWindow().
//   - Qt6 elimina QX11Info y usa la interfaz nativa QNativeInterface::QX11Application
//     (disponible desde Qt 6.2).
// El adaptador mantiene la lógica de los métodos públicos idéntica en ambas versiones.
#if QT_VERSION >= QT_VERSION_CHECK(6, 2, 0)
#  include <QtGui/qguiapplication_platform.h> // QNativeInterface::QX11Application
#else
#  include <QX11Info>
#endif

#include <cstring>
#include <cstdlib>

// ─── Conexión XCB / root window ───────────────────────────────────────────────

static xcb_connection_t* x11Connection() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 2, 0)
    if (auto* x11 = qGuiApp->nativeInterface<QNativeInterface::QX11Application>())
        return x11->connection();
    return nullptr;
#else
    return QX11Info::connection();
#endif
}

static xcb_window_t x11RootWindow(xcb_connection_t* conn) {
    if (!conn) return XCB_WINDOW_NONE;
#if QT_VERSION >= QT_VERSION_CHECK(6, 2, 0)
    const xcb_screen_iterator_t it = xcb_setup_roots_iterator(xcb_get_setup(conn));
    return it.data ? it.data->root : XCB_WINDOW_NONE;
#else
    return QX11Info::appRootWindow();
#endif
}

static xcb_atom_t getAtom(xcb_connection_t* conn, const char* name) {
    xcb_intern_atom_cookie_t cookie = xcb_intern_atom(conn, 0, strlen(name), name);
    xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(conn, cookie, nullptr);
    if (!reply) return XCB_NONE;
    xcb_atom_t atom = reply->atom;
    free(reply);
    return atom;
}

// ─── API pública ──────────────────────────────────────────────────────────────

WId X11WindowUtils::getActiveWindow() {
    xcb_connection_t* conn = x11Connection();
    if (!conn) return 0;

    xcb_window_t root = x11RootWindow(conn);
    xcb_atom_t activeAtom = getAtom(conn, "_NET_ACTIVE_WINDOW");

    if (activeAtom == XCB_NONE) return 0;
    xcb_get_property_cookie_t cookie = xcb_get_property(
        conn, 0, root, activeAtom, XCB_ATOM_WINDOW, 0, 1
    );
    xcb_get_property_reply_t* reply = xcb_get_property_reply(conn, cookie, nullptr);

    WId activeWin = 0;
    if (reply && xcb_get_property_value_length(reply) > 0) {
        activeWin = *(xcb_window_t*)xcb_get_property_value(reply);
    }

    free(reply);
    return activeWin;
}

void X11WindowUtils::minimizeWindow(WId windowId) {
    if (windowId == 0) return;
    xcb_connection_t* conn = x11Connection();
    if (!conn) return;

    xcb_window_t root = x11RootWindow(conn);
    xcb_atom_t wmChangeState = getAtom(conn, "WM_CHANGE_STATE");

    if (wmChangeState == XCB_NONE) return;
    xcb_client_message_event_t event;
    memset(&event, 0, sizeof(event));
    event.response_type = XCB_CLIENT_MESSAGE;
    event.format = 32;
    event.window = windowId;
    event.type = wmChangeState;
    event.data.data32[0] = 3; // 3 = IconicState (Minimizado en ICCCM)
    xcb_send_event(
        conn, 0, root,
        XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY,
        (const char*)&event
    );
    xcb_flush(conn);
}

void X11WindowUtils::activateWindow(WId windowId) {
    if (windowId == 0) return;
    xcb_connection_t* conn = x11Connection();
    if (!conn) return;

    xcb_window_t root = x11RootWindow(conn);
    xcb_atom_t activeAtom = getAtom(conn, "_NET_ACTIVE_WINDOW");

    if (activeAtom == XCB_NONE) return;
    xcb_client_message_event_t event;
    memset(&event, 0, sizeof(event));
    event.response_type = XCB_CLIENT_MESSAGE;
    event.format = 32;
    event.window = windowId;
    event.type = activeAtom;
    event.data.data32[0] = 1; // Origen: Solicitado por aplicación del usuario
    event.data.data32[1] = XCB_CURRENT_TIME;
    xcb_send_event(
        conn, 0, root,
        XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY,
        (const char*)&event
    );
    xcb_flush(conn);
}

void X11WindowUtils::toggleWindowFocusOrMinimize(WId windowId) {
    WId activeWin = getActiveWindow();
    if (activeWin == windowId) {
        // Si la ventana seleccionada YA TIENE el foco actual -> Minimizar
        minimizeWindow(windowId);
    } else {
        // Si está minimizada o en segundo plano -> Traer al frente
        activateWindow(windowId);
    }
}
