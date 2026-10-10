#!/bin/bash
# =============================================================================
# make-portable.sh — Empaca SGP (dock) como aplicación portable, sin depender
# portable, sin depender de Qt/librerías del sistema.
#
#   - Compila con Qt 6.11.2 (/opt/qt/6.11.2/gcc_64)
#   - Reúne binarios, librerías (Qt + sistema), plugins Qt y módulos QML
#   - Ajusta RPATH a $ORIGIN para que todo se resuelva dentro de la carpeta
#   - Genera: SGP-portable/  (y SGP-portable.tar.gz)
# =============================================================================
set -euo pipefail

# --- Rutas ---------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT="$SCRIPT_DIR"
BUILD_DIR="$PROJECT/build-qt611"
QT_ROOT="/opt/qt/6.11.2/gcc_64"
QT_BIN="$QT_ROOT/bin"
QT_LIB="$QT_ROOT/lib"
QT_PLUGINS="$QT_ROOT/plugins"

PORT_DIR="$PROJECT/SGP-portable"
PORT_BIN="$PORT_DIR/bin"
PORT_LIB="$PORT_DIR/lib"
PORT_PLUGINS="$PORT_DIR/plugins"

# Librerías de base del sistema que NO se empacan (glibc/kernel + drivers GL,
# porque son específicos del chipset/kernel de cada máquina):
EXCLUDE_LIBS="libc.so.6|libm.so.6|libdl.so.2|libpthread.so.0|librt.so.1|ld-linux.*|linux-vdso.*|libGL.so.1|libGLX.so.0|libEGL.so.1|libGLdispatch.so.0|libOpenGL.so.0|libgbm.so.1|libdrm.so.2"

echo "=============================================="
echo "  SGP — Portability packer (Qt 6.11.2)"
echo "=============================================="

# --- 1. Compilar con Qt 6.11.2 -------------------------------------------
echo ""
echo "[1/5] Compilando con Qt 6.11.2..."
unset LD_LIBRARY_PATH
# Si existe $QT_BIN/qmake6, evitar que caches del sistema interfieran
cmake -S "$PROJECT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH="$QT_ROOT" >/dev/null
cmake --build "$BUILD_DIR" -j"$(nproc)" >/dev/null
echo "      OK: build-qt611/sgp"

# --- 2. Preparar estructura ----------------------------------------------
echo "[2/5] Preparando estructura de SGP-portable/..."
rm -rf "$PORT_DIR"
mkdir -p "$PORT_BIN" "$PORT_LIB" "$PORT_PLUGINS"

# Binarios
cp "$BUILD_DIR/sgp" "$PORT_BIN/sgp"

# --- 3. Copiar plugins Qt -------------------------------------------------
echo "[3/5] Copiando plugins Qt..."
# Plugins esenciales (X11/xcb, imágenes, iconos SVG, input context, GL)
for p in \
    "platforms/libqxcb.so" "platforms/libqminimal.so" "platforms/libqoffscreen.so" \
    "imageformats/libqgif.so" "imageformats/libqico.so" "imageformats/libqjpeg.so" \
    "imageformats/libqsvg.so" \
    "platforminputcontexts/libcomposeplatforminputcontextplugin.so" \
    "platforminputcontexts/libibusplatforminputcontextplugin.so" \
    "iconengines/libqsvgicon.so" \
    "xcbglintegrations/libqxcb-egl-integration.so" \
    "xcbglintegrations/libqxcb-glx-integration.so" \
    "platformthemes/libqxdgdesktopportal.so" \
    "generic/libqevdevkeyboardplugin.so" "generic/libqevdevmouseplugin.so" \
    "generic/libqtuiotouchplugin.so" \
    ; do
    src="$QT_PLUGINS/$p"
    [ -f "$src" ] && install -D -m755 "$src" "$PORT_PLUGINS/$p"
done
echo "      plugins Qt copiados"

# --- 4. Reunir librerías (ldd recursivo) ---------------------------------
echo "[4/5] Reuniendo librerías dependientes (ldd)..."
# Alimentamos el ldd primero con el dir de Qt 6.11.2 y luego con el propio
# bundle para que ninguna .so de Qt se resuelva contra el sistema.
declare -A COPIED
collect_libs() {
    local bin="$1" out="$2" f path dep
    while IFS= read -r dep; do
        [ -z "$dep" ] && continue
        path="${dep#*=> }"
        path="${path%% *}"
        [ "$path" = "$dep" ] && continue                 # sin ruta resuelta
        [ -e "$path" ] || continue
        f="$(basename "$path")"
        [[ "$f" =~ ^($EXCLUDE_LIBS)$ ]] && continue      # base del sistema
        [ "${COPIED[$f]:-}" ] && continue
        COPIED[$f]=1
        cp -L "$path" "$out/"
        collect_libs "$path" "$out"                       # recursivo
    done < <(ldd "$bin" 2>/dev/null || true)
}

