<div align="center">

  <!-- Logo Adaptável ao Tema (Light / Dark Mode) -->
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="https://raw.githubusercontent.com/WolffsRoom/FMVita/master/media/Media/%C3%8Dcones%20-%20Branco.svg">
    <img src="https://raw.githubusercontent.com/WolffsRoom/FMVita/master/media/Media/%C3%8Dcones%20-%20Preto.svg" width="128" alt="FMVita Logo">
  </picture>

  <h1>FMVita</h1>
  <p><b>File Manager Vita</b> — Gerenciador de arquivos completo e otimizado para o PlayStation Vita.</p>

  <!-- Badges -->
  <p>
    <a href="https://github.com/WolffsRoom/FMVita/releases">
      <img src="https://img.shields.io/github/v/release/WolffsRoom/FMVita?style=for-the-badge&color=007acc&label=Release" alt="Latest Release">
    </a>
    <img src="https://img.shields.io/badge/Platform-PS_Vita-6f42c1?style=for-the-badge&logo=playstation&logoColor=white" alt="Platform">
    <a href="LICENSE">
      <img src="https://img.shields.io/github/license/WolffsRoom/FMVita?style=for-the-badge&color=2ea44f" alt="License">
    </a>
  </p>

  <!-- Navegação Rápida -->
  <p>
    <a href="#installation"><b>Instalação</b></a> •
    <a href="#key-differences-and-new-features"><b>Recursos</b></a> •
    <a href="#directory-structure-changes"><b>Diretórios</b></a> •
    <a href="#technical-details-and-build"><b>Build & Compilação</b></a> •
    <a href="#changelog"><b>Histórico</b></a>
  </p>

</div>
<br>

**FMVita** is an enhanced fork of **VitaShell** (originally created by *TheFloW*). This version focuses on **usability**, expanding touchscreen support for the PS Vita, introducing animated backgrounds, various Quality of Life (QoL) improvements, and a new program file organization under the independent directory `ux0:FMVita/`.

<div align="center">
  <img src="https://github.com/WolffsRoom/FMVita/blob/master/Screenshots/Tr%C3%AAs%20colunas.png" alt="3 Columns View" width="80%">
</div>

<br>

## Installation

FMVita was designed to prevent conflicts with your current configuration. Therefore, it features a **unique Title ID** and its own dedicated folder structure.

* **Zero Conflicts:** It can be installed side-by-side with the original VitaShell, OneMenuPlus, MolecularShell, or any other file manager.
* **How to Install:** Simply transfer the FMVita `.vpk` file to your console (via FTP or USB) and install it using VitaShell (or any other file manager application you currently use).

<br>

## Key Differences and New Features

### Interface and Themes

<div align="center">
  <img src="https://github.com/WolffsRoom/FMVita/blob/master/Screenshots/Tela%20Inicial%20(Grid).png" alt="Grid View" width="49%">
  <img src="https://github.com/WolffsRoom/FMVita/blob/master/Screenshots/FundobrancoWaves.png" alt="Waves Background - Light Theme" width="49%">
</div>

* **Eight Color Presets:** *Dark, Light, Blue, Red, Purple, Brown, Gray*, and *Custom*. The system automatically calculates text, highlight, selection, dialog, and border colors.
* **Custom Theme Support:** Select the *Custom* preset and edit `ux0:FMVita/colors.txt` to fully define every color in the interface.
* **Quick Action Toolbar:** A new toolbar featuring 9 quick shortcuts: *Move, Copy, Paste, Delete, Rename, Filter, Group, Search*, and *New*.
* **Transparency:** The interface elements (topbar, cards, and buttons) feature a reduced alpha channel when using image backgrounds (GIF or PNG) for better visibility.
* **Themed Scrollbar:** An adaptive scrollbar that matches the theme's highlight color.
* **Redesigned Address Bar:** Now displays the current directory path, battery status, and clock.
* **Partial Support for VitaShell Themes:** Due to the nature of the original project, themes developed for VitaShell are not 100% compatible with this version, only taking advantage of certain elements such as file icons.

### Animated Backgrounds

* **Four Procedural Animations:** *Particles* (floating PlayStation icons), *Waves* (colored mist), *Stars* (starfield), and *Squares* (PS2-styled falling rectangles).
* **Animated GIF Support:** Loads from `ux0:FMVita/Gif/theme.gif` (Mode 4) with multiple fallbacks. Powered by an `stb_image` engine featuring frame delay control, cover-fit scaling, and clipping. Requires 960×540, 8-bit color.
* **Static PNG Support:** Native loading from `ux0:FMVita/Background/bg.png` (Mode 5). Requires 960×540, 8-bit color.

### Touchscreen Support

* **Native Dialogs:** The old system YES/NO prompt has been replaced by custom **Touch Dialogs** (Confirmation, Deletion, and FTP) styled with PlayStation button icons.
* **Drag-and-Drop:** Hold `Square` + Touch (or perform a long press) to drag files into other folders. Features *Auto-scroll* when moving items near the screen edges.
* **Swipe Gestures:** Swipe Left to go *Back*, Swipe Right to *Enter*.
* **Multi-tap:** Double-tap and hold to open the context menu.
* **Mouse Cursor:** Control a mouse pointer using the right analog stick (click with `Cross`), featuring a smooth fade effect after inactivity.

