/*
  VitaShell
  Copyright (C) 2015-2018, TheFloW

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __MAIN_H__
#define __MAIN_H__

#include <vitasdk.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <malloc.h>
#include <locale.h>

#include <math.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <archive.h>
#include <archive_entry.h>

#include <zlib.h>

#include <vita2d.h>
#include <ftpvita.h>

#include <taihen.h>

#include <vitashell_user.h>

#include "file.h"
#include "vitashell_config.h"
#include "vitashell_error.h"

void drawShellInfo(const char *path);
void drawHeaderRow();
void drawStatusBar();
void drawScrollBar(int pos, int n);
void drawShellInfo(const char *path);

void ftpvita_PROM(ftpvita_client_info_t *client);

// Context menu callbacks (for toolbar actions)
int contextMenuMainEnterCallback(int sel, void *context);
int contextMenuHomeEnterCallback(int sel, void *context);
int contextMenuSortEnterCallback(int sel, void *context);

// Custom delete confirmation dialog
extern char delete_confirm_message[512];
extern int delete_confirm_is_folder;
void drawDeleteConfirmDialog();

// Generic touch confirm dialog (replaces YESNO system dialogs)
#define MAX_CONFIRM_MSG 512
extern char touch_confirm_message[MAX_CONFIRM_MSG];
extern void (*touch_confirm_yes_cb)(void);
extern void (*touch_confirm_no_cb)(void);
void setTouchConfirm(const char *msg, void (*yes_cb)(void), void (*no_cb)(void));
void touchInfoOk(void);
void drawTouchConfirmDialog();

// FTP touch dialog
void drawFtpTouchDialog();

// Theme helpers
#define THEME_PRESET_DARK   0
#define THEME_PRESET_LIGHT  1
#define THEME_PRESET_BLUE   2
#define THEME_PRESET_RED    3
#define THEME_PRESET_PURPLE 4
#define THEME_PRESET_BROWN  5

static inline unsigned int themeTopbarBg(int preset) {
  switch (preset) {
    case THEME_PRESET_LIGHT: return RGBA8(245, 243, 238, 245);
    case THEME_PRESET_BLUE:  return RGBA8(8, 22, 55, 245);
    case THEME_PRESET_RED:   return RGBA8(50, 14, 18, 245);
    case THEME_PRESET_PURPLE:return RGBA8(28, 12, 48, 245);
    case THEME_PRESET_BROWN: return RGBA8(40, 26, 14, 245);
    default:                 return RGBA8(14, 18, 28, 245);
  }
}
static inline unsigned int themeTopbarText(int preset) {
  switch (preset) {
    case THEME_PRESET_LIGHT: return RGBA8(25, 30, 45, 255);
    default:                 return RGBA8(215, 218, 228, 255);
  }
}
static inline unsigned int themeBgColor(int preset) {
  switch (preset) {
    case THEME_PRESET_LIGHT: return RGBA8(220, 222, 225, 255);
    case THEME_PRESET_BLUE:  return RGBA8(6, 14, 30, 255);
    case THEME_PRESET_RED:   return RGBA8(32, 12, 14, 255);
    case THEME_PRESET_PURPLE:return RGBA8(18, 10, 28, 255);
    case THEME_PRESET_BROWN: return RGBA8(22, 16, 8, 255);
    default:                 return RGBA8(8, 12, 18, 255);
  }
}
static inline unsigned int themeListBg(int preset) {
  switch (preset) {
    case THEME_PRESET_LIGHT: return RGBA8(205, 207, 212, 255);
    case THEME_PRESET_BLUE:  return RGBA8(10, 18, 38, 255);
    case THEME_PRESET_RED:   return RGBA8(40, 18, 20, 255);
    case THEME_PRESET_PURPLE:return RGBA8(22, 14, 34, 255);
    case THEME_PRESET_BROWN: return RGBA8(30, 22, 12, 255);
    default:                 return RGBA8(12, 16, 24, 255);
  }
}
static inline unsigned int themeCardBg(int preset) {
  switch (preset) {
    case THEME_PRESET_LIGHT: return RGBA8(230, 228, 222, 220);
    case THEME_PRESET_BLUE:  return RGBA8(10, 20, 44, 220);
    case THEME_PRESET_RED:   return RGBA8(40, 18, 20, 220);
    case THEME_PRESET_PURPLE:return RGBA8(22, 14, 38, 220);
    case THEME_PRESET_BROWN: return RGBA8(30, 22, 12, 220);
    default:                 return RGBA8(10, 14, 22, 220);
  }
}
static inline unsigned int themeButtonDefault(int preset) {
  switch (preset) {
    case THEME_PRESET_LIGHT: return RGBA8(160, 165, 175, 220);
    default:                 return RGBA8(100, 108, 128, 220);
  }
}
static inline unsigned int themeButtonAccent(int preset) {
  switch (preset) {
    case THEME_PRESET_LIGHT: return RGBA8(42, 88, 175, 220);
    case THEME_PRESET_RED:   return RGBA8(210, 48, 48, 220);
    case THEME_PRESET_PURPLE:return RGBA8(130, 45, 215, 220);
    case THEME_PRESET_BROWN: return RGBA8(155, 105, 38, 220);
    default:                 return RGBA8(35, 90, 210, 220);
  }
}
static inline unsigned int themeButtonSuccess(int preset) {
  switch (preset) {
    case THEME_PRESET_LIGHT: return RGBA8(55, 155, 95, 220);
    default:                 return RGBA8(45, 175, 105, 220);
  }
}
static inline unsigned int themeButtonDanger(int preset) {
  switch (preset) {
    case THEME_PRESET_LIGHT: return RGBA8(195, 55, 55, 220);
    default:                 return RGBA8(210, 65, 65, 220);
  }
}
static inline unsigned int themeTextColor(int preset) {
  switch (preset) {
    case THEME_PRESET_LIGHT: return RGBA8(25, 30, 45, 255);
    default:                 return RGBA8(220, 225, 235, 255);
  }
}
static inline unsigned int themeTextDim(int preset) {
  switch (preset) {
    case THEME_PRESET_LIGHT: return RGBA8(80, 85, 100, 255);
    default:                 return RGBA8(160, 170, 190, 255);
  }
}
static inline unsigned int themeFolderColor(int preset) {
  switch (preset) {
    case THEME_PRESET_LIGHT: return RGBA8(42, 88, 175, 255);
    case THEME_PRESET_BLUE:  return RGBA8(60, 180, 240, 255);
    case THEME_PRESET_RED:   return RGBA8(255, 110, 90, 255);
    case THEME_PRESET_PURPLE:return RGBA8(180, 100, 255, 255);
    case THEME_PRESET_BROWN: return RGBA8(200, 160, 80, 255);
    default:                 return RGBA8(60, 180, 240, 255);
  }
}
static inline unsigned int themeAccentColor(int preset) {
  switch (preset) {
    case THEME_PRESET_LIGHT: return RGBA8(42, 88, 175, 255);
    case THEME_PRESET_BLUE:  return RGBA8(60, 150, 255, 255);
    case THEME_PRESET_RED:   return RGBA8(210, 60, 60, 255);
    case THEME_PRESET_PURPLE:return RGBA8(140, 60, 220, 255);
    case THEME_PRESET_BROWN: return RGBA8(170, 120, 40, 255);
    default:                 return RGBA8(60, 150, 255, 255);
  }
}
static inline unsigned int themeDialogBg(int preset) {
  switch (preset) {
    case THEME_PRESET_LIGHT: return RGBA8(230, 228, 222, 250);
    default:                 return RGBA8(15, 20, 30, 248);
  }
}
static inline unsigned int themeSelectionBg(int preset) {
  switch (preset) {
    case THEME_PRESET_LIGHT: return RGBA8(42, 88, 175, 40);
    default:                 return RGBA8(40, 100, 220, 60);
  }
}
static inline unsigned int themeSelectionLine(int preset) {
  switch (preset) {
    case THEME_PRESET_LIGHT: return RGBA8(42, 88, 175, 180);
    default:                 return RGBA8(60, 150, 255, 180);
  }
}

// Undo system
extern int undo_available;
extern int undo_type;
extern char undo_src[MAX_PATH_LENGTH];
extern char undo_dst[MAX_PATH_LENGTH];
#define UNDO_NONE 0
#define UNDO_MOVE 1
#define UNDO_DELETE 2
#define UNDO_COPY 3
void undoLastOperation(void);

#endif