# Fase 1: binarios, resolviendo Qt contra /opt/qt 6.11.2
export LD_LIBRARY_PATH="$QT_LIB"
for f in "$PORT_BIN"/*; do
    [ -f "$f" ] && collect_libs "$f" "$PORT_LIB"
done

# Fase 2: TODOS los .so de plugins, resolviendo contra el bundle y,
# si falta algo de Qt, contra /opt/qt 6.11.2 (nunca contra el sistema).
# (el plugin qxcb necesita libxcb-cursor, libxcb-icccm…).
export LD_LIBRARY_PATH="$PORT_LIB:$QT_LIB"
while IFS= read -r so; do
    collect_libs "$so" "$PORT_LIB"
done < <(find "$PORT_PLUGINS" -name "*.so" -type f)

unset LD_LIBRARY_PATH
echo "      $(ls "$PORT_LIB" | wc -l) librerías copiadas a lib/"

# --- 5. RPATH $ORIGIN -----------------------------------------------------
echo "[5/5] Ajustando RPATH a \$ORIGIN..."
rp_rel() { # ruta relativa desde un dir de profundidad N hacia ../lib
    local depth; depth=$(awk -F/ '{print NF-1}' <<< "$1")
    local up=""; for ((i=0;i<depth;i++)); do up="../$up"; done
    echo "\$ORIGIN/${up}lib"
}
for f in "$PORT_BIN"/*; do
    [ -x "$f" ] && patchelf --set-rpath "\$ORIGIN/../lib" "$f"
done
for f in "$PORT_LIB"/*.so*; do
    rp="$(rp_rel "${f#"$PORT_DIR"/}")"
    patchelf --set-rpath "$rp" "$f"
done
for f in $(find "$PORT_PLUGINS" -name "*.so" -type f); do
    rp="$(rp_rel "${f#"$PORT_DIR"/}")"
    patchelf --set-rpath "$rp" "$f"
done
echo "      RPATH \$ORIGIN aplicado"

# --- Lanzador + extras ----------------------------------------------------
cat > "$PORT_DIR/sgp" <<'EOF'
#!/bin/bash
# SGP portable — lanzador: resuelve libs/plugins dentro de la carpeta.
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export LD_LIBRARY_PATH="$DIR/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export QT_PLUGIN_PATH="$DIR/plugins"
exec "$DIR/bin/sgp" "$@"
EOF
chmod +x "$PORT_DIR/sgp"

cat > "$PORT_DIR/README-portable.txt" <<'EOF'
SGP portable — Stellar Galaxy Dock
==================================

Qué incluye:
  bin/      sgp
  lib/      todas las librerías necesarias (Qt 6.11.2 + dependencias)
  plugins/  plugins Qt (platforms, imageformats, xcbglintegrations…)
  sgp       lanzador (define LD_LIBRARY_PATH / QT_PLUGIN_PATH)

Cómo ejecutar:
  ./sgp

Notas:
  * No requiere Qt instalado en el sistema (usa la Qt 6.11.2 empaquetada).
  * Requiere sistema con X11 y GPU con drivers OpenGL instalados (libGL de
    la máquina, no se empaqueta por ser dependiente del hardware).
  * Requiere glibc compatible (base del sistema, no se empaqueta).
  * Opciones de red/volumen/brillo se guardan en ~/.config/sgp/sgp.conf.
EOF

cp "$PROJECT/sgp.desktop" "$PORT_DIR/" 2>/dev/null || true

# --- Empaquetar .tar.gz ---------------------------------------------------
echo ""
echo "  Generando SGP-portable.tar.gz..."
tar -czf "$PROJECT/SGP-portable.tar.gz" -C "$PROJECT" SGP-portable

echo ""
echo "=============================================="
echo "  ¡Listo!"
echo "    Carpeta:  $PORT_DIR"
echo "    Archivo:  $PROJECT/SGP-portable.tar.gz"
echo "    Ejecutar: $PORT_DIR/sgp"
echo "=============================================="