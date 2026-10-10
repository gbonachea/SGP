#pragma once
#include <QWidget>
#include <QList>
#include "DockConfig.h"

class QHBoxLayout;
class QPushButton;
class QTimer;

/**
 * DesktopPopup — popup de escritorios virtuales.
 *
 * Estilo Cinnamon: se muestra al hacer clic en el botón de escritorios del
 * dock. Enseña un punto por escritorio, colocados horizontalmente; el punto del
 * escritorio activo se pinta azul y los demás quedan grises. Al hacer clic sobre
 * un punto se cambia a ese escritorio (el azul se mueve de inmediato). El popup
 * permanece abierto mientras el puntero no salga ni se haga clic fuera, y sigue
 * el estado real del WM (también cambia si se usa atajo de teclado).
 */
class DesktopPopup : public QWidget {
    Q_OBJECT
public:
    explicit DesktopPopup(QWidget* parent = nullptr);

    /// Abre el popup anclado a `ancla` (rect global del botón del dock).
    void abrirSobre(const QRect& ancla, DockPosition pos);

protected:
    void paintEvent(QPaintEvent*) override;
    void hideEvent(QHideEvent*) override;

private:
    void reconstruir();
    void refrescarActual();

    QList<QPushButton*> m_botones;
    QHBoxLayout*        m_layout = nullptr;
    QTimer*             m_timer  = nullptr;
};