### Navigation and Layout

<div align="center">
  <img src="https://github.com/WolffsRoom/FMVita/blob/master/Screenshots/Tr%C3%AAs%20colunas.png" alt="3 Columns View" width="49%">
  <img src="https://github.com/WolffsRoom/FMVita/blob/master/Screenshots/Tela%20Inicial%20(Grid).png" alt="Grid View" width="49%">
</div>

* **View Modes:** Four layout options available under Settings → View Mode:
  * **List** — Traditional single-column list view.
  * **Grid** — Icon-based tile grid view.
  * **2 Columns** — Split-screen view showing Parent and Current folders side-by-side, each occupying exactly **50% of the screen width**.
  * **3 Columns** — Three-panel view (*Grandparent / Parent / Current*) for ultra-fast directory browsing.
* **Column View Touch Navigation:** Highlights the active folder in gold within the upper columns and allows direct touch navigation on any column.
* **Page Speed:** Configurable scroll repeat speed in Settings → Page Speed, applies to both D-PAD and analog stick navigation.
* **Bookmarks and Recents:** The `Square` shortcut jumps directly to your bookmarks; the `Triangle` shortcut navigates to recent files (which automatically generate `.lnk` symlinks).
* **Search Tools:** A Filter system (All/Folders/Files) on the toolbar and case-insensitive Search within the current directory.
* **Enhanced Scrolling:** Infinite scroll wrapping (from top to bottom and vice versa) and smooth lerp-based scrolling with acceleration.
* **Transition Animations:** Configurable Slide, Smooth Slide, or Fade transitions when navigating between folders. Can be disabled.
* **Symlinks:** Optimized tracking via a `SymlinkDirectoryPath` linked list to ensure correct navigation through shortcuts.

### VPK Management

<div align="center">
  <img src="https://github.com/WolffsRoom/FMVita/blob/master/Screenshots/Instalar%20VPK.png" alt="VPK Installation Dialog" width="80%">
</div>

* **Batch VPK Installation:** Mark multiple VPK files and install them all sequentially in one go — no need to install one at a time.
* **Background VPK Installation:** VPK installation runs from the file browser, no longer dependent on PKGj.
* **Post-Install Launch:** Optionally launch the just-installed application directly after installation.
* **QR Code Download & Auto-Install:** Scan a QR code containing a VPK URL to download and auto-install directly.

### FTP Server

<div align="center">
  <img src="https://github.com/WolffsRoom/FMVita/blob/master/Screenshots/FTP.png" alt="FTP Server Dialog" width="80%">
</div>

* **Wi-Fi Auto-Check:** If Wi-Fi is off, FMVita will prompt you to enable it before starting the FTP server — no more silent failures.
* **Improved Status Messages:** FTP operations now display human-readable progress descriptions and current activity (upload, download, rename, etc.).
* **Progress Tracking:** File transfer operations show 0–100% progress in the FTP status modal.
* **Auto-Refresh on Close:** The current folder is automatically refreshed when the FTP server is stopped.

### Image Viewer

<div align="center">
  <img src="https://github.com/WolffsRoom/FMVita/blob/master/Screenshots/PreviewImagem.png" alt="Image Preview" width="80%">
</div>

* Open any PNG, JPEG, BMP, or GIF image directly from the file browser.
* Supports **zoom**, **pan**, and **mirror** operations with touch and button controls.

### Undo System

* Undo **Move** and **Copy** actions directly from the context menu (`UNDO_ACTION`).
* *Undo Move:* Reverts the `sceIoRename` command.
* *Undo Copy:* Cleanly deletes the file copied by mistake.
* The *Undo* option appears dynamically and also registers actions performed via Drag-and-Drop.

### Security and Configuration

<div align="center">
  <img src="https://github.com/WolffsRoom/FMVita/blob/master/Screenshots/Menu%20Options.png" alt="Settings Screen" width="80%">
</div>

* **System Protection:** Renaming locks on critical system folders (verified via `isProtectedPath()`) with custom error messages.
* **Independent Settings:** Reads and writes directly to `ux0:FMVita/settings.txt` (`use_custom_config = 1` by default; can be temporarily disabled by holding `L` during boot).
* **Language Switching:** Change the interface language without rebooting the console — FMVita restarts itself to apply the change instantly.
* **New Menu Options:** *USB Device* (4 modes), *SELECT Button* (USB/FTP/QR), *View Mode* (List/Grid/2 Columns/3 Columns), *Background Anim* (7 modes), *Theme Preset* (8 options), *Page Speed*, *Transition Mode*, and *Scroll Loop*.

---

## Removed Features and Optimizations

