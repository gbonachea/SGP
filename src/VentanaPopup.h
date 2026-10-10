#pragma once
#include <QWidget>
#include <QList>
#include <QIcon>
#include "DockConfig.h"

class QVBoxLayout;
class QScrollArea;
class QWidget;

/**
 * VentanaPopup — popup de ventanas de una aplicación agrupada.
 *
 * Se muestra al pasar el puntero (o hacer clic) sobre un item del dock que
 * tenga varias ventanas abiertas. Enumerá las ventanas con su título para que
 * el usuario elija cuál activar (maximizar/levantar) o minimizar; el clic sobre
 * una ventana aplica el comportamiento estándar del dock (si está activa la
 * minimiza; si no, la activa y desminimiza).
 */
class VentanaPopup : public QWidget {
    Q_OBJECT
public:
    explicit VentanaPopup(QWidget* parent = nullptr);

    /// Abre el popup anclado junto a `ancla` (rect global del item del dock).
    /// `pos` indica el lado del dock; `ventanas` son los ids X11 de las
    /// ventanas abiertas de la aplicación.
    void abrirSobre(const QRect& ancla, DockPosition pos,
                    const QList<quint64>& ventanas,
                    const QIcon& iconoApp, const QString& nombreApp);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    void limpiar();

    QScrollArea* m_area        = nullptr;
    QWidget*     m_contenedor  = nullptr;
    QVBoxLayout* m_layout      = nullptr;
};