<div align="center">
  <img src="https://github.com/WolffsRoom/FMVita/raw/master/media/icon0.png" width="128" alt="Logo FMVita">
  <h1>FMVita</h1>
  <h3>File Manager Vita</h3>
  <p align="center">
    <a href="#installation">Installation</a> •
    <a href="#key-differences-and-new-features">Features</a> •
    <a href="#directory-structure-changes">Directories</a> •
    <a href="#technical-details-and-build">Build</a>
  </p>
</div>

---

**FMVita** is an enhanced fork of **VitaShell** (originally created by *TheFloW*). This version focuses on **usability**, expanding touchscreen support for the PS Vita, introducing animated backgrounds, various Quality of Life (QoL) improvements, and a new program file organization under the independent directory `ux0:FMVita/`.

<div align="center">
  <img src="https://github.com/WolffsRoom/FMVita/raw/master/media/Tela%20Inicial.png" alt="Home Screen" width="80%">
</div>

---

## Installation

FMVita was designed to prevent conflicts with your current configuration. Therefore, it features a **unique Title ID** and its own dedicated folder structure.

* **Zero Conflicts:** It can be installed side-by-side with the original VitaShell, OneMenuPlus, MolecularShell, or any other file manager.
* **How to Install:** Simply transfer the FMVita `.vpk` file to your console (via FTP or USB) and install it using VitaShell (or any other file manager application you currently use).

---

## Key Differences and New Features

### Interface and Themes

<div align="center">
  <img src="https://github.com/WolffsRoom/FMVita/raw/master/media/Modo%20Grid.png" alt="Grid Mode" width="80%">
</div>

* **Six Color Presets:** *Dark, Light, Blue, Red, Purple*, and *Brown*. The system automatically calculates text, highlight, selection, dialog, and border colors.
* **Quick Action Toolbar:** A new toolbar featuring 8 quick shortcuts: *Copy, Paste, Delete, Rename, Filter, Group, Search*, and *New*.
* **Transparency:** The interface elements (topbar, cards, and buttons) feature a reduced alpha channel when using image backgrounds (GIF or PNG) for better visibility.
* **Themed Scrollbar:** An adaptive scrollbar that matches the theme's highlight color.
* **Redesigned Address Bar:** Now displays the current directory path, battery status, and clock.
* **Partial Support for VitaShell Themes:** Due to the nature of the original project, themes developed for VitaShell are not 100% compatible with this version, only taking advantage of certain elements such as file icons.

### Animated Backgrounds

* **Four Procedural Animations:** *Particles* (floating PlayStation icons), *Waves* (colored mist), *Stars* (starfield), and *Squares* (PS2-styled falling rectangles).
* **Animated GIF Support:** Loads from `ux0:FMVita/Gif/theme.gif` (Mode 4) with multiple fallbacks. Powered by an `stb_image` engine featuring frame delay control, cover-fit scaling, and clipping.
* **Static PNG Support:** Native loading from `ux0:FMVita/Background/bg.png` (Mode 5).

### Touchscreen Support

<div align="center">
  <img src="https://github.com/WolffsRoom/FMVita/raw/master/media/FTP%20Server.png" alt="FTP Server" width="80%">
</div>

* **Native Dialogs:** The old system YES/NO prompt has been replaced by custom **Touch Dialogs** (Confirmation, Deletion, and FTP) styled with PlayStation button icons.
* **Drag-and-Drop:** Hold `Square` + Touch (or perform a long press) to drag files into other folders. Features *Auto-scroll* when moving items near the screen edges.
* **Swipe Gestures:** Swipe Left to go *Back*, Swipe Right to *Enter*.
* **Multi-tap:** Double-tap and hold to open the context menu.
* **Mouse Cursor:** Control a mouse pointer using the right analog stick (click with `Cross`), featuring a smooth fade effect after inactivity.

### Navigation and Layout

<div align="center">
  <img src="https://github.com/WolffsRoom/FMVita/raw/master/media/Modo%20Coluna.png" alt="Column Mode 1" width="49%">
  <img src="https://github.com/WolffsRoom/FMVita/raw/master/media/Modo%20Coluna2.png" alt="Column Mode 2" width="49%">
</div>

* **Column View:** An innovative 3-column layout (*Grandparent / Parent / Current*) for ultra-fast directory browsing. Highlights the active folder in gold within the upper columns and allows direct touch navigation on any column.
* **Bookmarks and Recents:** The `Square` shortcut jumps directly to your bookmarks; the `Triangle` shortcut navigates to recent files (which automatically generate `.lnk` symlinks).
* **Search Tools:** A Filter system (All/Folders/Files) on the toolbar and case-insensitive Search within the current directory.
* **Enhanced Scrolling:** Infinite scroll wrapping (from top to bottom and vice versa) and smooth lerp-based scrolling with acceleration.
* **Symlinks:** Optimized tracking via a `SymlinkDirectoryPath` linked list to ensure correct navigation through shortcuts.

### Undo System

* Undo **Move** and **Copy** actions directly from the context menu (`UNDO_ACTION`).
* *Undo Move:* Reverts the `sceIoRename` command.
* *Undo Copy:* Cleanly deletes the file copied by mistake.
* The *Undo* option appears dynamically and also registers actions performed via Drag-and-Drop.

### Security and Configuration

<div align="center">
  <img src="https://github.com/WolffsRoom/FMVita/raw/master/media/Tela%20de%20Config.png" alt="Configuration Screen" width="80%">
</div>

* **System Protection:** Renaming locks on critical system folders (verified via `isProtectedPath()`) with custom error messages.
* **Independent Settings:** Reads and writes directly to `ux0:FMVita/settings.txt` (`use_custom_config = 1` by default; can be temporarily disabled by holding `L` during boot).
* **New Menu Options:** *USB Device* (4 modes), *SELECT Button* (USB/FTP), *View Mode* (List/Grid/Column), *Background Anim* (6 modes), and *Theme Preset* (6 colors).

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

---

## Technical Details and Build

### Source Code Modifications
* **New Modules:** `modules/kernel/` (loads its own `umass.skprx`), `modules/patch/`, `modules/user/`, and the `usbdevice/` driver.
* **New Files:** Implemented `buttons.c/.h` for PlayStation button rendering, `main_context.c` for reorganized context submenus, and a GIF decoding engine using `stb_image.h`.
* **Localization (`english_us.txt`):** Over 27 new entries integrated and copied to the native directory upon first launch.
* **Internal Refactoring:** Added over 30 inline theme color functions in `main.h`, restructured `browser.c` (fixing D-PAD scroll issues, column view, touch engine), and cleaned up legacy options in `settings.c`.

### Build Instructions
Requires [vitasdk](https://github.com/vitasdk) and the standard toolchains properly installed and configured.

```bash
mkdir build
cd build
cmake ..
make
```

## Credits
* **TheFloW** — Creator of the original VitaShell.
* **Team Molecule** — HENkaku.
* **xerpi** — ftpvitalib and vita2dlib libraries.
* **Sean Barrett** — stb_image implementation.
* **WolffsRoom** — FMVita development, visual modifications, and engine.

---

## AI Notice
Artificial Intelligence tools were utilized as a support resource during the development of this project:
* **Gemini and DeepSeek:** Assisted in bug fixing, logical troubleshooting, and C/C++ code optimization.
* **Claude:** Used for brainstorming, project structuring, and prototyping new features.
