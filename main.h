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

#define INCLUDE_EXTERN_RESOURCE(name) extern unsigned char _binary_resources_##name##_start; extern unsigned char _binary_resources_##name##_size; \

// VitaShell version major.minor
#define VITASHELL_VERSION_MAJOR 0x02
#define VITASHELL_VERSION_MINOR 0x00

#define VITASHELL_VERSION ((VITASHELL_VERSION_MAJOR << 0x18) | (VITASHELL_VERSION_MINOR << 0x10))

#define VITASHELL_LASTDIR "ux0:FMVita/internal/lastdir.txt"

// needs / at the end
#define VITASHELL_BOOKMARKS_PATH "ux0:FMVita/bookmarks/"
#define VITASHELL_RECENT_PATH "ux0:FMVita/recent/"
#define VITASHELL_RECENT_PATH_DELETE_INTERVAL_DAYS 14

#define VITASHELL_TITLEID "VITASHELL"

#define ALIGN(x, align) (((x) + ((align) - 1)) & ~((align) - 1))

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

#define IS_DIGIT(i) (i >= '0' && i <= '9')

#define NOALPHA 0xFF

#define COLOR_ALPHA(color, alpha) (color & 0x00FFFFFF) | ((alpha & 0xFF) << 24)

// Font
#define FONT_SIZE 1.0f
#define FONT_X_SPACE 15.0f
#define FONT_Y_SPACE 40.0f

#define pgf_draw_text(x, y, color, text) \
  vita2d_pgf_draw_text(font, x, (y)+20, color, FONT_SIZE, text)

#define pgf_draw_textf(x, y, color, ...) \
  vita2d_pgf_draw_textf(font, x, (y)+20, color, FONT_SIZE, __VA_ARGS__)

#define pgf_text_width(text) \
  vita2d_pgf_text_width(font, FONT_SIZE, text)
  
// Screen
#define SCREEN_WIDTH 960
#define SCREEN_HEIGHT 544
#define SCREEN_HALF_WIDTH (SCREEN_WIDTH/2)
#define SCREEN_HALF_HEIGHT (SCREEN_HEIGHT/2)

// Main
#define SHELL_MARGIN_X 20.0f
#define SHELL_MARGIN_Y 18.0f
#define HEADER_H 30.0f
#define STATUSBAR_H 34.0f

#define PATH_Y (SHELL_MARGIN_Y + 1.0f * FONT_Y_SPACE)

#define START_Y (SHELL_MARGIN_Y + 2.0f * FONT_Y_SPACE + 4.0f)

#define MAX_WIDTH (SCREEN_WIDTH - 2.0f * SHELL_MARGIN_X)

#define STATUS_BAR_SPACE_X 12.0f

// Hex
#define HEX_OFFSET_X 147.0f
#define HEX_CHAR_X (SCREEN_WIDTH - SHELL_MARGIN_X - 0x10 * FONT_X_SPACE)
#define HEX_OFFSET_SPACE 34.0f

// Coredump
#define COREDUMP_INFO_X (SHELL_MARGIN_X + 160.0f)
#define COREDUMP_REGISTER_NAME_SPACE 45.0f
#define COREDUMP_REGISTER_SPACE 165.0f

// Scroll bar
#define SCROLL_BAR_X 0.0f
#define SCROLL_BAR_WIDTH 8.0f
#define SCROLL_BAR_MIN_HEIGHT 4.0f

// Context menu
#define CONTEXT_MENU_MIN_WIDTH 180.0f
#define CONTEXT_MENU_MARGIN 20.0f
#define CONTEXT_MENU_VELOCITY 10.0f

// File browser
#define FILE_X (SHELL_MARGIN_X+36.0f)
#define MARK_WIDTH (SCREEN_WIDTH - 2.0f * SHELL_MARGIN_X)
#define INFORMATION_X 680.0f
#define MAX_NAME_WIDTH 500.0f

// Uncommon dialog
#define UNCOMMON_DIALOG_MAX_WIDTH 800
#define UNCOMMON_DIALOG_PROGRESS_BAR_BOX_WIDTH 420.0f
#define UNCOMMON_DIALOG_PROGRESS_BAR_HEIGHT 8.0f
#define MSG_DIALOG_MODE_QR_SCAN 10

// QR Camera
#define CAM_WIDTH 640
#define CAM_HEIGHT 360

// Max entries
#define MAX_QR_LENGTH 1024
#define MAX_POSITION 16
#define MAX_ENTRIES 10
#define MAX_URL_LENGTH 128

#define BIG_BUFFER_SIZE 16 * 1024 * 1024

enum RefreshModes {
  REFRESH_MODE_NONE,
  REFRESH_MODE_NORMAL,
  REFRESH_MODE_SETFOCUS,
};

enum DialogSteps {
  DIALOG_STEP_NONE,

  DIALOG_STEP_CANCELED,

