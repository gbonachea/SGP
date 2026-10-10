#include "SettingsDialog.h"
#include "DockItem.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QTabWidget>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QApplication>
#include <QScreen>
#include <QScrollArea>
#include <QFrame>
#include <QLabel>
#include <QDebug>

static QString estiloDialogo() {
    return R"(
        QDialog, QWidget {
            background-color: #222222;
            color: #ffffff;
            font-family: "Noto Sans", "DejaVu Sans", "Sans";
            font-size: 13px;
        }
        QTabWidget::pane {
            border: 1px solid rgba(255,255,255,25);
            border-radius: 10px;
            background: rgba(255,255,255,5);
            margin-top: -1px;
        }
        QTabBar::tab {
            background: rgba(255,255,255,8);
            color: #aaaaaa;
            padding: 9px 20px;
            border-top-left-radius: 8px;
            border-top-right-radius: 8px;
            margin-right: 3px;
        }
        QTabBar::tab:selected { background: rgba(255,255,255,18); color: white; }
        QTabBar::tab:hover    { background: rgba(255,255,255,13); color: #ddd; }
        QGroupBox {
            color: #888;
            border: 1px solid rgba(255,255,255,15);
            border-radius: 8px;
            margin-top: 12px;
            padding-top: 10px;
            font-size: 11px;
        }
        QGroupBox::title { subcontrol-origin: margin; left: 12px; }
        QLabel { color: #cccccc; }
        QSlider::groove:horizontal {
            height: 4px; background: rgba(255,255,255,18); border-radius: 2px;
        }
        QSlider::handle:horizontal {
            background: white; width: 16px; height: 16px;
            border-radius: 8px; margin: -6px 0;
        }
        QSlider::sub-page:horizontal { background: #3d8eff; border-radius: 2px; }
        QCheckBox { color: #cccccc; spacing: 9px; }
        QCheckBox::indicator {
            width: 20px; height: 20px; border-radius: 5px;
            border: 1.5px solid rgba(255,255,255,35);
            background: rgba(255,255,255,8);
        }
        QCheckBox::indicator:checked { background: #3d8eff; border-color: #3d8eff; }
        QComboBox {
            background: rgba(255,255,255,12);
            border: 1px solid rgba(255,255,255,25);
            border-radius: 7px; padding: 6px 12px;
            color: white; min-width: 150px;
        }
        QComboBox::drop-down { border: none; width: 20px; }
        QComboBox QAbstractItemView {
            background: #2d2d2d; color: white; border-radius: 6px;
            selection-background-color: #3d8eff;
        }
        QSpinBox {
            background: rgba(255,255,255,12);
            border: 1px solid rgba(255,255,255,25);
            border-radius: 7px; padding: 5px 10px; color: white;
        }
        QListWidget {
            background: rgba(255,255,255,6);
            border: 1px solid rgba(255,255,255,18);
            border-radius: 8px; color: white;
            outline: none;
        }
        QListWidget::item { padding: 5px 8px; border-radius: 5px; }
        QListWidget::item:selected { background: rgba(61,142,255,130); }
        QListWidget::item:hover    { background: rgba(255,255,255,12); }
        QPushButton {
            background: rgba(255,255,255,12);
            border: 1px solid rgba(255,255,255,25);
            border-radius: 7px; padding: 7px 18px; color: white;
            min-width: 80px;
        }
        QPushButton:hover   { background: rgba(255,255,255,20); }
        QPushButton:pressed { background: rgba(255,255,255,8); }
        QPushButton#btnAceptar { background: #3d8eff; border-color: #3d8eff; }
        QPushButton#btnAceptar:hover { background: #5a9fff; }
        QPushButton#btnAplicar { background: rgba(61,142,255,60); border-color: #3d8eff; }
    )";
}

// Devuelve una página que hace scroll: cuando el diálogo es más pequeño que el
// formulario, el contenido se desplaza en vez de comprimirse (que recortaba los
// textos a la mitad).
static QWidget* paginaConScroll(QWidget* contenido) {
    auto* area = new QScrollArea;
    area->setWidgetResizable(true);
    area->setFrameShape(QFrame::NoFrame);
    area->setWidget(contenido);
    area->setStyleSheet(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollArea > QWidget > QWidget { background: transparent; }"
        "QScrollBar:vertical { background: rgba(255,255,255,10);"
        "   width: 10px; margin: 2px 0; border-radius: 5px; }"
        "QScrollBar::handle:vertical { background: rgba(255,255,255,55);"
        "   min-height: 30px; border-radius: 4px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar:horizontal { background: transparent; height: 10px; }");
    return area;
}

SettingsDialog::SettingsDialog(const DockConfig& cfg,
                               const QList<InfoApp>& todasApps,
                               const QList<DockItem*>& itemsActuales,
                               QWidget* parent)
    : QDialog(parent)
    , m_cfg(cfg)
    , m_todasApps(todasApps)
    , m_itemsActuales(itemsActuales)
{
    setWindowTitle("Preferencias del Dock");
    // Las pestañas hacen scroll, así que el mínimo puede ser pequeño; se abre a
    // un alto cómodo si la pantalla lo permite.
    setMinimumSize(520, 460);
    setStyleSheet(estiloDialogo());
    construirUI();
    cargarValores();

    int altoDisponible = 700;
    if (QScreen* s = QApplication::primaryScreen())
        altoDisponible = qMax(380, s->availableGeometry().height() - 80);
    resize(560, qMin(720, altoDisponible));
}

void SettingsDialog::construirUI() {
    auto* mainL = new QVBoxLayout(this);
    mainL->setSpacing(14);
    mainL->setContentsMargins(18, 18, 18, 18);

    // Título
    auto* titulo = new QLabel("⚙  Preferencias del Dock");
    titulo->setStyleSheet("font-size: 19px; font-weight: bold; color: white; margin-bottom: 4px;");
    mainL->addWidget(titulo);

    auto* tabs = new QTabWidget;
    mainL->addWidget(tabs);

    auto* tabApar = new QWidget; construirTabApariencia(tabApar);
    auto* tabComp = new QWidget; construirTabComportamiento(tabComp);
    auto* tabApps = new QWidget; construirTabApps(tabApps);

    tabs->addTab(paginaConScroll(tabApar), "🎨  Apariencia");
    tabs->addTab(paginaConScroll(tabComp), "⚡  &Comportamiento");
    tabs->addTab(paginaConScroll(tabApps), "📦  Aplicaciones");

    // Botones
    auto* btnBox = new QDialogButtonBox;
    auto* btnOk      = btnBox->addButton("Aceptar",  QDialogButtonBox::AcceptRole);
    auto* btnCancel  = btnBox->addButton("Cancelar", QDialogButtonBox::RejectRole);
    auto* btnApply   = btnBox->addButton("Aplicar",  QDialogButtonBox::ApplyRole);
    btnOk->setObjectName("btnAceptar");
    btnOk->setDefault(true);
    btnApply->setObjectName("btnAplicar");

    connect(btnBox, &QDialogButtonBox::accepted, this, [this](){ guardarValores(); accept(); });
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(btnApply, &QPushButton::clicked, this, &SettingsDialog::aplicar);
    mainL->addWidget(btnBox);
}

void SettingsDialog::construirTabApariencia(QWidget* tab) {
    auto* L = new QVBoxLayout(tab);
    L->setSpacing(10);
    L->setContentsMargins(12, 12, 12, 12);

    // Posición y monitor
    auto* grpPos = new QGroupBox("Posición");
    auto* fPos   = new QFormLayout(grpPos);
    fPos->setSpacing(10);
    m_comboPosicion = new QComboBox;
    m_comboPosicion->addItems({"Abajo", "Arriba", "Izquierda", "Derecha"});
    fPos->addRow("Posición del dock:", m_comboPosicion);

    m_comboMonitor = new QComboBox;
    for (int i = 0; i < QApplication::screens().size(); i++)
        m_comboMonitor->addItem(QString("Monitor %1").arg(i+1));
    fPos->addRow("Pantalla:", m_comboMonitor);

    // Modo panel: estira el dock a todo el borde de la pantalla manteniendo
    // los bordes redondeados.
    m_checkPanel = new QCheckBox("Extender como panel (ancho completo)");
    fPos->addRow("", m_checkPanel);
    L->addWidget(grpPos);

    // Tamaño e íconos
    auto* grpTam = new QGroupBox("Íconos y Magnificación");
    auto* fTam   = new QFormLayout(grpTam);
    fTam->setSpacing(10);

    auto* rowIcono = new QHBoxLayout;
    m_sliderIcono = new QSlider(Qt::Horizontal);
    m_sliderIcono->setRange(24, 96);
    m_labelIcono  = new QLabel("48 px");
    m_labelIcono->setMinimumWidth(48);
    rowIcono->addWidget(m_sliderIcono);
    rowIcono->addWidget(m_labelIcono);
    connect(m_sliderIcono, &QSlider::valueChanged, [this](int v){
        m_labelIcono->setText(QString("%1 px").arg(v));
    });
    fTam->addRow("Tamaño de ícono:", rowIcono);

    m_checkMag = new QCheckBox("Activar magnificación al pasar el cursor");
    fTam->addRow("", m_checkMag);

    auto* rowMag = new QHBoxLayout;
    m_sliderMag = new QSlider(Qt::Horizontal);
    m_sliderMag->setRange(110, 250);
    m_labelMag  = new QLabel("160%");
    m_labelMag->setMinimumWidth(48);
    rowMag->addWidget(m_sliderMag);
    rowMag->addWidget(m_labelMag);
    connect(m_sliderMag, &QSlider::valueChanged, [this](int v){
        m_labelMag->setText(QString("%1%").arg(v));
    });
    fTam->addRow("Tamaño máximo:", rowMag);
    connect(m_checkMag, &QCheckBox::toggled, m_sliderMag, &QWidget::setEnabled);

    auto* rowPanel = new QHBoxLayout;
    m_sliderPanel = new QSlider(Qt::Horizontal);
    m_sliderPanel->setRange(30, 90);
    m_labelPanel  = new QLabel("42 px");
    m_labelPanel->setMinimumWidth(48);
    rowPanel->addWidget(m_sliderPanel);
    rowPanel->addWidget(m_labelPanel);
    connect(m_sliderPanel, &QSlider::valueChanged, [this](int v){
        m_labelPanel->setText(QString("%1 px").arg(v));
    });
    fTam->addRow("Altura del panel:", rowPanel);
    L->addWidget(grpTam);

    // Fondo
    auto* grpFondo = new QGroupBox("Fondo y Estilo");
    auto* fFondo   = new QFormLayout(grpFondo);
    fFondo->setSpacing(10);

    m_checkTranslucido = new QCheckBox("Fondo translúcido (efecto vidrio)");
    fFondo->addRow("", m_checkTranslucido);

    auto* rowOp = new QHBoxLayout;
    m_sliderOpacidad = new QSlider(Qt::Horizontal);
    m_sliderOpacidad->setRange(20, 100);
    m_labelOpacidad  = new QLabel("80%");
    m_labelOpacidad->setMinimumWidth(48);
    rowOp->addWidget(m_sliderOpacidad);
    rowOp->addWidget(m_labelOpacidad);
    connect(m_sliderOpacidad, &QSlider::valueChanged, [this](int v){
        m_labelOpacidad->setText(QString("%1%").arg(v));
    });
    fFondo->addRow("Opacidad:", rowOp);
    connect(m_checkTranslucido, &QCheckBox::toggled, m_sliderOpacidad, &QWidget::setEnabled);

    m_checkIndicadores = new QCheckBox("Mostrar puntos de ejecución");
    fFondo->addRow("", m_checkIndicadores);

    m_checkEtiquetas = new QCheckBox("Mostrar nombre de app al pasar el cursor");
    fFondo->addRow("", m_checkEtiquetas);

    auto* rowRadio = new QHBoxLayout;
    m_spinRadio = new QSpinBox;
    m_spinRadio->setRange(0, 30);
    m_spinRadio->setSuffix(" px");
    rowRadio->addWidget(m_spinRadio);
    rowRadio->addStretch();
    fFondo->addRow("Radio de esquinas:", rowRadio);
    L->addWidget(grpFondo);

    L->addStretch();
}

void SettingsDialog::construirTabComportamiento(QWidget* tab) {
    auto* L = new QVBoxLayout(tab);
    L->setSpacing(10);
    L->setContentsMargins(12, 12, 12, 12);

    auto* grp = new QGroupBox("Visibilidad");
    auto* f   = new QFormLayout(grp);
    f->setSpacing(10);
    m_comboComportamiento = new QComboBox;
    m_comboComportamiento->addItems({"Siempre visible", "Auto-ocultar"});
    f->addRow("Comportamiento:", m_comboComportamiento);
    L->addWidget(grp);

    auto* grp2 = new QGroupBox("Lanzamiento");
    auto* f2   = new QFormLayout(grp2);
    f2->setSpacing(10);
    m_checkRebotar = new QCheckBox("Animar ícono al abrir una aplicación");
    f2->addRow("", m_checkRebotar);
    L->addWidget(grp2);

    // Perfil de rendimiento: no es una preferencia "en caliente", cambia el
    // perfil visual al arrancar (el dock se reinicia al aplicarlo).
    auto* grpPerf = new QGroupBox("Rendimiento / GPU");
    auto* vPerf   = new QVBoxLayout(grpPerf);
    vPerf->setSpacing(6);
    m_checkLigero = new QCheckBox("Modo &ultra ligero (equipos con GPU débil)");
    vPerf->addWidget(m_checkLigero);
    auto* notaPerf = new QLabel(
        "Fondo plano opaco, sin esquinas redondeadas ni sombras/glow, y "
        "magnificación limitada a ~30 fps. Al cambiarlo, el dock se reinicia "
        "para aplicar el perfil.");
    notaPerf->setWordWrap(true);
    notaPerf->setStyleSheet("color: #888; font-size: 11px; margin-left: 29px;");
    vPerf->addWidget(notaPerf);
    L->addWidget(grpPerf);

    L->addStretch();
}

void SettingsDialog::setModoLigero(bool on) {
    if (m_checkLigero) m_checkLigero->setChecked(on);
}

bool SettingsDialog::modoLigero() const {
    return m_checkLigero && m_checkLigero->isChecked();
}

void SettingsDialog::construirTabApps(QWidget* tab) {
    auto* L = new QVBoxLayout(tab);
    L->setSpacing(8);
    L->setContentsMargins(12, 12, 12, 12);

    // Apps ancladas
    auto* lblAnc = new QLabel("Aplicaciones en el Dock:");
    lblAnc->setStyleSheet("font-weight: bold; color: white; margin-bottom: 2px;");
    L->addWidget(lblAnc);

    m_listaAncladas = new QListWidget;
    m_listaAncladas->setIconSize(QSize(24, 24));
    m_listaAncladas->setFixedHeight(140);
    L->addWidget(m_listaAncladas);

    auto* rowBtns = new QHBoxLayout;
    auto* btnQuitar = new QPushButton("Quitar selección");
    connect(btnQuitar, &QPushButton::clicked, [this](){
        delete m_listaAncladas->currentItem();
    });
    rowBtns->addWidget(btnQuitar);
    rowBtns->addStretch();
    L->addLayout(rowBtns);

    // Buscador de apps
    auto* lblDisp = new QLabel("Todas las aplicaciones instaladas:");
    lblDisp->setStyleSheet("font-weight: bold; color: white; margin-top: 8px; margin-bottom: 2px;");
    L->addWidget(lblDisp);

    m_listaDisponibles = new QListWidget;
    m_listaDisponibles->setIconSize(QSize(24, 24));
    L->addWidget(m_listaDisponibles);

    // Poblar apps disponibles
    for (const InfoApp& app : m_todasApps) {
        if (app.nombre.isEmpty()) continue;
        auto* it = new QListWidgetItem(app.icono, app.nombre);
        it->setData(Qt::UserRole, app.archivoDesktop);
        m_listaDisponibles->addItem(it);
    }

    auto* btnAgregar = new QPushButton("⬆  Anclar al Dock");
    btnAgregar->setObjectName("btnAplicar");
    connect(btnAgregar, &QPushButton::clicked, [this](){
        auto* sel = m_listaDisponibles->currentItem();
        if (!sel) return;
        QString df = sel->data(Qt::UserRole).toString();
        // No duplicar
        for (int i=0; i<m_listaAncladas->count(); i++)
            if (m_listaAncladas->item(i)->data(Qt::UserRole).toString() == df) return;
        auto* nit = new QListWidgetItem(sel->icon(), sel->text());
        nit->setData(Qt::UserRole, df);
        m_listaAncladas->addItem(nit);
    });
    L->addWidget(btnAgregar);
}

void SettingsDialog::cargarValores() {
    m_comboPosicion->setCurrentIndex((int)m_cfg.posicion);
    m_comboMonitor->setCurrentIndex(qBound(0, m_cfg.indiceMonitor, m_comboMonitor->count()-1));
    m_checkPanel->setChecked(m_cfg.modoPanel);
    m_sliderIcono->setValue(m_cfg.tamanoIcono);
    m_labelIcono->setText(QString("%1 px").arg(m_cfg.tamanoIcono));
    m_sliderPanel->setValue(m_cfg.altoPanel);
    m_labelPanel->setText(QString("%1 px").arg(m_cfg.altoPanel));
    m_checkMag->setChecked(m_cfg.magnificacion);
    m_sliderMag->setValue(m_cfg.multiplicadorMag);
    m_sliderMag->setEnabled(m_cfg.magnificacion);
    m_labelMag->setText(QString("%1%").arg(m_cfg.multiplicadorMag));
    m_checkTranslucido->setChecked(m_cfg.fondoTranslucido);
    int op = (int)(m_cfg.opacidadFondo * 100);
    m_sliderOpacidad->setValue(op);
    m_labelOpacidad->setText(QString("%1%").arg(op));
    m_sliderOpacidad->setEnabled(m_cfg.fondoTranslucido);
    m_checkIndicadores->setChecked(m_cfg.mostrarIndicadores);
    m_checkEtiquetas->setChecked(m_cfg.mostrarEtiquetas);
    m_spinRadio->setValue(m_cfg.radioEsquina);
    m_comboComportamiento->setCurrentIndex((int)m_cfg.comportamiento);
    m_checkRebotar->setChecked(m_cfg.rebotarAlAbrir);

    // Cargar apps ancladas actuales
    m_listaAncladas->clear();
    for (DockItem* it : m_itemsActuales) {
        if (it->tipo() != DockItem::Tipo::Aplicacion || !it->estaAnclada()) continue;
        auto* li = new QListWidgetItem(it->infoApp().icono, it->infoApp().nombre);
        li->setData(Qt::UserRole, it->infoApp().archivoDesktop);
        m_listaAncladas->addItem(li);
    }
}

void SettingsDialog::guardarValores() {
    m_cfg.posicion           = (DockPosition)m_comboPosicion->currentIndex();
    m_cfg.indiceMonitor      = m_comboMonitor->currentIndex();
    m_cfg.modoPanel          = m_checkPanel->isChecked();
    m_cfg.tamanoIcono        = m_sliderIcono->value();
    m_cfg.altoPanel          = m_sliderPanel->value();
    m_cfg.magnificacion      = m_checkMag->isChecked();
    m_cfg.multiplicadorMag   = m_sliderMag->value();
    m_cfg.fondoTranslucido   = m_checkTranslucido->isChecked();
    m_cfg.opacidadFondo      = m_sliderOpacidad->value() / 100.0;
    m_cfg.mostrarIndicadores = m_checkIndicadores->isChecked();
    m_cfg.mostrarEtiquetas   = m_checkEtiquetas->isChecked();
    m_cfg.radioEsquina       = m_spinRadio->value();
    m_cfg.comportamiento     = (DockComportamiento)m_comboComportamiento->currentIndex();
    m_cfg.rebotarAlAbrir     = m_checkRebotar->isChecked();
}

void SettingsDialog::aplicar() {
    guardarValores();
    // Emitir señal al padre para que aplique inmediatamente
    if (parentWidget()) {
        // DockWindow::aplicarConfig será llamada al aceptar
    }
}

QStringList SettingsDialog::appsAncladas() const {
    QStringList lista;
    for (int i = 0; i < m_listaAncladas->count(); i++)
        lista << m_listaAncladas->item(i)->data(Qt::UserRole).toString();
    return lista;
}
