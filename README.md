# SGP — Stellar Gravity Panel 🚀

<img width="1366" height="735" alt="Captura de tela de 2026-10-10 01-19-34" src="https://github.com/user-attachments/assets/01399718-dfef-4679-9e19-6036c7bc81ac" />

A ChromeOS and macOS-style application dock for Linux, built with C++ and Qt6.

## Features

### 🎯 Core Features (macOS parity)
- **App launching** — click to open, click again to focus running window
- **Running indicators** — dots below/beside icons show open windows
- **Window management** — right-click to see all open windows per app
- **Bounce animation** — icons bounce when launching applications
- **Icon magnification** — smooth zoom effect when hovering (like macOS)
- **App labels** — tooltip-style labels appear on hover
- **Pinned apps** — keep favourite apps in the dock permanently
- **Running apps** — uninstalled/unpinned running apps appear automatically
- **Context menus** — right-click for macOS-style app menus
- **Quit/close windows** — close apps directly from the dock
- **New window** — middle-click or right-click → "New Window"
- **Show/hide** — auto-hide with IntelliHide support

### 📐 Positioning
- **Bottom** (default, macOS-style)
- **Top**
- **Left**
- **Right**
- Multi-monitor support (select which screen)

### 🎨 Appearance
- Translucent frosted glass background
- Adjustable opacity
- Configurable icon size (24–96px)
- Magnification factor control
- Running indicator dots
- App label tooltips
- Drop shadows on icons
- Rounded corners (configurable radius)
- Custom border color/opacity

### ⚙️ Behavior
- **Always Visible** — dock is always shown
- **Auto-Hide** — hides when mouse leaves, slides back in
- **IntelliHide** — hides only when a window overlaps

### 🔧 System Integration
- Reads `.desktop` files from standard XDG paths
- Loads icons from freedesktop icon themes
- X11 EWMH integration (struts, window type, state)
- Sets `_NET_WM_WINDOW_TYPE_DOCK` for proper WM handling
- `_NET_WM_STRUT_PARTIAL` so maximized windows don't overlap
- Polls `_NET_CLIENT_LIST` to track running applications
- Activates windows via `_NET_ACTIVE_WINDOW` messages
- Closes windows via `_NET_CLOSE_WINDOW` messages

## Requirements

### Runtime
- Linux with X11 (Xorg)
- Qt6 (Core, Gui, Widgets, DBus)
- freedesktop-compliant desktop environment

Optional tools for the **system panel** (click on the clock):
- `nmcli` (NetworkManager) — list and connect WiFi networks
- `pactl` (PulseAudio/PipeWire) or `amixer` (ALSA) — volume and mute
- `brightnessctl` — brightness control (falls back to `/sys/class/backlight`)

### Build
- CMake 3.16+
- GCC or Clang with C++17
- Qt6 development headers
- Qt6 Quick/QML development headers (`qt6-declarative-dev`)
- X11 development libraries

## Installation

### Ubuntu / Debian
```bash
sudo apt install qt6-base-dev qt6-declarative-dev libx11-dev cmake build-essential
./install.sh
```

### Fedora / RHEL
```bash
sudo dnf install qt6-qtbase-devel libX11-devel cmake gcc-c++
./install.sh
```

### Arch Linux
```bash
sudo pacman -S qt6-base libx11 cmake base-devel
./install.sh
```

