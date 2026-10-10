#pragma once
#include <QWidget>
#include <QList>
#include <QMap>
#include "DockConfig.h"
#include "ApplicationScanner.h"

class QVBoxLayout;
class QHBoxLayout;
class QScrollArea;
class QListWidget;
class QLineEdit;

/**
 * AppMenu — menú/lanzador tipo popup con las aplicaciones instaladas.
 *
 * Barra de búsqueda en la parte superior: al escribir filtra las apps en el
 * panel derecho (estilo Cinnamon). Columna izquierda: categorías (Accesorios,
 * Internet, …); al hacer clic se muestran sus aplicativos. Al final de la
 * lista hay una categoría especial "Apagar y salir" con las acciones de
 * sesión y energía: bloquear, cerrar sesión, suspender, reiniciar y apagar.
 */
class AppMenu : public QWidget {
    Q_OBJECT
public:
    explicit AppMenu(const QList<InfoApp>& apps, QWidget* parent = nullptr);

    /// Abre el menú anclándolo junto al rectángulo del botón (coords globales).
    void abrirSobre(const QRect& ancla, DockPosition pos);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QString etiquetaCategoria(const QString& rawCats) const;
    void construirUI();
    void mostrarCategoria(const QString& categoria);
    void buscarCambio(const QString& texto);
    void poblarDerecha();
    void anadirBotonApp(const InfoApp& app);
    void lanzarApp(const InfoApp& app);
    void lanzarComando(const QString& comando);
    void alPulsarEnter();
    void alPulsarEscape();

    QMap<QString, QList<InfoApp>> m_grupos;
    QList<QString>                m_ordenCategorias;
    QString                       m_categoriaActual;

    QLineEdit*   m_busqueda      = nullptr;
    QListWidget* m_listaCat      = nullptr;
    QScrollArea* m_areaApps      = nullptr;
    QWidget*     m_contenedorApps = nullptr;
    QVBoxLayout* m_layoutApps    = nullptr;
};