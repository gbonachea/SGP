#ifndef X11WINDOWUTILS_H
#define X11WINDOWUTILS_H

#include <QWidget>
#include <xcb/xcb.h>

class X11WindowUtils {
public:
    // Obtiene el ID (WId) de la ventana que tiene el foco activo en el escritorio
    static WId getActiveWindow();

    // Minimiza la ventana enviando el evento WM_CHANGE_STATE (IconicState) al WM
    static void minimizeWindow(WId windowId);

    // Activa, desminimiza y trae la ventana al frente enviando _NET_ACTIVE_WINDOW
    static void activateWindow(WId windowId);

    // Gestiona el comportamiento estándar de un Dock/Barra al hacer clic sobre un icono:
    // - Si la ventana está enfocada -> La minimiza
    // - Si la ventana no está enfocada o está minimizada -> La trae al frente
    static void toggleWindowFocusOrMinimize(WId windowId);
};

#endif // X11WINDOWUTILS_H