### Manual Build
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
sudo make install
```

## Configuration

SGP saves its configuration to:
```
~/.config/sgp/sgp.conf
```

### Right-click the dock background for quick settings:
- Change position (Bottom/Top/Left/Right)
- Open full Preferences dialog

### Preferences Dialog (right-click → Dock Preferences)

**Appearance Tab:**
- Position & display selection
- Icon size slider
- Magnification toggle + strength
- Background opacity
- Show/hide indicators, labels, shadows
- Corner radius

**Behavior Tab:**
- Always visible / Auto-hide / IntelliHide
- Bounce on launch
- Recent apps

**Apps Tab:**
- Manage pinned applications
- Browse all installed apps
- Pin/unpin with one click

## Usage

### Launching
```bash
sgp
```

### Lightweight emergency profile (`--ligero`)
On machines where the GPU really can't keep up, SGP can start in an emergency
profile:

```bash
sgp --ligero          # or: SGP_LIGERO=1 sgp
sgp --optimizado      # force the full profile for this run
```

It is **off by default**: without the flag the dock looks exactly as always.
When enabled it voluntarily trades part of the "glass" for GPU headroom:

- opaque flat panel background (no per-pixel transparency)
- no rounded corners or borders on the panel, popups and menus
- no hover glow and no icon drop shadows
- icon magnification capped at ~30 fps

Everything else (layout, clock/battery indicator, running dots, menus, grouped
windows, virtual-desktop popup) stays identical. Run `sgp --ayuda` for the
built-in help.

**Switching profiles from the UI.** Preferences → *Comportamiento* has a
**Modo ultra ligero** checkbox. The choice is saved as `modoLigero` in
`sgp.conf` and remembered across sessions. Because the profile is decided at
startup, applying the change closes and relaunches the dock automatically
(the binary is the same; only the startup flag changes). `--ligero` /
`--optimizado` / `SGP_LIGERO=1` on the command line override the saved
preference for that single run; the precedence is
**explicit flag > `SGP_LIGERO` > saved preference**. Autostart keeps launching
`sgp`, so it honours the saved preference.

### App Launcher (menú de aplicaciones)
The **first icon in the dock** (left tip in panel mode) opens the application
menu: categories on the left, applications on the right. Click a category to
see its apps and click an app to launch it. At the top there is a **search
bar**: type to filter all applications (as in the Cinnamon menu) and press
**Enter** to launch the first match. **Esc** clears the search, or closes the
menu if it is already empty. The last category, **Apagar y salir**, holds
session/power actions: lock, log out, suspend, reboot and shut down.

### Grouped windows (window popup)
When an application has **more than one window open** (e.g. two file-manager
folders), hovering its dock icon for a moment — or left-clicking it — shows a
popup listing that application's open windows (count and titles). Click a
window entry to bring it to the front, or click the active one again to
minimize it. The popup stays open while the pointer is over it or over the
dock icon.

### Virtual desktops (workspace switcher)
Next to the launcher button there is a **desktops button** (icon `icons/desktop.png`)
that opens a horizontal popup with one **dot per virtual desktop**. The active
desktop's dot is shown **blue**; the rest are grey. Click a dot to switch to
that desktop — the blue dot moves immediately — or switch elsewhere (e.g. the
workspace switcher, `wmctrl`) and the popup follows the real state while it
stays open. It closes when clicking outside.

### Clock & battery indicator
The clock at the end of the dock shows only the time (`HH:MM`). Its
**background box** works as a **battery indicator** (semi-transparent
bright dark navy blue):

- Battery fully charged → the whole box is bright navy blue.
- Discharging → the colored box shrinks **from right to left** as the level drops.
- Charging → it grows **from left to right** as the level rises.

The level and status (`Charging` / `Discharging` / `Full`) are read from
`/sys/class/power_supply/BAT*` every 5 seconds. With no battery present the
outline stays a faint neutral gray. A test hook is available via the
`SGP_BATERIA_TEST` environment variable (e.g. `SGP_BATERIA_TEST=37:Charging`)
to preview the indicator without touching the real battery.

### System Panel (click on the clock)
Left-click (or middle-click) the **clock at the end of the dock** to open a
system control panel, positioned above (or beside) the clock:

- **Red** — list of available WiFi networks (click one to connect via
  `nmcli`), a refresh button to rescan, and **Configurar red…** which opens
  the network configuration app of the desktop (`nm-connection-editor`,
  GNOME Control Center network panel, XFCE settings, or KDE System Settings).
- **Volumen** — slider with `−`/`+` buttons and mute toggle
  (`pactl` if available, otherwise `amixer`).
- **Brillo** — slider with `−`/`+` buttons (`brightnessctl`, falling back
  to writing `/sys/class/backlight`).

The panel closes automatically when you click outside it.

### Autostart
The installer adds `sgp.desktop` to `~/.config/autostart/`.

To remove autostart:
```bash
rm ~/.config/autostart/sgp.desktop
```

### Mouse Controls
| Action | Result |
|--------|--------|
| Left click (not running) | Launch app |
| Left click (running) | Focus/raise window |
| Middle click | Open new instance |
| Right click on app | Context menu |
| Right click on background | Dock settings |
| Hover | Show label + magnification |
| Left/middle click on clock | Open system panel (WiFi, volume, brightness) |
| Clock background box | Battery indicator (full = bright navy, shrinks/grows with level) |

### Context Menu (right-click app)
- App name (header)
- Keep in Dock / Remove from Dock
- Open / New Window
- Window list (if multiple windows open)
- Quit

## Architecture

```
src/
├── main.cpp              — Entry point
├── DockConfig.h          — Configuration structures
├── PerfilVisual.h/.cpp   — Emergency "glass-off" profile (--ligero)
├── DockWindow.h/.cpp     — Main dock window, layout, WM integration
├── DockItem.h/.cpp       — Individual icon widget (animations, events)
├── ApplicationScanner.h/.cpp — Scans .desktop files, loads icons
├── SettingsDialog.h/.cpp — Preferences UI
├── AppMenu.h/.cpp        — Application menu (categories + power actions)
└── TrashItem.h           — Trash/special items (placeholder)

resources/
└── icons/                — lanzador.png (dock launcher button icon)
```

## Known Limitations

- X11 only (Wayland support planned)
- Some DE-specific features (like Dash-to-Dock integration) not available
- Application matching relies on `StartupWMClass` in .desktop files; some apps may not be recognized automatically

## License

MIT License — free to use, modify, and distribute.
