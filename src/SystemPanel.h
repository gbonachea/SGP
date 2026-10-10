#pragma once
#include <QWidget>
#include <QString>
#include <QSet>
#include "DockConfig.h"

class QVBoxLayout;
class QLabel;
class QSlider;
class QPushButton;
class QWidget;
class QScrollArea;

/**
 * Panel de control del sistema que se abre con clic sobre el reloj del dock:
 * redes WiFi, volumen y brillo. Se cierra solo al hacer clic fuera (Qt::Popup).
 *
 * Dependencias externas opcionales:
 *   - Red:      nmcli (NetworkManager)
 *   - Volumen:  pactl (PulseAudio/PipeWire) o amixer (ALSA)
 *   - Brillo:   brightnessctl o /sys/class/backlight
 */
class SystemPanel : public QWidget {
    Q_OBJECT
public:
    explicit SystemPanel(QWidget* parent = nullptr);

    // Abre el panel anclándolo junto al rectángulo del reloj (coordenadas globales).
    void abrirSobre(const QRect& ancla, DockPosition pos);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    void construirUI();
    QLabel* seccionHeader(const QString& texto);

    QString ejecutar(const QString& cmd, int timeoutMs = 2500) const;

    // Red
    void actualizarRed();
    void rescanearRed();
    void poblarRed(const QString& out);
    // Refresca en segundo plano la lista de SSIDs que ya tienen un perfil
    // guardado en NetworkManager (contraseña almacenada) y devuelve la última
    // lista conocida. Se usa para NO volver a pedir la contraseña al conectar
    // a una red con la que ya nos hemos conectado antes.
    void refrescarPerfilesWifi();
    QSet<QString> consultarPerfilesWifiSync() const;
    QSet<QString> perfilesWifiGuardados() const { return m_perfilesWifi; }
    void conectarWifi(const QString& ssid, bool secure, bool activa = false);
    void lanzarConexionWifi(const QString& ssid, const QString& password, bool secure);
    // Diálogo oscuro de contraseña (a juego con el dock). Devuelve la clave o
    // una cadena vacía si se cancela.
    static QString pedirClave(QWidget* parent,
                              const QString& titulo, const QString& mensaje);
    void abrirConfigRed();

    // Volumen
    void actualizarVolumen();
    void setVolumen(int v);
    void cambiarVolumen(int delta);
    void alternarMute();

    // Brillo
    void actualizarBrillo();
    void setBrillo(int v);
    void cambiarBrillo(int delta);
    QString rutaBacklight() const;

    // Estado
    void setEstado(const QString& txt);

    QVBoxLayout* m_contenedor   = nullptr;
    QWidget*     m_zonaRed      = nullptr;
    QVBoxLayout* m_layoutRed    = nullptr;
    QScrollArea* m_scrollRed    = nullptr;
    QLabel*      m_estado       = nullptr;

    // SSIDs con perfil guardado en NetworkManager (se refresca al escanear).
    QSet<QString> m_perfilesWifi;
    bool m_perfilesWifiListos = false;

    QLabel*      m_labelVol     = nullptr;
    QSlider*     m_sliderVol    = nullptr;
    QPushButton* m_btnMute      = nullptr;

    QLabel*      m_labelBrillo  = nullptr;
    QSlider*     m_sliderBrillo = nullptr;

    bool         m_volPactl    = false;
    bool         m_brilloSysfs = false;
    bool         m_brilloCtl   = false;
    QString      m_backlightDir;
};