  DIALOG_STEP_ERROR,
  DIALOG_STEP_INFO,
  DIALOG_STEP_SYSTEM,

  DIALOG_STEP_REFRESH_LIVEAREA_QUESTION,
  DIALOG_STEP_REFRESH_LICENSE_DB_QUESTION,
  DIALOG_STEP_REFRESHING,

  DIALOG_STEP_USB_ATTACH_WAIT,

  DIALOG_STEP_FTP_WAIT,
  DIALOG_STEP_FTP,

  DIALOG_STEP_USB_WAIT,
  DIALOG_STEP_USB,

  DIALOG_STEP_RENAME,

  DIALOG_STEP_NEW_FILE,
  DIALOG_STEP_NEW_FOLDER,

  DIALOG_STEP_COPYING,
  DIALOG_STEP_COPIED,
  DIALOG_STEP_MOVED,
  DIALOG_STEP_PASTE,

  DIALOG_STEP_DELETE_QUESTION,
  DIALOG_STEP_DELETE_CONFIRMED,
  DIALOG_STEP_DELETING,
  DIALOG_STEP_DELETED,

  DIALOG_STEP_COMPRESS_NAME,
  DIALOG_STEP_COMPRESS_LEVEL,
  DIALOG_STEP_COMPRESSING,
  DIALOG_STEP_COMPRESSED,

  DIALOG_STEP_EXPORT_QUESTION,
  DIALOG_STEP_EXPORT_CONFIRMED,
  DIALOG_STEP_EXPORTING,

  DIALOG_STEP_INSTALL_QUESTION,
  DIALOG_STEP_INSTALL_CONFIRMED,
  DIALOG_STEP_INSTALL_CONFIRMED_QR,
  DIALOG_STEP_INSTALL_WARNING,
  DIALOG_STEP_INSTALL_WARNING_AGREED,
  DIALOG_STEP_INSTALLING,
  DIALOG_STEP_INSTALLED,

  DIALOG_STEP_UPDATE_QUESTION,
  DIALOG_STEP_DOWNLOADING,
  DIALOG_STEP_DOWNLOADED,
  DIALOG_STEP_EXTRACTING,
  DIALOG_STEP_EXTRACTED,

  DIALOG_STEP_HASH_QUESTION,
  DIALOG_STEP_HASH_CONFIRMED,
  DIALOG_STEP_HASHING,

  DIALOG_STEP_SETTINGS_AGREEMENT,
  DIALOG_STEP_SETTINGS_STRING,
  
  DIALOG_STEP_QR,
  DIALOG_STEP_QR_DONE,
  DIALOG_STEP_QR_WAITING,
  DIALOG_STEP_QR_CONFIRM,
  DIALOG_STEP_QR_DOWNLOADING,
  DIALOG_STEP_QR_DOWNLOADED,
  DIALOG_STEP_QR_DOWNLOADED_VPK,
  DIALOG_STEP_QR_OPEN_WEBSITE,
  DIALOG_STEP_QR_SHOW_CONTENTS,
  
  DIALOG_STEP_ENTER_PASSWORD,
  
  DIALOG_STEP_ADHOC_SEND_NETCHECK,
  DIALOG_STEP_ADHOC_SEND_WAITING,
  DIALOG_STEP_ADHOC_SEND_CLIENT_DECLINED,
  DIALOG_STEP_ADHOC_SENDING,
  DIALOG_STEP_ADHOC_SENDED,
  DIALOG_STEP_ADHOC_RECEIVE_NETCHECK,
  DIALOG_STEP_ADHOC_RECEIVE_SEARCHING,
  DIALOG_STEP_ADHOC_RECEIVE_QUESTION,
  DIALOG_STEP_ADHOC_RECEIVING,
  DIALOG_STEP_ADHOC_RECEIVED,
  DIALOG_STEP_SEARCH,
  DIALOG_STEP_DELETE_CONFIRM_TOUCH,
  DIALOG_STEP_TOUCH_CONFIRM,
  DIALOG_STEP_FTP_TOUCH,
};

extern vita2d_pgf *font;
extern char font_size_cache[256];

#include <psp2/touch.h>
extern SceTouchData touch;

extern char vita_ip[16];
extern unsigned short int vita_port;

extern char ftp_status_msg[256];

extern VitaShellConfig vitashell_config;

extern int SCE_CTRL_ENTER, SCE_CTRL_CANCEL;

extern int use_custom_config;

int getDialogStep();
void setDialogStep(int step);
int dialogSteps();

void initFtp();
void initUsb();

extern int toolbar_press_btn;
extern int toolbar_hover_btn;
void drawStatusBar();
void drawScrollBar(int pos, int n);
void drawShellInfo(const char *path);

void ftpvita_PROM(ftpvita_client_info_t *client);

int contextMenuMainEnterCallback(int sel, void *context);
int contextMenuHomeEnterCallback(int sel, void *context);
int contextMenuSortEnterCallback(int sel, void *context);