To keep the application lightweight and focused purely on file management, the following features from the original VitaShell have been deprecated or hidden:
* **HENkaku Settings** menu (PSN spoofing, unsafe homebrew toggle, version spoofing).
* *Disable Auto-Update* and *Disable Warning Messages*.
* *Rear Touchpad* support.

---

## Directory Structure Changes

FMVita completely isolates its data from the original VitaShell:

| Original (VitaShell) | FMVita |
| :--- | :--- |
| `ux0:VitaShell/settings.txt` | `ux0:FMVita/settings.txt` |
| `ux0:VitaShell/language/` | `ux0:FMVita/language/` |
| `ux0:VitaShell/theme/` | `ux0:FMVita/theme/` |
| `ux0:data/lastdir.txt` | `ux0:FMVita/internal/lastdir.txt` |
| `ux0:VitaShell/bookmarks/` | `ux0:FMVita/bookmarks/` |
| `ux0:data/recent/` | `ux0:FMVITA/recent/` |
| `ux0:app/VITASHELL/module/*` | `ux0:FMVita/module/*` |
| *(Did not exist)* | `ux0:FMVita/Gif/theme.gif` |
| *(Did not exist)* | `ux0:FMVita/Background/bg.png` |
| *(Did not exist)* | `ux0:FMVita/colors.txt` |

---

## Technical Details and Build

### Source Code Modifications
* **New Modules:** `modules/kernel/` (loads its own `umass.skprx`), `modules/patch/`, `modules/user/`, and the `usbdevice/` driver.
* **New Files:** Implemented `buttons.c/.h` for PlayStation button rendering, `main_context.c` for reorganized context submenus, and a GIF decoding engine using `stb_image.h`.
* **Localization (`english_us.txt`):** Over 40 new entries integrated and copied to the native directory upon first launch. Language switch now restarts only FileManager, not the console.
* **Internal Refactoring:** Added over 30 inline theme color functions in `main.h`, restructured `browser.c` (fixing D-PAD scroll issues, column views with proper 50/50 split, touch engine), and cleaned up legacy options in `settings.c`.

### Build Instructions
Requires [vitasdk](https://github.com/vitasdk) and the standard toolchains properly installed and configured.

```bash
mkdir build
cd build
cmake ..
make
```

Or use the provided script:

```bash
export VITASDK=/usr/local/vitasdk
./build.sh
```

---

## Changelog

### v0.5.0 — 2026-06-17
* **2 Columns view — 50/50 split:** The two-column layout now divides the screen evenly (each column gets exactly 50% width), eliminating empty space on the right.
* **Improved text clipping in column modes:** Filename display boundaries are now dynamic — text is correctly clipped and scrolled within each column without spilling over.
* **Grandparent panel restricted to 3 Columns:** The grandparent folder panel is no longer drawn in 2 Columns mode, keeping the layout clean.
* **Better mouse hover in column modes:** Hover highlights are now correctly bounded to the active column area only.
* **Language switch restarts FileManager (not console):** Changing the language in Settings now triggers a FileManager restart instead of a full console reboot.
* **Custom theme preset:** Added a "Custom" theme option that reads colors from `ux0:FMVita/colors.txt`.
* **Batch VPK installation:** Mark multiple VPKs and install them all in sequence.
* **FTP Wi-Fi check modal:** FTP now warns if Wi-Fi is disabled before starting.
* **FTP status improvements:** More readable transfer descriptions and 0–100% progress tracking.
* **Auto-refresh on FTP close:** Current folder refreshes automatically after FTP is stopped.
* **Background image validation:** PNG/GIF backgrounds are now validated for 960×540 resolution and 8-bit color depth.
* **Page Speed via D-PAD and Analog stick:** Configurable scroll repeat rate now applies to both input methods.
* **Transition animation fixes:** Slide and Fade transitions overhauled to avoid visual bugs.

### v0.4.5
* View Mode with 3 Columns panel layout (Grandparent / Parent / Current).
* Grid view with icon tiles.
* Transition animations (Slide, Smooth Slide, Fade).
* Page speed setting for scroll rate.
* Scroll Loop toggle (wrap from top to bottom).
* Enhanced toolbar with 8 quick actions.
* Undo system for Move and Copy.
* Drag-and-drop file management with auto-scroll.

### v0.4.3
* QR code download and auto-install.
* Touch-friendly confirmation dialogs (no system dialogs).
* Post-install app launch.
* VPK context menu integration.
* Video player improvements.
* PlayStation button texture rendering.

---

## Credits
* **TheFloW** — Creator of the original VitaShell.
* **Team Molecule** — HENkaku.
* **xerpi** — ftpvitalib and vita2dlib libraries.
* **Sean Barrett** — stb_image implementation.
* **WolffsRoom** — FMVita development, visual modifications, and engine.

---

## AI Notice
Artificial Intelligence tools were utilized as a support resource during the development of this project:
* **Gemini (Google DeepMind):** Assisted in bug fixing, logical troubleshooting, C code optimization, and feature implementation throughout all phases.
* **Claude:** Used for brainstorming, project structuring, and prototyping new features.
