#pragma once
#include <QWidget>
#include <QPropertyAnimation>
#include <QTimer>
#include <QPixmap>
#include "DockConfig.h"
#include "ApplicationScanner.h"

class DockWindow;

class DockItem : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal escala   READ escala   WRITE setEscala)
    Q_PROPERTY(qreal rebote   READ rebote   WRITE setRebote)

public:
    enum class Tipo { Aplicacion, Separador, Lanzador, Reloj, Escritorios };

    explicit DockItem(const InfoApp& app, DockWindow* dock, QWidget* parent = nullptr);
    explicit DockItem(Tipo tipo,          DockWindow* dock, QWidget* parent = nullptr);

    InfoApp     infoApp() const          { return m_app; }
    Tipo        tipo() const             { return m_tipo; }
    bool        estaAnclada() const      { return m_app.estaAnclada; }
    bool        estaEjecutando() const   { return m_app.estaEjecutando; }
    QList<quint64> ventanas() const      { return m_app.ventanas; }
    int         tamanoActual() const;

    void setEjecutando(bool v)           { m_app.estaEjecutando = v; update(); }
    void setIcono(const QIcon& ic)       { m_app.icono = ic; m_pixIco = QPixmap(); m_pixIcoSz = 0; update(); }
    void setNombre(const QString& n)     { m_app.nombre = n; }
    void agregarVentana(quint64 id);
    void quitarVentana(quint64 id);
    void setActiva(bool v)               { m_activa = v; update(); }
    void setAnclada(bool v)              { m_app.estaAnclada = v; }
    void setTamanoMagnificado(int sz)    { m_tamMag = sz; updateGeometry(); update(); }

    // Animation props
    qreal escala() const  { return m_escala; }
    void  setEscala(qreal v) { m_escala = v; update(); }
    qreal rebote() const  { return m_rebote; }
    void  setRebote(qreal v) { m_rebote = v; update(); }

    void lanzar();
    void animarRebote();

    // Public size so DockWindow can query without breaking encapsulation
    QSize tamano() const;

signals:
    void clicIzquierdo();
    void clicDerecho(QPoint pos);
    void clicMedio();
    // Hover prolongado sobre un item con varias ventanas abiertas: el dock
    // muestra el popup de ventanas para elegir una.
    void solicitudPopupVentanas();
    // El puntero salió del item; el dock decide si cerrar el popup.
    void salidaHover();

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void enterEvent(QEnterEvent*) override;
    void leaveEvent(QEvent*) override;

private:
    void dibujarIndicadores(QPainter& p, const QRect& rc);
    void dibujarEtiqueta(QPainter& p);
    void actualizarBateria();
    QPixmap iconoScaled(int sz);   // caché del icono ya escalado (evita reescalar cada frame)

    InfoApp     m_app;
    Tipo        m_tipo;
    DockWindow* m_dock;

    qreal  m_escala  = 1.0;
    qreal  m_rebote  = 0.0;
    bool   m_hover   = false;
    bool   m_presion = false;
    bool   m_activa  = false;
    int    m_tamMag  = 0;
    bool   m_mostrarEtiqueta = false;

    QPropertyAnimation* m_animEscala = nullptr;
    QPropertyAnimation* m_animRebote = nullptr;
    QTimer*             m_timerEtiqueta = nullptr;
    QTimer*             m_timerRebote   = nullptr;
    QTimer*             m_timerReloj    = nullptr;
    QTimer*             m_timerPopupVentanas = nullptr;
    int                 m_contRebote    = 0;

    // Caché del icono escalado (sombra + dibujo usan el mismo pixmap)
    QPixmap             m_pixIco;
    int                 m_pixIcoSz      = 0;
    // Contador para releer la batería de sysfs cada ~12 ticks (5 s × 12 = 60 s)
    int                 m_ticksBateria  = 0;

    // Indicador de batería (contorno del reloj); -1 = sin batería detectada
    int  m_bateriaNivel    = -1;
    bool m_bateriaCargando = false;
};