extern char delete_confirm_message[512];
extern int delete_confirm_is_folder;
void drawHeaderOverlay();
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
#define THEME_PRESET_GRAY   6

static inline unsigned int themeTopbarBg(int preset) {
  switch (preset) {
    case THEME_PRESET_LIGHT: return RGBA8(245, 243, 238, 245);
    case THEME_PRESET_BLUE:  return RGBA8(8, 22, 55, 245);
    case THEME_PRESET_RED:   return RGBA8(50, 14, 18, 245);
    case THEME_PRESET_PURPLE:return RGBA8(28, 12, 48, 245);
    case THEME_PRESET_BROWN: return RGBA8(40, 26, 14, 245);
    case THEME_PRESET_GRAY:  return RGBA8(45, 45, 45, 245);
    default:                 return RGBA8(14, 18, 28, 245);
  }
}
static inline unsigned int themeTopbarText(int preset) {
  switch (preset) {
    case THEME_PRESET_LIGHT: return RGBA8(25, 30, 45, 255);
    case THEME_PRESET_GRAY:  return RGBA8(220, 220, 220, 255);
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
    case THEME_PRESET_GRAY:  return RGBA8(20, 20, 20, 255);
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
    case THEME_PRESET_GRAY:  return RGBA8(30, 30, 30, 255);
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
    case THEME_PRESET_GRAY:  return RGBA8(25, 25, 25, 220);
    default:                 return RGBA8(10, 14, 22, 220);
  }
}
static inline unsigned int themeButtonDefault(int preset) {
  switch (preset) {
    case THEME_PRESET_LIGHT: return RGBA8(160, 165, 175, 220);
    case THEME_PRESET_GRAY:  return RGBA8(80, 80, 80, 220);
    default:                 return RGBA8(100, 108, 128, 220);
  }
}
static inline unsigned int themeButtonAccent(int preset) {
  switch (preset) {
    case THEME_PRESET_LIGHT: return RGBA8(42, 88, 175, 220);
    case THEME_PRESET_RED:   return RGBA8(210, 48, 48, 220);
    case THEME_PRESET_PURPLE:return RGBA8(130, 45, 215, 220);
    case THEME_PRESET_BROWN: return RGBA8(155, 105, 38, 220);
    case THEME_PRESET_GRAY:  return RGBA8(120, 120, 120, 220);
    default:                 return RGBA8(35, 90, 210, 220);
  }
}
static inline unsigned int themeButtonSuccess(int preset) {
  switch (preset) {
    case THEME_PRESET_LIGHT: return RGBA8(55, 155, 95, 220);
    case THEME_PRESET_GRAY:  return RGBA8(100, 150, 100, 220);
    default:                 return RGBA8(45, 175, 105, 220);
  }
}
static inline unsigned int themeButtonDanger(int preset) {
  switch (preset) {
    case THEME_PRESET_LIGHT: return RGBA8(195, 55, 55, 220);
    case THEME_PRESET_GRAY:  return RGBA8(150, 80, 80, 220);
    default:                 return RGBA8(210, 65, 65, 220);
  }
}
static inline unsigned int themeTextColor(int preset) {
  switch (preset) {
    case THEME_PRESET_LIGHT: return RGBA8(25, 30, 45, 255);
    case THEME_PRESET_GRAY:  return RGBA8(210, 210, 210, 255);
    default:                 return RGBA8(220, 225, 235, 255);
  }
}
static inline unsigned int themeTextDim(int preset) {
  switch (preset) {
    case THEME_PRESET_LIGHT: return RGBA8(80, 85, 100, 255);
    case THEME_PRESET_GRAY:  return RGBA8(140, 140, 140, 255);
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
    case THEME_PRESET_GRAY:  return RGBA8(180, 180, 180, 255);
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
    case THEME_PRESET_GRAY:  return RGBA8(160, 160, 160, 255);
    default:                 return RGBA8(60, 150, 255, 255);
  }
}
static inline unsigned int themeDialogBg(int preset) {
  switch (preset) {
    case THEME_PRESET_LIGHT: return RGBA8(230, 228, 222, 250);
    case THEME_PRESET_GRAY:  return RGBA8(35, 35, 35, 248);
    default:                 return RGBA8(15, 20, 30, 248);
  }
}
static inline unsigned int themeSelectionBg(int preset) {
  switch (preset) {
    case THEME_PRESET_LIGHT: return RGBA8(42, 88, 175, 40);
    case THEME_PRESET_GRAY:  return RGBA8(100, 100, 100, 60);
    default:                 return RGBA8(40, 100, 220, 60);
  }
}
static inline unsigned int themeSelectionLine(int preset) {
  switch (preset) {
    case THEME_PRESET_LIGHT: return RGBA8(42, 88, 175, 180);
    case THEME_PRESET_GRAY:  return RGBA8(150, 150, 150, 180);
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

