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

#include "main.h"
#include "main_context.h"
#include "browser.h"
#include "init.h"
#include "io_process.h"
#include "refresh.h"
#include "makezip.h"
#include "package_installer.h"
#include "context_menu.h"
#include "archive.h"
#include "photo.h"
#include "audioplayer.h"
#include "file.h"
#include "text.h"
#include "hex.h"
#include "settings.h"
#include "adhoc_dialog.h"
#include "property_dialog.h"
#include "message_dialog.h"
#include "netcheck_dialog.h"

#define GRID_COLS 5
#define GRID_CELL_W 180
#define GRID_CELL_H 88
#define GRID_GAP 8
#define GET_MAX_POSITION() ((vitashell_config.view_mode != 1) ? 11 : 30)
#define GRID_START_X 14
#define GRID_ICON_SIZE 56
#include "ime_dialog.h"
#include "theme.h"
#include "language.h"
#include "utils.h"
#include "uncommon_dialog.h"
#include "sfo.h"
#include "coredump.h"
#include "usb.h"
#include "qr.h"
#include "pfs.h"

INCLUDE_EXTERN_RESOURCE(default_mouse_png);

// File lists
FileList file_list, mark_list, copy_list, install_list, parent_list, child_list, grandparent_list;

// Paths
char cur_file[MAX_PATH_LENGTH];
char archive_copy_path[MAX_PATH_LENGTH];
char archive_path[MAX_PATH_LENGTH];

static char focus_name[MAX_NAME_LENGTH];

// Position
int base_pos = 0, rel_pos = 0;
static int base_pos_list[MAX_DIR_LEVELS];
static int rel_pos_list[MAX_DIR_LEVELS];
static int dir_level = 0;

// Modes
int sort_mode = SORT_BY_NAME;
int last_set_sort_mode = SORT_BY_NAME;
int copy_mode = COPY_MODE_NORMAL;
int file_type = FILE_TYPE_UNKNOWN;
int filter_mode = 0; // 0=Todos, 1=Pastas, 2=Arquivos
char search_term[256] = "";
int search_active = 0;

// Archive
static SceUID net_thid = -1;

float scroll_y = 0.0f;
float target_scroll_y = 0.0f;
int is_touching = 0;
int touch_y_start = 0;
int touch_x_start = 0;
float scroll_y_start = 0.0f;

// Undo system
int undo_type = UNDO_NONE;
int undo_available = 0;
char undo_src[MAX_PATH_LENGTH];
char undo_dst[MAX_PATH_LENGTH];

static void dragConfirmYes(void) {
  sceIoRename(undo_src, undo_dst);
  refreshFileList();
}
static void dragConfirmNo(void) {
  undo_available = 0;
  undo_type = UNDO_NONE;
}

void undoLastOperation(void) {
  if (!undo_available) return;
  if (undo_type == UNDO_MOVE) {
    sceIoRename(undo_dst, undo_src);
    undo_available = 0;
    undo_type = UNDO_NONE;
    refreshFileList();
  } else if (undo_type == UNDO_COPY) {
    removePath(undo_dst, NULL);
    undo_available = 0;
    undo_type = UNDO_NONE;
    refreshFileList();
  }
}

// Advanced Touch
int touch_frames = 0;
int is_dragging = 0;
int dragged_index = -1;
uint64_t last_tap_time = 0;
int last_tap_index = -1;
uint64_t last_last_tap_time = 0;
int last_last_tap_index = -1;
int touch_x_last = 0;
int touch_y_last = 0;
static int last_dialog_step = -1;
static int last_ctx_menu_mode = -1;
static int touch_handled = 0;

// Mouse cursor (right analog stick)
static float mouse_x = SCREEN_WIDTH / 2.0f;
static float mouse_y = SCREEN_HEIGHT / 2.0f;
static int mouse_inactive = 0;
static int mouse_visible = 0;
static vita2d_texture *mouse_tex = NULL;

// Drag state (Square + touch to move file)
static int drag_file_active = 0;
static char drag_file_name[MAX_NAME_LENGTH];
static char drag_src_path[MAX_PATH_LENGTH];

float wave_time = 0.0f;

#define NUM_PARTICLES 40
typedef struct { float x, y, vx, vy, radius, alpha; } Particle;
Particle particles[NUM_PARTICLES];
int particles_initialized = 0;

static float scroll_x = FILE_X;

// Archive
static int is_in_archive = 0;
static char dir_level_archive = -1;

// Scrolling filename
static int scroll_count = 0;

SceInt64 time_last_recent_files, time_last_bookmarks;

static void fileBrowserHandleSymlink(FileListEntry* file_entry);
static void fileBrowserHandleFolder(FileListEntry* file_entry);
static void fileBrowserHandleFile(FileListEntry* file_entry);

static void create_recent_symlink(FileListEntry *file_entry);

// escape from dir hierarchy with a symlink
typedef struct SymlinkDirectoryPath {
  struct SymlinkDirectoryPath* previous;
  // contains / at the end, is directory where jumped from
  char last_path[MAX_PATH_LENGTH];
  // contains / at the end, is directory where jumped to
  char last_hook[MAX_PATH_LENGTH];
} SymlinkDirectoryPath;

static SymlinkDirectoryPath* symlink_directory_path = NULL;
static SymlinkDirectoryPath* symlink_directory_path_head = NULL;

static void storeSymlinkPath(SymlinkDirectoryPath * path) {
  if (!symlink_directory_path) {
    symlink_directory_path = path;
    symlink_directory_path_head = path;
    symlink_directory_path->previous = 0;
  } else {
    SymlinkDirectoryPath *prev = symlink_directory_path;
    symlink_directory_path = path;
    symlink_directory_path->previous = prev;
  }
}

void dirLevelUp() {
  if (dir_level < MAX_DIR_LEVELS - 1) {
    base_pos_list[dir_level] = base_pos;
    rel_pos_list[dir_level] = rel_pos;
    dir_level++;
    base_pos_list[dir_level] = 0;
    rel_pos_list[dir_level] = 0;
  }

  base_pos = 0;
  rel_pos = 0;
}

void setDirArchiveLevel() {
  dir_level_archive = dir_level;
}

void setInArchive() {
  is_in_archive = 1;
}

int isInArchive() {
  return is_in_archive;
}

void dirUpCloseArchive() {
  if (isInArchive() && dir_level_archive >= dir_level) {
    is_in_archive = 0;
    archiveClose();
    dir_level_archive = -1;
  }
}

int change_to_directory(char *lastdir) {
  if (!checkFolderExist(lastdir)) {
    return VITASHELL_ERROR_NAVIGATION;
  } else {
    if (isInArchive()) {
      dirUpCloseArchive();
    }
    int i;
    for (i = 0; i < strlen(lastdir) + 1; i++) {
      if (lastdir[i] == ':' || lastdir[i] == '/') {
        char ch = lastdir[i + 1];
        lastdir[i + 1] = '\0';

        char ch2 = lastdir[i];
        lastdir[i] = '\0';

        char *p = strrchr(lastdir, '/');
        if (!p)
          p = strrchr(lastdir, ':');
        if (!p)
          p = lastdir - 1;

        lastdir[i] = ch2;

        refreshFileList();
        setFocusOnFilename(p + 1);

        strcpy(file_list.path, lastdir);

        lastdir[i + 1] = ch;

        dirLevelUp();
      }
    }
  }
  refreshFileList();
  return 0;
}

static void dirUp() {
  if (pfs_mounted_path[0] &&
      strcmp(file_list.path, pfs_mounted_path) == 0 && // we're about to leave the pfs path
      !strstr(copy_list.path, pfs_mounted_path)) { // nothing has been copied from pfs path
    // Then umount
    pfsUmount();
  }

  // skip all symlink hierarchies when pressing O in bookmarks/ recent files
  if (symlink_directory_path_head &&
      ((strncmp(file_list.path, VITASHELL_BOOKMARKS_PATH, MAX_PATH_LENGTH) == 0)
       || strncmp(file_list.path, VITASHELL_RECENT_PATH, MAX_PATH_LENGTH) == 0)) {
    strcpy(file_list.path, symlink_directory_path_head->last_path);
    SymlinkDirectoryPath *e = symlink_directory_path;
    while(e != NULL) {
      SymlinkDirectoryPath *prev = e->previous;
      free(e);
      dir_level--;
      e = prev;
    }
    symlink_directory_path_head = 0;
    symlink_directory_path = 0;
    goto DIR_UP_RETURN;
  }

  if (symlink_directory_path
      && strncmp(file_list.path, symlink_directory_path->last_hook, MAX_PATH_LENGTH) == 0) {
    strcpy(file_list.path, symlink_directory_path->last_path);
    SymlinkDirectoryPath* prev = symlink_directory_path->previous;
    free(symlink_directory_path);
    symlink_directory_path = prev;
    dir_level--;
    goto DIR_UP_RETURN;
  }

  removeEndSlash(file_list.path);

  char *p;
  p = strrchr(file_list.path, '/');
  if (p) {
    p[1] = '\0';
    dir_level--;
    goto DIR_UP_RETURN;
  }

  p = strrchr(file_list.path, ':');
  if (p) {
    if (strlen(file_list.path) - ((p + 1) - file_list.path) > 0) {
      p[1] = '\0';
      dir_level--;
      goto DIR_UP_RETURN;
    }
  }

  strcpy(file_list.path, HOME_PATH);
  dir_level = 0;

DIR_UP_RETURN:
  if (dir_level < 0)
    dir_level = 0;

  base_pos = (int)base_pos_list[dir_level];
  rel_pos = (int)rel_pos_list[dir_level];
  dirUpCloseArchive();
}

void setFocusName(const char *name) {
  strncpy(focus_name, name, MAX_NAME_LENGTH-1);
  focus_name[MAX_NAME_LENGTH-1] = '\0';
}

void setFocusOnFilename(const char *name) {
  int name_pos = fileListGetNumberByName(&file_list, name);
  if (name_pos < 0 || name_pos >= file_list.length)
    return;
  
  int max_pos = GET_MAX_POSITION();
  if (name_pos >= base_pos && name_pos < (base_pos + max_pos)) {
    rel_pos = name_pos - base_pos;
  } else if (name_pos < base_pos) {
    base_pos = name_pos;
    rel_pos = 0;
  } else if (name_pos >= (base_pos + max_pos)) {
    rel_pos = max_pos - 1;
    base_pos = name_pos - rel_pos;
  }
}

int refreshFileList() {
  int ret = 0, res = 0;

  // always sort recent files by date
  char *contains = strstr(file_list.path, VITASHELL_RECENT_PATH);
  if (contains) {
    sort_mode = SORT_BY_DATE;
  } else {
    sort_mode = last_set_sort_mode;
  }

  do {
    fileListEmpty(&file_list);

    res = fileListGetEntries(&file_list, file_list.path, sort_mode);

    if (res < 0) {
      ret = res;
      dirUp();
    }
  } while (res < 0);
    
  // Position correction
  int max_pos = GET_MAX_POSITION();
  if (file_list.length >= max_pos) {
    if (rel_pos > file_list.length - 1)
      rel_pos = max_pos - 1;

    if (base_pos > file_list.length - 1)
      base_pos = 0;
    if ((base_pos + max_pos - 1) >= file_list.length) {
      base_pos = file_list.length - max_pos;
      if (base_pos < 0) base_pos = 0;
    }
  } else {
    if ((base_pos + rel_pos) >= file_list.length) {
      rel_pos = file_list.length - 1;
    }
    
    base_pos = 0;
  }
  
  // Apply filter if active
  if (filter_mode > 0) {
    FileListEntry *entry = file_list.head;
    while (entry) {
      FileListEntry *next = entry->next;
      if (strcmp(entry->name, DIR_UP) != 0) {
        if ((filter_mode == 1 && !entry->is_folder) || 
            (filter_mode == 2 && entry->is_folder)) {
          fileListRemoveEntry(&file_list, entry);
        }
      }
      entry = next;
    }
    // Re-do position correction after filtering
    if (file_list.length >= MAX_POSITION) {
      if ((base_pos + rel_pos) >= file_list.length)
        rel_pos = MAX_POSITION - 1;
      if ((base_pos + MAX_POSITION - 1) >= file_list.length)
        base_pos = file_list.length - MAX_POSITION;
    } else {
      if ((base_pos + rel_pos) >= file_list.length)
        rel_pos = file_list.length - 1;
      base_pos = 0;
    }
  }

  // Apply search filter if active
  if (search_active && search_term[0] != '\0') {
    FileListEntry *entry = file_list.head;
    while (entry) {
      FileListEntry *next = entry->next;
      if (strcmp(entry->name, DIR_UP) != 0) {
        if (strcasestr(entry->name, search_term) == NULL) {
          fileListRemoveEntry(&file_list, entry);
        }
      }
      entry = next;
    }
    if (file_list.length >= MAX_POSITION) {
      if ((base_pos + rel_pos) >= file_list.length)
        rel_pos = MAX_POSITION - 1;
      if ((base_pos + MAX_POSITION - 1) >= file_list.length)
        base_pos = file_list.length - MAX_POSITION;
    } else {
      if ((base_pos + rel_pos) >= file_list.length)
        rel_pos = file_list.length - 1;
      base_pos = 0;
    }
  }
  if (vitashell_config.view_mode == 2) {
    if (dir_level > 0) {
      char parent_path[MAX_PATH_LENGTH];
      snprintf(parent_path, MAX_PATH_LENGTH, "%s", file_list.path);
      int len = strlen(parent_path);
      if (len > 0 && parent_path[len-1] == '/') parent_path[len-1] = '\0';
      char *slash = strrchr(parent_path, '/');
      if (slash) {
        slash[1] = '\0';
      } else {
        char *colon = strchr(parent_path, ':');
        if (colon) colon[1] = '\0';
      }
      fileListEmpty(&parent_list);
      // If parent resolves to same as current dir (device root), use HOME_PATH
      char cur_clean[MAX_PATH_LENGTH], pp_clean[MAX_PATH_LENGTH];
      strcpy(cur_clean, file_list.path); removeEndSlash(cur_clean);
      strcpy(pp_clean, parent_path); removeEndSlash(pp_clean);
      if (strcmp(cur_clean, pp_clean) == 0) {
        fileListGetEntries(&parent_list, HOME_PATH, sort_mode);
        strcpy(pp_clean, HOME_PATH); removeEndSlash(pp_clean);
      } else {
        fileListGetEntries(&parent_list, parent_path, sort_mode);
      }

      if (dir_level > 1) {
        char gparent_path[MAX_PATH_LENGTH];
        snprintf(gparent_path, MAX_PATH_LENGTH, "%s", pp_clean);
        len = strlen(gparent_path);
        if (len > 0 && gparent_path[len-1] == '/') gparent_path[len-1] = '\0';
        // Always add trailing slash for device-root paths to find parent
        if (strchr(gparent_path, ':') && !strchr(gparent_path, '/'))
          strcat(gparent_path, "/");
        slash = strrchr(gparent_path, '/');
        if (slash) {
          slash[1] = '\0';
        }
        // If gparent_path would be same as parent, use HOME_PATH for grandparent
        char gc_clean[MAX_PATH_LENGTH];
        strcpy(gc_clean, gparent_path); removeEndSlash(gc_clean);
        fileListEmpty(&grandparent_list);
        if (strcmp(pp_clean, gc_clean) != 0)
          fileListGetEntries(&grandparent_list, gparent_path, sort_mode);
        else
          fileListGetEntries(&grandparent_list, HOME_PATH, sort_mode);
      } else {
        fileListEmpty(&grandparent_list);
      }
    } else {
      fileListEmpty(&parent_list);
      fileListEmpty(&grandparent_list);
    }
  } else {
    fileListEmpty(&parent_list);
    fileListEmpty(&grandparent_list);
  }

  return ret;
}

static void refreshMarkList() {
  if (isInArchive())
    return;
  
  FileListEntry *entry = mark_list.head;

  int length = mark_list.length;

  int i;
  for (i = 0; i < length; i++) {
    // Get next entry already now to prevent crash after entry is removed
    FileListEntry *next = entry->next;

    char path[MAX_PATH_LENGTH];
    snprintf(path, MAX_PATH_LENGTH, "%s%s", file_list.path, entry->name);

    // Check if the entry still exits. If not, remove it from list
    SceIoStat stat;
    memset(&stat, 0, sizeof(SceIoStat));
    if (sceIoGetstat(path, &stat) < 0)
      fileListRemoveEntry(&mark_list, entry);

    // Next
    entry = next;
  }
}

static void refreshCopyList() {
  if (copy_list.is_in_archive)
    return;
  
  FileListEntry *entry = copy_list.head;

  int length = copy_list.length;

  int i;
  for (i = 0; i < length; i++) {
    // Get next entry already now to prevent crash after entry is removed
    FileListEntry *next = entry->next;

    char path[MAX_PATH_LENGTH];
    snprintf(path, MAX_PATH_LENGTH, "%s%s", copy_list.path, entry->name);

    // Check if the entry still exits. If not, remove it from list
    SceIoStat stat;
    memset(&stat, 0, sizeof(SceIoStat));
    if (sceIoGetstat(path, &stat) < 0)
      fileListRemoveEntry(&copy_list, entry);

    // Next
    entry = next;
  }
}

static void vpkInstallYes() { initMessageDialog(MESSAGE_DIALOG_PROGRESS_BAR, language_container[INSTALLING]); setDialogStep(DIALOG_STEP_INSTALL_CONFIRMED); }
static void vpkInstallNo() { setDialogStep(DIALOG_STEP_NONE); }

static int handleFile(const char *file, FileListEntry *entry) {
  int res = 0;

	// try to fix GPU freeze
	vita2d_wait_rendering_done();

  int type = getFileType(file);

  switch (type) {
    case FILE_TYPE_PSP2DMP:
    case FILE_TYPE_MP3:
    case FILE_TYPE_OGG:
    case FILE_TYPE_VPK:
    case FILE_TYPE_ARCHIVE:
      if (isInArchive())
        type = FILE_TYPE_UNKNOWN;

      break;
  }

  switch (type) {
    case FILE_TYPE_PSP2DMP:
      res = coredumpViewer(file);
      break;
      
    case FILE_TYPE_INI:
    case FILE_TYPE_TXT:
    case FILE_TYPE_XML:
    case FILE_TYPE_UNKNOWN:
      res = textViewer(file);
      break;
      
    case FILE_TYPE_BMP:
    case FILE_TYPE_PNG:
    case FILE_TYPE_JPEG:
      res = photoViewer(file, type, &file_list, entry, &base_pos, &rel_pos);
      break;

    case FILE_TYPE_MP3:
    case FILE_TYPE_OGG:
      res = audioPlayer(file, type, &file_list, entry, &base_pos, &rel_pos);
      break;
      
    case FILE_TYPE_SFO:
      res = SFOReader(file);
      break;
      
    case FILE_TYPE_VPK:
      setTouchConfirm(language_container[INSTALL_QUESTION], vpkInstallYes, vpkInstallNo);
      break;
      
    case FILE_TYPE_ARCHIVE:
      archiveClearPassword();
      res = archiveOpen(file);
      if (res >= 0 && archiveNeedPassword()) {
        initImeDialog(language_container[ENTER_PASSWORD], "", 128, SCE_IME_TYPE_BASIC_LATIN, 0, 1);
        setDialogStep(DIALOG_STEP_ENTER_PASSWORD);
      }
      break;
      
    default:
      res = textViewer(file);
      break;
  }

  if (res < 0) {
    errorDialog(res);
    return res;
  }

  return type;
}

int shortCuts() {
  // bookmarks shortcut
  if (current_pad[PAD_SQUARE]) {
    SceInt64 now = sceKernelGetSystemTimeWide();

    // switching too quickly back and forth between recent and bookmarks
    // causes VS to crash
    if (now - time_last_bookmarks > THRESHOLD_LAST_PAD_BOOKMARKS_WAIT) {
      if (strncmp(file_list.path, VITASHELL_BOOKMARKS_PATH, MAX_PATH_LENGTH) != 0) {
        char path[MAX_PATH_LENGTH] = VITASHELL_BOOKMARKS_PATH;
        sort_mode = last_set_sort_mode;
        jump_to_directory_track_current_path(path);
        time_last_bookmarks = now;
        return 0;
      }
    }
  }

  // recent files shortcut
  if (current_pad[PAD_TRIANGLE]) {
    SceInt64 now = sceKernelGetSystemTimeWide();
    if (now - time_last_recent_files > THRESHOLD_LAST_PAD_RECENT_FILES_WAIT) {
      if (strncmp(file_list.path, VITASHELL_RECENT_PATH, MAX_PATH_LENGTH) != 0) {
        char path[MAX_PATH_LENGTH] = VITASHELL_RECENT_PATH;
        sort_mode = SORT_BY_DATE;
        jump_to_directory_track_current_path(path);
        time_last_recent_files = now;
        return 0;
      }
    }
  }

  // QR
  if (current_pad[PAD_CIRCLE] && enabledQR()) {
    startQR();
    initMessageDialog(MESSAGE_DIALOG_QR_CODE, language_container[QR_SCANNING]);
    setDialogStep(DIALOG_STEP_QR);
  }

  return 0;
}

static int fileBrowserMenuCtrl() {
  int refresh = 0;

  // D-PAD Left/Right Navigation (Grid and Folders) — joystick only up/down
  if (pressed_pad[PAD_LEFT]) {
      if (vitashell_config.view_mode == 1) { // Grid View: move to previous column
          int old_pos = base_pos + rel_pos;
          int col = old_pos % GRID_COLS;
          if (col > 0 && old_pos > 0) {
              if (rel_pos > 0) rel_pos--;
              else if (base_pos > 0) base_pos--;
              scroll_count = 0;
              target_scroll_y = (vitashell_config.view_mode != 1) ? base_pos * FONT_Y_SPACE : (base_pos / GRID_COLS) * GRID_CELL_H;
              if (target_scroll_y < 0) target_scroll_y = 0;
          }
      } else { // List View: go back folder
          pressed_pad[PAD_CANCEL] = 1;
      }
  } else if (pressed_pad[PAD_RIGHT]) {
      if (vitashell_config.view_mode == 1) { // Grid View: move to next column
          int old_pos = base_pos + rel_pos;
          int col = old_pos % GRID_COLS;
          if ((col + 1) < GRID_COLS && (old_pos + 1) < file_list.length) {
              if ((rel_pos + 1) < GET_MAX_POSITION()) rel_pos++;
              else if ((base_pos + rel_pos + 1) < file_list.length) base_pos++;
              scroll_count = 0;
              target_scroll_y = (vitashell_config.view_mode != 1) ? base_pos * FONT_Y_SPACE : (base_pos / GRID_COLS) * GRID_CELL_H;
              if (target_scroll_y < 0) target_scroll_y = 0;
          }
      } else { // List View: enter folder
          pressed_pad[PAD_ENTER] = 1;
      }
  }

  // Settings menu
  if (pressed_pad[PAD_START]) {
    openSettingsMenu();
  }

  // SELECT button
  if (pressed_pad[PAD_SELECT]) {
    if (vitashell_config.select_button == SELECT_BUTTON_MODE_USB &&
        sceKernelGetModel() == SCE_KERNEL_MODEL_VITA) {
      if (isSafeMode()) {
        infoDialog(language_container[EXTENDED_PERMISSIONS_REQUIRED]);
      } else {
        SceUdcdDeviceState state;
        sceUdcdGetDeviceState(&state);

        if (state.cable & SCE_UDCD_STATUS_CABLE_CONNECTED) {
          initUsb();
        } else {
          initMessageDialog(SCE_MSG_DIALOG_BUTTON_TYPE_CANCEL,
                            language_container[USB_NOT_CONNECTED]);
          setDialogStep(DIALOG_STEP_USB_WAIT);
        }
      }
    } else if (vitashell_config.select_button == SELECT_BUTTON_MODE_FTP ||
               sceKernelGetModel() == SCE_KERNEL_MODEL_VITATV) {
      // Init FTP
      if (!ftpvita_is_initialized()) {
        int res = ftpvita_init(vita_ip, &vita_port);
        if (res < 0) {
          initMessageDialog(SCE_MSG_DIALOG_BUTTON_TYPE_CANCEL, language_container[PLEASE_WAIT]);
          setDialogStep(DIALOG_STEP_FTP_WAIT);
        } else {
          initFtp();
        }

        // Lock power timers
        powerLock();
      }

      // Dialog - custom touch FTP dialog
      if (ftpvita_is_initialized()) {
        setDialogStep(DIALOG_STEP_FTP_TOUCH);
      }
    }
  }

  // Move - with acceleration for long lists
  if (hold_pad[PAD_UP] || hold2_pad[PAD_LEFT_ANALOG_UP]) {
    int old_pos = base_pos + rel_pos;

    int steps = (vitashell_config.view_mode == 1) ? GRID_COLS : 1;
    int accel = 1;
    if (hold_pad[PAD_UP] && hold_count[PAD_UP] > 15) accel = 1 + (hold_count[PAD_UP] / 20);
    if (hold2_pad[PAD_LEFT_ANALOG_UP] && hold2_count[PAD_LEFT_ANALOG_UP] > 15) accel = 1 + (hold2_count[PAD_LEFT_ANALOG_UP] / 20);
    steps *= accel;
    for (int s = 0; s < steps; s++) {
        if (rel_pos > 0) {
          rel_pos--;
        } else if (base_pos > 0) {
          base_pos--;
        }
    }

    // Infinite scroll: wrap to last item when at top (single press only)
    if (old_pos == base_pos + rel_pos && file_list.length > 0 && accel == 1) {
      int limit = (vitashell_config.view_mode != 1) ? MAX_ENTRIES : 30;
      base_pos = (file_list.length > limit) ? file_list.length - limit : 0;
      rel_pos = file_list.length - 1 - base_pos;
    }

    if (old_pos != base_pos + rel_pos) {
      scroll_count = 0;
      target_scroll_y = (vitashell_config.view_mode != 1) ? base_pos * FONT_Y_SPACE : (base_pos / GRID_COLS) * GRID_CELL_H;
      if (target_scroll_y < 0) target_scroll_y = 0;
    }
  } else if (hold_pad[PAD_DOWN] || hold2_pad[PAD_LEFT_ANALOG_DOWN]) {
    int old_pos = base_pos + rel_pos;

    int steps = (vitashell_config.view_mode == 1) ? GRID_COLS : 1;
    int accel = 1;
    if (hold_pad[PAD_DOWN] && hold_count[PAD_DOWN] > 15) accel = 1 + (hold_count[PAD_DOWN] / 20);
    if (hold2_pad[PAD_LEFT_ANALOG_DOWN] && hold2_count[PAD_LEFT_ANALOG_DOWN] > 15) accel = 1 + (hold2_count[PAD_LEFT_ANALOG_DOWN] / 20);
    steps *= accel;
    for (int s = 0; s < steps; s++) {
        if ((old_pos + 1 + s) < file_list.length) {
          int limit = (vitashell_config.view_mode != 1) ? MAX_ENTRIES : 30;
          if ((rel_pos + 1) < limit) {
            rel_pos++;
          } else if ((base_pos + rel_pos + 1) < file_list.length) {
            base_pos += (vitashell_config.view_mode == 1) ? GRID_COLS : 1;
          }
        }
    }

    // Infinite scroll: wrap to first item when at bottom (single press only)
    if (old_pos == base_pos + rel_pos && file_list.length > 0 && accel == 1) {
      base_pos = 0;
      rel_pos = 0;
    }

    if (old_pos != base_pos + rel_pos) {
      scroll_count = 0;
      target_scroll_y = (vitashell_config.view_mode != 1) ? base_pos * FONT_Y_SPACE : (base_pos / GRID_COLS) * GRID_CELL_H;
      if (target_scroll_y < 0) target_scroll_y = 0;
    }
  }

  // Page skip
  if (hold_pad[PAD_LTRIGGER] || hold_pad[PAD_RTRIGGER]) {
    int old_pos = base_pos + rel_pos;

    int limit = (vitashell_config.view_mode != 1) ? MAX_ENTRIES : 30;
    if (hold_pad[PAD_LTRIGGER]) { // Skip page up
      base_pos = base_pos - limit;
      if (base_pos < 0) {
        base_pos = 0;
        rel_pos = 0;
      }
    } else { // Skip page down
      base_pos = base_pos + limit;
      if (base_pos >= file_list.length - limit) {
        base_pos = MAX(file_list.length - limit, 0);
        rel_pos = MIN(limit - 1, file_list.length - 1);
      }
    }

    if (old_pos != base_pos + rel_pos) {
      scroll_count = 0;
      target_scroll_y = (vitashell_config.view_mode != 1) ? base_pos * FONT_Y_SPACE : (base_pos / GRID_COLS) * GRID_CELL_H;
      if (target_scroll_y < 0) target_scroll_y = 0;
    }
  }

  // Context menu trigger
  if (pressed_pad[PAD_TRIANGLE]) {
    if (getContextMenuMode() == CONTEXT_MENU_CLOSED) {
      if (dir_level > 0) {
        setContextMenu(&context_menu_main);
        setContextMenuMainVisibilities();
        setContextMenuMode(CONTEXT_MENU_OPENING);
      } else {
        setContextMenu(&context_menu_home);
        setContextMenuHomeVisibilities();
        setContextMenuMode(CONTEXT_MENU_OPENING);
      }
    }
  }

  // Not at 'home'
  if (dir_level > 0) {
    // Mark entry    
    if (pressed_pad[PAD_SQUARE]) {
      FileListEntry *file_entry = fileListGetNthEntry(&file_list, base_pos + rel_pos);
      if (file_entry && strcmp(file_entry->name, DIR_UP) != 0) {
        if (!fileListFindEntry(&mark_list, file_entry->name)) {
          fileListAddEntry(&mark_list, fileListCopyEntry(file_entry), SORT_NONE);
        } else {
          fileListRemoveEntryByName(&mark_list, file_entry->name);
        }
      }
    }

    // Back
    if (pressed_pad[PAD_CANCEL]) {
      scroll_count = 0;
      fileListEmpty(&mark_list);
      dirUp();
      WriteFile(VITASHELL_LASTDIR, file_list.path, strlen(file_list.path) + 1);
      refreshFileList();
    }
  }

  // Handle
  if (pressed_pad[PAD_ENTER]) {
    scroll_count = 0;

    fileListEmpty(&mark_list);

    // Handle file, symlink or folder
    FileListEntry *file_entry = fileListGetNthEntry(&file_list, base_pos + rel_pos);
    if (file_entry) {
      if (file_entry->is_symlink) {
        fileBrowserHandleSymlink(file_entry);
      } else if (file_entry->is_folder) {
        fileBrowserHandleFolder(file_entry);
      } else {
        fileBrowserHandleFile(file_entry);
        create_recent_symlink(file_entry);
      }
    }
  }

  // Snap scroll immediately for button navigation (no lerp lag)
  scroll_y = target_scroll_y;
  return refresh;
}

static void create_recent_symlink(FileListEntry *file_entry) {
  if (isInArchive()) return;

  char target[MAX_PATH_LENGTH];
  snprintf(target, MAX_PATH_LENGTH, "%s%s."SYMLINK_EXT, VITASHELL_RECENT_PATH,
               file_entry->name);
  snprintf(cur_file, MAX_PATH_LENGTH, "%s%s", file_list.path, file_entry->name);

  // create file recent symlink
  createSymLink(target, cur_file);
}

static void fileBrowserHandleFile(FileListEntry *file_entry) {
  snprintf(cur_file, MAX_PATH_LENGTH, "%s%s", file_list.path, file_entry->name);
  int type = handleFile(cur_file, file_entry);

  // Archive mode
  if (type == FILE_TYPE_ARCHIVE && getDialogStep() != DIALOG_STEP_ENTER_PASSWORD) {
    is_in_archive = 1;
    dir_level_archive = dir_level;

    snprintf(archive_path, MAX_PATH_LENGTH, "%s%s", file_list.path, file_entry->name);

    strcat(file_list.path, file_entry->name);
    addEndSlash(file_list.path);

    dirLevelUp();
    refreshFileList();
  }
}

static void fileBrowserHandleFolder(FileListEntry *file_entry) {
  if (strcmp(file_entry->name, DIR_UP) == 0) {
    dirUp();
  } else {
    if (dir_level == 0) {
      strcpy(file_list.path, file_entry->name);
    } else {
      if (dir_level > 1)
        addEndSlash(file_list.path);
      strcat(file_list.path, file_entry->name);
    }
    dirLevelUp();
  }

  // Save last dir
  WriteFile(VITASHELL_LASTDIR, file_list.path, strlen(file_list.path) + 1);

  // Open folder
  int res = refreshFileList();
  if (res < 0)
    errorDialog(res);
}

// escape from dir level structure so that parent directory is browsed
// where this jump came from and not the hierarchically higher folder
int jump_to_directory_track_current_path(char *path) {
  SymlinkDirectoryPath *symlink_path = malloc(sizeof(SymlinkDirectoryPath));

  int archive = 0;
  while(isInArchive()) {
    archive = 1;
    dirUp();
  }
  if (archive) {
    dirUpCloseArchive();
  }

  if (symlink_path) {
    snprintf(symlink_path->last_path, MAX_PATH_LENGTH, "%s", file_list.path);
    snprintf(symlink_path->last_hook, MAX_PATH_LENGTH, "%s", path);
    dirLevelUp();
    int _dir_level = dir_level; // we escape from hierarchical dir level structure
    if (change_to_directory(path) < 0) {
      free(symlink_path);
      return VITASHELL_ERROR_NAVIGATION;
    }
    WriteFile(VITASHELL_LASTDIR, file_list.path, strlen(file_list.path) + 1);
    storeSymlinkPath(symlink_path);
    dir_level = _dir_level;
    refreshFileList();
  }
  return 0;
}

static void fileBrowserHandleSymlink(FileListEntry *file_entry) {
  if ((file_entry->symlink->to_file == 1 && !checkFileExist(file_entry->symlink->target_path))
      || (file_entry->symlink->to_file == 0 && !checkFolderExist(file_entry->symlink->target_path))) {
    // TODO: What if in archive?
    snprintf(cur_file, MAX_PATH_LENGTH, "%s%s", file_list.path, file_entry->name);
    textViewer(cur_file);
    return;
  }
  if (file_entry->symlink->to_file == 0) {
    if (jump_to_directory_track_current_path(file_entry->symlink->target_path) < 0) {
      errorDialog(VITASHELL_ERROR_SYMLINK_INVALID_PATH);
    }
  } else {
    char *target_base_directory = getBaseDirectory(file_entry->symlink->target_path);
    if (!target_base_directory) {
      errorDialog(VITASHELL_ERROR_SYMLINK_CANT_RESOLVE_BASEDIR);
      return;
    }
    char *target_file_name = getFilename(file_entry->symlink->target_path);
    if (!target_file_name) {
      errorDialog(VITASHELL_ERROR_SYMLINK_CANT_RESOLVE_FILENAME);
      return;
    }
    if (jump_to_directory_track_current_path(target_base_directory) < 0) {
      errorDialog(VITASHELL_ERROR_SYMLINK_INVALID_PATH);
      return;
    }
    FileListEntry *resolved_file_entry = fileListFindEntry(&file_list, target_file_name);
    if (!resolved_file_entry) {
      errorDialog(VITASHELL_ERROR_SYMLINK_INVALID_PATH);
      return;
    }
    fileBrowserHandleFile(resolved_file_entry);
    dirUp();

    free(target_file_name);
    free(target_base_directory);
  }
  int res = refreshFileList();
  if (res < 0)
    errorDialog(res);
}

int browserMain() {
  // Position
  memset(base_pos_list, 0, sizeof(base_pos_list));
  memset(rel_pos_list, 0, sizeof(rel_pos_list));

  // Paths
  memset(cur_file, 0, sizeof(cur_file));
  memset(archive_path, 0, sizeof(archive_path));

  // File lists
  memset(&file_list, 0, sizeof(FileList));
  memset(&mark_list, 0, sizeof(FileList));
  memset(&copy_list, 0, sizeof(FileList));
  memset(&install_list, 0, sizeof(FileList));

  // Current path is 'home'
  strcpy(file_list.path, HOME_PATH);

  if (use_custom_config) {
    // Last dir
    char lastdir[MAX_PATH_LENGTH];
    memset(lastdir, 0, sizeof(lastdir));
    if (ReadFile(VITASHELL_LASTDIR, lastdir, sizeof(lastdir) - 1) > 0) {
      change_to_directory(lastdir);
    }
  }

  // Refresh file list
  refreshFileList();

  // Init settings menu
  initSettingsMenu();

  // Load mouse cursor texture
  mouse_tex = vita2d_load_PNG_buffer(&_binary_resources_default_mouse_png_start);

  while (1) {
    readPad();

    int refresh = REFRESH_MODE_NONE;

    // ----- RIGHT ANALOG → MOUSE CURSOR -----
    if (mouse_tex) {
      int dx = (int)pad.rx - ANALOG_CENTER;
      int dy = (int)pad.ry - ANALOG_CENTER;
      if (abs(dx) > ANALOG_THRESHOLD || abs(dy) > ANALOG_THRESHOLD) {
        mouse_x += (dx > 0 ? 1 : -1) * (abs(dx) - ANALOG_THRESHOLD) / 6.0f;
        mouse_y += (dy > 0 ? 1 : -1) * (abs(dy) - ANALOG_THRESHOLD) / 6.0f;
        if (mouse_x < 0) mouse_x = 0;
        if (mouse_x >= SCREEN_WIDTH) mouse_x = SCREEN_WIDTH - 1;
        if (mouse_y < 0) mouse_y = 0;
        if (mouse_y >= SCREEN_HEIGHT) mouse_y = SCREEN_HEIGHT - 1;
        mouse_inactive = 0;
        mouse_visible = 1;
      }
      if (mouse_visible) {
        mouse_inactive++;
        if (mouse_inactive > 180) {
          mouse_visible = 0;
        }
      }
      // Mouse click via CROSS when visible
      if (mouse_visible && pressed_pad[PAD_ENTER]) {
        mouse_visible = 0;
        mouse_inactive = 0;
        // Determine what's at cursor position
        int mx = (int)mouse_x, my = (int)mouse_y;
        if (my <= 92) {
          // Toolbar
          if (my >= 5 && my < 45) {
            // Address card — open context menu
          } else if (my >= 50 && my < 90) {
            if (mx >= 8 && mx < 106) {
              if (dir_level > 0) contextMenuMainEnterCallback(MENU_MAIN_ENTRY_MOVE, NULL);
            } else if (mx >= 106 && mx < 204) {
              if (dir_level > 0) contextMenuMainEnterCallback(MENU_MAIN_ENTRY_COPY, NULL);
            } else if (mx >= 204 && mx < 302) {
              if (dir_level > 0) contextMenuMainEnterCallback(MENU_MAIN_ENTRY_PASTE, NULL);
            } else if (mx >= 302 && mx < 400) {
              if (dir_level > 0) contextMenuMainEnterCallback(MENU_MAIN_ENTRY_DELETE, NULL);
            } else if (mx >= 400 && mx < 498) {
              if (dir_level > 0) contextMenuMainEnterCallback(MENU_MAIN_ENTRY_RENAME, NULL);
            } else if (mx >= 498 && mx < 596) {
              filter_mode = (filter_mode + 1) % 3;
              refreshFileList();
            } else if (mx >= 596 && mx < 694) {
              sort_mode = (sort_mode % 2) + 1;
              last_set_sort_mode = sort_mode;
              refreshFileList();
            } else if (mx >= 694 && mx < 792) {
              if (search_active) {
                search_active = 0;
                search_term[0] = '\0';
                refreshFileList();
              } else {
                initImeDialog(language_container[SEARCH], "", 255, SCE_IME_TYPE_DEFAULT, 0, 0);
                setDialogStep(DIALOG_STEP_SEARCH);
              }
            } else if (mx >= 792 && mx < 890) {
              if (dir_level > 0) {
                setContextMenu(&context_menu_new);
                setContextMenuNewVisibilities();
                setContextMenuMode(CONTEXT_MENU_OPENING);
              }
            }
          }
          pressed_pad[PAD_ENTER] = 0;
        } else {
          // Floating buttons (bottom-right)
          int btn_size = 48, btn_gap = 12;
          int btn_rx = SCREEN_WIDTH - 20 - btn_size;
          int btn_plus_y = SCREEN_HEIGHT - 20 - btn_size;
          int btn_bmk_y = btn_plus_y - btn_size - btn_gap;
          if (mx >= btn_rx && mx < btn_rx + btn_size && my >= btn_plus_y && my < btn_plus_y + btn_size) {
            if (dir_level > 0) {
              setContextMenu(&context_menu_new);
              setContextMenuNewVisibilities();
              setContextMenuMode(CONTEXT_MENU_OPENING);
            }
            pressed_pad[PAD_ENTER] = 0;
          } else if (mx >= btn_rx && mx < btn_rx + btn_size && my >= btn_bmk_y && my < btn_bmk_y + btn_size) {
            SceInt64 now = sceKernelGetSystemTimeWide();
            if (now - time_last_bookmarks > THRESHOLD_LAST_PAD_BOOKMARKS_WAIT) {
              if (strncmp(file_list.path, VITASHELL_BOOKMARKS_PATH, MAX_PATH_LENGTH) != 0) {
                char path[MAX_PATH_LENGTH] = VITASHELL_BOOKMARKS_PATH;
                sort_mode = last_set_sort_mode;
                jump_to_directory_track_current_path(path);
                time_last_bookmarks = now;
              }
            }
            pressed_pad[PAD_ENTER] = 0;
          } else {
            // File list — select item under cursor, let ENTER handle opening
            int ci;
            if (vitashell_config.view_mode != 1) {
              ci = (int)((my + scroll_y - START_Y) / FONT_Y_SPACE);
            } else {
              ci = ((int)((my + scroll_y - START_Y) / GRID_CELL_H)) * GRID_COLS + ((mx - GRID_START_X) / (GRID_CELL_W + GRID_GAP));
            }
            if (ci >= 0 && ci < file_list.length) {
              int limit = (vitashell_config.view_mode != 1) ? 9 : 16;
              if (ci >= base_pos && ci < base_pos + limit) {
                rel_pos = ci - base_pos;
              } else {
                if (ci < base_pos) {
                  base_pos = ci;
                  rel_pos = 0;
                } else {
                  base_pos = ci - limit + 1;
                  rel_pos = limit - 1;
                }
              }
              target_scroll_y = (vitashell_config.view_mode != 1) ? base_pos * FONT_Y_SPACE : (base_pos / GRID_COLS) * GRID_CELL_H;
              if (target_scroll_y < 0) target_scroll_y = 0;
              scroll_y = target_scroll_y;
            } else {
              pressed_pad[PAD_ENTER] = 0;
            }
          }
        }
      }
    }

    // ----- ADVANCED TOUCH ENGINE -----
    // Reset touch state on context switch (dialog step or context menu change)
    if (getDialogStep() != last_dialog_step || getContextMenuMode() != last_ctx_menu_mode) {
        is_touching = 0;
        is_dragging = 0;
        drag_file_active = 0;
        touch_handled = 0;
        last_dialog_step = getDialogStep();
        last_ctx_menu_mode = getContextMenuMode();
    }

    if (getDialogStep() == DIALOG_STEP_NONE) {
      if (getContextMenuMode() != CONTEXT_MENU_CLOSED) {
        // Context menu aberto: captura toque para selecionar itens
        if (touch.reportNum > 0) {
          if (!is_touching) {
            is_touching = 1;
            touch_x_start = (touch.report[0].x * 960) / 1920;
            touch_y_start = (touch.report[0].y * 544) / 1088;
          }
          touch_x_last = (touch.report[0].x * 960) / 1920;
          touch_y_last = (touch.report[0].y * 544) / 1088;
        } else {
          if (is_touching) {
            contextMenuTouch(touch_x_last, touch_y_last);
            is_touching = 0;
          }
        }
      } else if (touch.reportNum > 0 || touch_back.reportNum > 0) {
          int ty = 0, tx = 0;
          if (touch.reportNum > 0) {
              ty = (touch.report[0].y * 544) / 1088;
              tx = (touch.report[0].x * 960) / 1920;
          } else {
              ty = (touch_back.report[0].y * 544) / 1088;
              tx = (touch_back.report[0].x * 960) / 1920;
          }
           if (!is_touching) {
               is_touching = 1;
               touch_y_start = ty;
               touch_x_start = tx;
               scroll_y_start = scroll_y;
               touch_frames = 0;
                is_dragging = 0;

                // Immediate toolbar press handling — no frame delay needed
                if (!touch_handled && ty <= 92) {
                    touch_handled = 1;
                    if (ty >= 5 && ty < 45) {
                        // Address card → context menu (reset touch state)
                        if (dir_level != 0) { setContextMenu(&context_menu_main); setContextMenuMainVisibilities(); }
                        else { setContextMenu(&context_menu_home); setContextMenuHomeVisibilities(); }
                        setContextMenuMode(CONTEXT_MENU_OPENING);
                        is_touching = 0;
                        goto skip_touch_processing;
                    } else if (ty >= 50 && ty < 90) {
                        // Quick action buttons — execute immediately, keep touch tracking
                        if (tx >= 8 && tx < 106) { if (dir_level > 0) contextMenuMainEnterCallback(MENU_MAIN_ENTRY_MOVE, NULL); }
                        else if (tx >= 106 && tx < 204) { if (dir_level > 0) contextMenuMainEnterCallback(MENU_MAIN_ENTRY_COPY, NULL); }
                        else if (tx >= 204 && tx < 302) { if (dir_level > 0) contextMenuMainEnterCallback(MENU_MAIN_ENTRY_PASTE, NULL); }
                        else if (tx >= 302 && tx < 400) { if (dir_level > 0) contextMenuMainEnterCallback(MENU_MAIN_ENTRY_DELETE, NULL); }
                        else if (tx >= 400 && tx < 498) { if (dir_level > 0) contextMenuMainEnterCallback(MENU_MAIN_ENTRY_RENAME, NULL); }
                        else if (tx >= 498 && tx < 596) { filter_mode = (filter_mode + 1) % 3; refreshFileList(); }
                        else if (tx >= 596 && tx < 694) { sort_mode = (sort_mode % 2) + 1; last_set_sort_mode = sort_mode; refreshFileList(); }
                        else if (tx >= 694 && tx < 792) {
                            if (search_active) { search_active = 0; search_term[0] = '\0'; refreshFileList(); }
                            else { initImeDialog(language_container[SEARCH], "", 255, SCE_IME_TYPE_DEFAULT, 0, 0); setDialogStep(DIALOG_STEP_SEARCH); }
                        } else if (tx >= 792 && tx < 890) {
                            if (dir_level > 0) { setContextMenu(&context_menu_new); setContextMenuNewVisibilities(); setContextMenuMode(CONTEXT_MENU_OPENING); }
                            is_touching = 0;
                            goto skip_touch_processing;
                        }
                    }
                }
                // Floating action buttons (bottom-right) — immediate on press
                { int btn_size = 48, btn_gap = 12;
                  int btn_rx = SCREEN_WIDTH - 20 - btn_size;
                  int btn_plus_y = SCREEN_HEIGHT - 20 - btn_size;
                  int btn_bmk_y = btn_plus_y - btn_size - btn_gap;
                  if (!touch_handled && tx >= btn_rx && tx < btn_rx + btn_size && ty >= btn_plus_y && ty < btn_plus_y + btn_size) {
                      touch_handled = 1;
                      if (dir_level > 0) { setContextMenu(&context_menu_new); setContextMenuNewVisibilities(); setContextMenuMode(CONTEXT_MENU_OPENING); }
                      is_touching = 0; goto skip_touch_processing;
                  } else if (!touch_handled && tx >= btn_rx && tx < btn_rx + btn_size && ty >= btn_bmk_y && ty < btn_bmk_y + btn_size) {
                      touch_handled = 1;
                      SceInt64 now = sceKernelGetSystemTimeWide();
                      if (now - time_last_bookmarks > THRESHOLD_LAST_PAD_BOOKMARKS_WAIT) {
                          if (strncmp(file_list.path, VITASHELL_BOOKMARKS_PATH, MAX_PATH_LENGTH) != 0) {
                              char path[MAX_PATH_LENGTH] = VITASHELL_BOOKMARKS_PATH;
                              sort_mode = last_set_sort_mode;
                              jump_to_directory_track_current_path(path);
                              time_last_bookmarks = now;
                          }
                      }
                      is_touching = 0; goto skip_touch_processing;
                  }
                }
                
                // Square + touch on file → drag mode
                drag_file_active = 0;
               if (current_pad[PAD_SQUARE] && ty > 92) {
                   int idx;
                   if (vitashell_config.view_mode != 1) {
                        idx = (ty + scroll_y < START_Y) ? -1 : (int)((ty + scroll_y - START_Y) / FONT_Y_SPACE);
                   } else {
                       idx = ((int)((ty + scroll_y - START_Y) / GRID_CELL_H)) * GRID_COLS + ((tx - GRID_START_X) / (GRID_CELL_W + GRID_GAP));
                   }
                   if (idx >= 0 && idx < file_list.length) {
                       FileListEntry *entry = fileListGetNthEntry(&file_list, idx);
                       if (entry && strcmp(entry->name, DIR_UP) != 0) {
                           drag_file_active = 1;
                           strncpy(drag_file_name, entry->name, MAX_NAME_LENGTH - 1);
                           drag_file_name[MAX_NAME_LENGTH - 1] = '\0';
                           snprintf(drag_src_path, MAX_PATH_LENGTH, "%s%s", file_list.path, entry->name);
                           is_dragging = 1;
                       }
                   }
               }
          } else {
              touch_frames++;
              float diff_x = tx - touch_x_start;
              float diff_y = ty - touch_y_start;
              
                 if (!is_dragging && touch_frames > 60 && abs((int)diff_x) < 20 && abs((int)diff_y) < 20) {
                    int start_i = 0;
                    if (vitashell_config.view_mode != 1) start_i = (ty + scroll_y < START_Y) ? -1 : (int)((ty + scroll_y - START_Y) / FONT_Y_SPACE);
                    else start_i = ((int)((ty + scroll_y - START_Y) / GRID_CELL_H)) * GRID_COLS + ((tx - GRID_START_X) / (GRID_CELL_W + GRID_GAP));
                    
                    if (start_i >= 0 && start_i < file_list.length) {
                        uint64_t now = sceKernelGetProcessTimeWide();
                        if (now - last_tap_time < 400000) {
                            // Double tap + hold → context menu
                             int limit = (vitashell_config.view_mode != 1) ? 9 : 16;
                             if (start_i >= base_pos && start_i < base_pos + limit) {
                                 rel_pos = start_i - base_pos;
                             } else {
                                 if (start_i < base_pos) {
                                     base_pos = start_i;
                                     rel_pos = 0;
                                 } else {
                                     base_pos = start_i - limit + 1;
                                     rel_pos = limit - 1;
                                 }
                             }
                             target_scroll_y = (vitashell_config.view_mode != 1) ? base_pos * FONT_Y_SPACE : (base_pos / GRID_COLS) * GRID_CELL_H;
                             if (target_scroll_y < 0) target_scroll_y = 0;
                             scroll_y = target_scroll_y;
                            if (dir_level > 0) {
                                setContextMenu(&context_menu_main);
                                setContextMenuMainVisibilities();
                            } else {
                                setContextMenu(&context_menu_home);
                                setContextMenuHomeVisibilities();
                            }
                            setContextMenuMode(CONTEXT_MENU_OPENING);
                            is_touching = 0;
                        } else {
                            // Single tap + hold → drag mode (no Square needed)
                            FileListEntry *entry = fileListGetNthEntry(&file_list, start_i);
                            if (entry && strcmp(entry->name, DIR_UP) != 0) {
                                drag_file_active = 1;
                                strncpy(drag_file_name, entry->name, MAX_NAME_LENGTH - 1);
                                drag_file_name[MAX_NAME_LENGTH - 1] = '\0';
                                snprintf(drag_src_path, MAX_PATH_LENGTH, "%s%s", file_list.path, entry->name);
                                is_dragging = 1;
                            }
                        }
                    }
                }
               if (!is_dragging) {
                   target_scroll_y = scroll_y_start - diff_y;
                   scroll_y = target_scroll_y;
               }
               // Auto-scroll during drag: near top/bottom edges
               if (drag_file_active) {
            if (ty < 80) {
                       target_scroll_y -= 2.0f + (80 - ty) * 0.2f;
                   } else if (ty > SCREEN_HEIGHT - 80) {
                       target_scroll_y += 2.0f + (ty - (SCREEN_HEIGHT - 80)) * 0.2f;
                   }
                }
            }
skip_touch_processing:
            touch_x_last = tx;
            touch_y_last = ty;
        } else {
           if (is_touching) {
                 // Drag completion: release touch → move file if in different folder
                if (drag_file_active) {
                   char target_dir[MAX_PATH_LENGTH];
                   snprintf(target_dir, MAX_PATH_LENGTH, "%s", file_list.path);
                   
                   int drop_index = -1;
                   if (vitashell_config.view_mode != 1) drop_index = (touch_y_last + scroll_y < START_Y) ? -1 : (int)((touch_y_last + scroll_y - START_Y) / FONT_Y_SPACE);
                   else drop_index = (touch_y_last + scroll_y < START_Y) ? -1 : ((int)((touch_y_last + scroll_y - START_Y) / GRID_CELL_H)) * GRID_COLS + ((touch_x_last - GRID_START_X) / (GRID_CELL_W + GRID_GAP));
                   
                   if (drop_index >= 0 && drop_index < file_list.length) {
                       FileListEntry *drop_entry = fileListGetNthEntry(&file_list, drop_index);
                       if (drop_entry && drop_entry->is_folder) {
                           if (strcmp(drop_entry->name, DIR_UP) == 0) {
                               // Go up one directory for drop
                               char *slash = strrchr(target_dir, '/');
                               if (slash) { *slash = '\0'; slash = strrchr(target_dir, '/'); if (slash) slash[1] = '\0'; }
                           } else {
                               snprintf(target_dir, MAX_PATH_LENGTH, "%s%s/", file_list.path, drop_entry->name);
                           }
                       }
                   }

                    char drag_dir[MAX_PATH_LENGTH];
                    snprintf(drag_dir, MAX_PATH_LENGTH, "%s", drag_src_path);
                    char *p = strrchr(drag_dir, '/');
                    if (p) p[1] = '\0';
                    if (strcmp(target_dir, drag_dir) != 0) {
                        char new_path[MAX_PATH_LENGTH];
                        snprintf(new_path, MAX_PATH_LENGTH, "%s%s", target_dir, drag_file_name);
                        // Store for undo
                        snprintf(undo_src, MAX_PATH_LENGTH, "%s", drag_src_path);
                        snprintf(undo_dst, MAX_PATH_LENGTH, "%s", new_path);
                        undo_type = UNDO_MOVE;
                        undo_available = 1;
                        setTouchConfirm("Mover arquivo?", dragConfirmYes, dragConfirmNo);
                    }
                     drag_file_active = 0;
                     is_touching = 0;
                     touch_handled = 0;
                } else {
                float diff_x = touch_x_last - touch_x_start;
               float diff_y = touch_y_last - touch_y_start;
               
               if (abs((int)diff_x) > 150 && abs((int)diff_y) < 50) {
                  if (diff_x > 150) {
                      pressed_pad[PAD_CANCEL] = 1;
                  } else if (diff_x < -150) {
                      pressed_pad[PAD_ENTER] = 1;
                  }
                } else if (abs((int)diff_x) < 35 && abs((int)diff_y) < 35) {
                    if (touch_y_last <= 92 && !touch_handled) {
                        int tx = touch_x_last, ty = touch_y_last;
                        // Address card row (y:5-45)
                        if (ty >= 5 && ty < 45) {
                            if (dir_level != 0) { setContextMenu(&context_menu_main); setContextMenuMainVisibilities(); }
                            else { setContextMenu(&context_menu_home); setContextMenuHomeVisibilities(); }
                            setContextMenuMode(CONTEXT_MENU_OPENING);
                          // Quick actions row (y:50-90)
                          } else if (ty >= 50 && ty < 90) {
                               if (tx >= 8 && tx < 106) {          // Mover
                                   if (dir_level > 0) contextMenuMainEnterCallback(MENU_MAIN_ENTRY_MOVE, NULL);
                               } else if (tx >= 106 && tx < 204) { // Copiar
                                   if (dir_level > 0) contextMenuMainEnterCallback(MENU_MAIN_ENTRY_COPY, NULL);
                               } else if (tx >= 204 && tx < 302) { // Colar
                                   if (dir_level > 0) contextMenuMainEnterCallback(MENU_MAIN_ENTRY_PASTE, NULL);
                               } else if (tx >= 302 && tx < 400) { // Apagar
                                   if (dir_level > 0) contextMenuMainEnterCallback(MENU_MAIN_ENTRY_DELETE, NULL);
                               } else if (tx >= 400 && tx < 498) { // Renomear
                                   if (dir_level > 0) contextMenuMainEnterCallback(MENU_MAIN_ENTRY_RENAME, NULL);
                               } else if (tx >= 498 && tx < 596) { // Filtrar
                                   filter_mode = (filter_mode + 1) % 3;
                                   refreshFileList();
                                } else if (tx >= 596 && tx < 694) { // Agrupar
                                    sort_mode = (sort_mode % 2) + 1;
                                    last_set_sort_mode = sort_mode;
                                    refreshFileList();
                                } else if (tx >= 694 && tx < 792) { // Buscar
                                    if (search_active) {
                                        search_active = 0;
                                        search_term[0] = '\0';
                                        refreshFileList();
                                    } else {
                                         initImeDialog(language_container[SEARCH], "", 255, SCE_IME_TYPE_DEFAULT, 0, 0);
                                         setDialogStep(DIALOG_STEP_SEARCH);
                                     }
                                } else if (tx >= 792 && tx < 890) { // Novo
                                    if (dir_level > 0) {
                                        setContextMenu(&context_menu_new);
                                        setContextMenuNewVisibilities();
                                        setContextMenuMode(CONTEXT_MENU_OPENING);
                                    }
                                }
                           }
                      } else if (touch_y_last > 92) {
                         // Check floating action buttons (bottom-right) before file list
                         int btn_size = 48, btn_gap = 12;
                         int btn_rx = SCREEN_WIDTH - 20 - btn_size;
                         int btn_plus_y = SCREEN_HEIGHT - 20 - btn_size;
                         int btn_bmk_y = btn_plus_y - btn_size - btn_gap;
                         int tx = touch_x_last, ty = touch_y_last;
 
                         if (tx >= btn_rx && tx < btn_rx + btn_size && ty >= btn_plus_y && ty < btn_plus_y + btn_size) {
                             if (dir_level > 0) {
                                 setContextMenu(&context_menu_new);
                                 setContextMenuNewVisibilities();
                                 setContextMenuMode(CONTEXT_MENU_OPENING);
                             }
                         } else if (tx >= btn_rx && tx < btn_rx + btn_size && ty >= btn_bmk_y && ty < btn_bmk_y + btn_size) {
                             SceInt64 now = sceKernelGetSystemTimeWide();
                             if (now - time_last_bookmarks > THRESHOLD_LAST_PAD_BOOKMARKS_WAIT) {
                                 if (strncmp(file_list.path, VITASHELL_BOOKMARKS_PATH, MAX_PATH_LENGTH) != 0) {
                                     char path[MAX_PATH_LENGTH] = VITASHELL_BOOKMARKS_PATH;
                                     sort_mode = last_set_sort_mode;
                                     jump_to_directory_track_current_path(path);
                                     time_last_bookmarks = now;
                                 }
                             }
                         } else {
                            int clicked_index = 0;
                            
                            if (vitashell_config.view_mode == 2 && dir_level > 0 && touch_y_last > START_Y) {
                                int num_columns = 1;
                                if (parent_list.length > 0) num_columns = 2;
                                if (grandparent_list.length > 0) num_columns = 3;
                                float c_x = SHELL_MARGIN_X;
                                if (num_columns == 2) c_x = SHELL_MARGIN_X + 300.0f;
                                else if (num_columns == 3) c_x = SHELL_MARGIN_X + 600.0f;
                                if (touch_x_last < c_x) {
                                    if (num_columns >= 3 && touch_x_last < c_x - 300.0f) {
                                        if (dir_level > 1) { dirUp(); dirUp(); }
                                        else { dirUp(); }
                                    } else {
                                        dirUp();
                                    }
                                    clicked_index = -1;
                                    refreshFileList();
                                }
                            }

                            if (clicked_index >= 0) {
                                if (vitashell_config.view_mode != 1) {
                                    clicked_index = (touch_y_last + scroll_y < START_Y) ? -1 : (int)((touch_y_last + scroll_y - START_Y) / FONT_Y_SPACE);
                                } else {
                                    clicked_index = (touch_y_last + scroll_y < START_Y) ? -1 : ((int)((touch_y_last + scroll_y - START_Y) / GRID_CELL_H)) * GRID_COLS + ((touch_x_last - GRID_START_X) / (GRID_CELL_W + GRID_GAP));
                                }
                            }

                            if (clicked_index >= 0 && clicked_index < file_list.length) {
                                if (current_pad[PAD_LTRIGGER]) {
                                    FileListEntry *file_entry = fileListGetNthEntry(&file_list, clicked_index);
                                    if (file_entry && strcmp(file_entry->name, DIR_UP) != 0) {
                                        if (!fileListFindEntry(&mark_list, file_entry->name)) {
                                            fileListAddEntry(&mark_list, fileListCopyEntry(file_entry), SORT_NONE);
                                        } else {
                                            fileListRemoveEntryByName(&mark_list, file_entry->name);
                                        }
                                    }
                                    pressed_pad[PAD_ENTER] = 0;
                                } else {
                                    int selected_index = base_pos + rel_pos;
                                    if (clicked_index == selected_index) {
                                        scroll_count = 0;
                                        fileListEmpty(&mark_list);
                                        FileListEntry *file_entry = fileListGetNthEntry(&file_list, clicked_index);
                                        if (file_entry) {
                                            if (file_entry->is_symlink) fileBrowserHandleSymlink(file_entry);
                                            else if (file_entry->is_folder) fileBrowserHandleFolder(file_entry);
                                            else { fileBrowserHandleFile(file_entry); create_recent_symlink(file_entry); }
                                        }
                                    } else {
                                        int limit = (vitashell_config.view_mode != 1) ? 9 : 30;
                                        if (clicked_index >= base_pos && clicked_index < base_pos + limit) {
                                            rel_pos = clicked_index - base_pos;
                                        } else {
                                            if (clicked_index < base_pos) {
                                                base_pos = clicked_index;
                                                rel_pos = 0;
                                            } else {
                                                base_pos = clicked_index - limit + 1;
                                                rel_pos = limit - 1;
                                            }
                                        }
                                        target_scroll_y = (vitashell_config.view_mode != 1) ? base_pos * FONT_Y_SPACE : (base_pos / GRID_COLS) * GRID_CELL_H;
                                        if (target_scroll_y < 0) target_scroll_y = 0;
                                        scroll_y = target_scroll_y;
                                    }
                                }
                            }
                        }
                    }
                }
                is_touching = 0;
                touch_handled = 0;
           }
         }
       }
      } else if (getDialogStep() == DIALOG_STEP_TOUCH_CONFIRM) {
      // Generic touch confirm: Sim/Não buttons
      if (touch.reportNum > 0) {
        if (!is_touching) {
          is_touching = 1;
          touch_x_start = (touch.report[0].x * 960) / 1920;
          touch_y_start = (touch.report[0].y * 544) / 1088;
        }
        touch_x_last = (touch.report[0].x * 960) / 1920;
        touch_y_last = (touch.report[0].y * 544) / 1088;
      } else if (is_touching) {
        is_touching = 0;
        int tx = touch_x_last, ty = touch_y_last;
        int cw = 540, ch = 180;
        int cx = (SCREEN_WIDTH - cw) / 2, cy = (SCREEN_HEIGHT - ch) / 2;
        int sim_x = cx + 50, sim_y = cy + 105, sim_w = 190, sim_h = 52;
        if (tx >= sim_x && tx < sim_x + sim_w && ty >= sim_y && ty < sim_y + sim_h) {
          if (touch_confirm_yes_cb) {
            touch_confirm_yes_cb();
          }
          touch_confirm_yes_cb = NULL;
          touch_confirm_no_cb = NULL;
          setDialogStep(DIALOG_STEP_NONE);
        }
        int nao_x = cx + cw - 50 - 190, nao_y = cy + 105, nao_w = 190, nao_h = 52;
        if (tx >= nao_x && tx < nao_x + nao_w && ty >= nao_y && ty < nao_y + nao_h) {
          if (touch_confirm_no_cb) {
            touch_confirm_no_cb();
          }
          touch_confirm_yes_cb = NULL;
          touch_confirm_no_cb = NULL;
          setDialogStep(DIALOG_STEP_NONE);
        }
      }
    } else if (getDialogStep() == DIALOG_STEP_FTP_TOUCH) {
      // FTP touch dialog: OK and Stop buttons
      if (touch.reportNum > 0) {
        if (!is_touching) {
          is_touching = 1;
          touch_x_start = (touch.report[0].x * 960) / 1920;
          touch_y_start = (touch.report[0].y * 544) / 1088;
        }
        touch_x_last = (touch.report[0].x * 960) / 1920;
        touch_y_last = (touch.report[0].y * 544) / 1088;
      } else if (is_touching) {
        is_touching = 0;
        int tx = touch_x_last, ty = touch_y_last;
        int cw = 520, ch = 240;
        int cx = (SCREEN_WIDTH - cw) / 2, cy = (SCREEN_HEIGHT - ch) / 2;
        // OK button
        int ok_x = cx + 60, ok_y = cy + 170, ok_w = 180, ok_h = 44;
        if (tx >= ok_x && tx < ok_x + ok_w && ty >= ok_y && ty < ok_y + ok_h) {
          setDialogStep(DIALOG_STEP_NONE);
        }
        // Parar FTP button
        int stop_x = cx + cw - 60 - 180, stop_y = cy + 170, stop_w = 180, stop_h = 44;
        if (tx >= stop_x && tx < stop_x + stop_w && ty >= stop_y && ty < stop_y + stop_h) {
          powerUnlock();
          ftpvita_fini();
          setDialogStep(DIALOG_STEP_NONE);
        }
      }
    } else if (getDialogStep() == DIALOG_STEP_DELETE_CONFIRM_TOUCH) {
      // Delete confirm touch: Sim/Não buttons
      if (touch.reportNum > 0) {
        if (!is_touching) {
          is_touching = 1;
          touch_x_start = (touch.report[0].x * 960) / 1920;
          touch_y_start = (touch.report[0].y * 544) / 1088;
        }
        touch_x_last = (touch.report[0].x * 960) / 1920;
        touch_y_last = (touch.report[0].y * 544) / 1088;
      } else if (is_touching) {
        is_touching = 0;
        int tx = touch_x_last, ty = touch_y_last;
        int cw = 540, ch = 180;
        int cx = (SCREEN_WIDTH - cw) / 2, cy = (SCREEN_HEIGHT - ch) / 2;
        int sim_x = cx + 50, sim_y = cy + 105, sim_w = 190, sim_h = 52;
        if (tx >= sim_x && tx < sim_x + sim_w && ty >= sim_y && ty < sim_y + sim_h) {
          initMessageDialog(MESSAGE_DIALOG_PROGRESS_BAR, language_container[DELETING]);
          setDialogStep(DIALOG_STEP_DELETE_CONFIRMED);
        }
        int nao_x = cx + cw - 50 - 190, nao_y = cy + 105, nao_w = 190, nao_h = 52;
        if (tx >= nao_x && tx < nao_x + nao_w && ty >= nao_y && ty < nao_y + nao_h) {
          setDialogStep(DIALOG_STEP_NONE);
        }
      }
    }
    // ----- END TOUCH ENGINE -----

    // Control
    if (getDialogStep() != DIALOG_STEP_NONE) {
      refresh = dialogSteps();
      // scroll_count = 0;
    } else if (getAdhocDialogStatus() != ADHOC_DIALOG_CLOSED) {
      adhocDialogCtrl();
    } else if (getPropertyDialogStatus() != PROPERTY_DIALOG_CLOSED) {
      propertyDialogCtrl();
      scroll_count = 0;
    } else if (getSettingsMenuStatus() != SETTINGS_MENU_CLOSED) {
      settingsMenuCtrl();
    } else if (getContextMenuMode() != CONTEXT_MENU_CLOSED) {
      contextMenuCtrl();
    } else {
      refresh = fileBrowserMenuCtrl();
    }

    // Receive system event
    SceAppMgrSystemEvent event;
    sceAppMgrReceiveSystemEvent(&event);

    // Refresh on app resume
    if (event.systemEvent == SCE_APPMGR_SYSTEMEVENT_ON_RESUME) {
      sceShellUtilLock(SCE_SHELL_UTIL_LOCK_TYPE_USB_CONNECTION);
      pfsUmount(); // umount game data at resume
      refresh = REFRESH_MODE_NORMAL;
    }
    if (refresh != REFRESH_MODE_NONE) {
      // Refresh lists
      refreshFileList();
      refreshMarkList();
      refreshCopyList();

      // Focus
      if (refresh == REFRESH_MODE_SETFOCUS)
        setFocusOnFilename(focus_name);
    }


    if (target_scroll_y < 0) target_scroll_y = 0;
    float max_scroll = 0;
     if (vitashell_config.view_mode != 1) max_scroll = (file_list.length * 
FONT_Y_SPACE) - (MAX_ENTRIES * FONT_Y_SPACE);
    else max_scroll = (((file_list.length + GRID_COLS - 1) / GRID_COLS) * GRID_CELL_H) - (5 * GRID_CELL_H);
    
    if (max_scroll < 0) max_scroll = 0;
    if (target_scroll_y > max_scroll && (!is_touching || !is_dragging)) target_scroll_y = max_scroll;

    scroll_y += (target_scroll_y - scroll_y) * 0.15f;
    if (scroll_y < 0) scroll_y = 0;

    // Start drawing
    startDrawing(NULL);

    // Background / GIF / Animation
    if (vitashell_config.background_anim >= 4) {
      drawGifBackground();
    } else {
      drawGifBackground();
      wave_time += 0.018f;
      if (vitashell_config.background_anim == 0) {
        // Partículas: PlayStation button symbols
        const char *btn_sym[] = { TRIANGLE, CIRCLE, CROSS, SQUARE };
        const unsigned int btn_base[] = {
          RGBA8(80, 255, 120, 0), RGBA8(255, 100, 100, 0),
          RGBA8(100, 150, 255, 0), RGBA8(210, 120, 255, 0)
        };
        for (int i = 0; i < 25; i++) {
          float speed = 0.4f + (i % 6) * 0.15f;
          float base_x = (i * 77 + 23) % SCREEN_WIDTH;
          float base_y = (i * 91 + 47) % SCREEN_HEIGHT;
          float sz = 14.0f + (i % 7) * 2.5f;
          int si = i % 4;
          float x = base_x + sinf(wave_time * speed + i * 1.7f) * 35.0f;
          float y = base_y - (wave_time * 55.0f * speed);
          while (y < -sz) y += SCREEN_HEIGHT + sz * 2;
          int alpha = 30 + (int)((sinf(wave_time * 1.8f + i * 2.3f) + 1.0f) * 25.0f);
          vita2d_pgf_draw_text(font, x, y + sz, btn_base[si] | (alpha << 24), sz / 13.0f, btn_sym[si]);
        }
      } else if (vitashell_config.background_anim == 1) {
        // PS Vita/PSP Style Flowing Waves of Light (Nebula/Ribbons)
        // Wave 1 (Indigo/Blue) - refined with smoother motion
        for (int x = 0; x < SCREEN_WIDTH + 20; x += 10) {
          float y = 240.0f + sinf(x * 0.005f + wave_time * 0.6f) * 55.0f + cosf(x * 0.003f - wave_time * 0.35f) * 25.0f;
          int alpha = 14 + (int)((sinf(wave_time * 0.8f + x * 0.002f) + 1.0f) * 8.0f);
          vita2d_draw_fill_circle(x, y, 20.0f, RGBA8(40, 120, 255, alpha));
        }
        // Wave 2 (Purple/Violet) - richer
        for (int x = 0; x < SCREEN_WIDTH + 20; x += 10) {
          float y = 290.0f + sinf(x * 0.006f - wave_time * 0.7f + 1.5f) * 45.0f + cosf(x * 0.004f + wave_time * 0.45f) * 20.0f;
          int alpha = 12 + (int)((cosf(wave_time * 0.7f - x * 0.003f) + 1.0f) * 7.0f);
          vita2d_draw_fill_circle(x, y, 24.0f, RGBA8(200, 80, 255, alpha));
        }
        // Wave 3 (Cyan/Green-Blue) - smoother flow
        for (int x = 0; x < SCREEN_WIDTH + 20; x += 10) {
          float y = 180.0f + sinf(x * 0.004f + wave_time * 0.45f - 1.0f) * 65.0f + cosf(x * 0.002f + wave_time * 0.25f) * 18.0f;
          int alpha = 10 + (int)((sinf(wave_time * 0.6f + x * 0.004f) + 1.0f) * 6.0f);
          vita2d_draw_fill_circle(x, y, 16.0f, RGBA8(60, 230, 255, alpha));
        }
        // Sparkle highlights on wave peaks
        for (int s = 0; s < 8; s++) {
          float sx = (float)((s * 137 + 53) % SCREEN_WIDTH);
          float sy = 240.0f + sinf(sx * 0.005f + wave_time * 0.6f + s * 2.1f) * 55.0f;
          int sa = (int)((sinf(wave_time * 1.5f + s * 3.7f) + 1.0f) * 35.0f);
          vita2d_draw_fill_circle(sx, sy, 4.0f, RGBA8(255, 255, 255, sa));
        }
      } else if (vitashell_config.background_anim == 2) {
        // Stars: Particle Starfield with Parallax
        for (int i = 0; i < 60; i++) {
          float speed = 0.15f + (i % 8) * 0.06f;
          float base_x = (i * 41 + 17) % SCREEN_WIDTH;
          float base_y = (i * 53 + 31) % SCREEN_HEIGHT;
          float twinkle = sinf(wave_time * (1.5f + speed * 2.0f) + i * 4.1f) * 0.5f + 0.5f;
          float x = base_x + sinf(wave_time * speed * 0.3f + i * 1.3f) * 12.0f;
          float y = base_y - (wave_time * 30.0f * speed);
          while (y < -6.0f) y += SCREEN_HEIGHT + 12.0f;
          int alpha = 20 + (int)(twinkle * 80.0f);
          float sz = 2.0f + twinkle * 3.0f;
          unsigned int star_color = RGBA8(180 + (int)(75 * twinkle), 200 + (int)(55 * twinkle), 255, alpha);
          vita2d_draw_fill_circle(x, y, sz, star_color);
        }
      } else if (vitashell_config.background_anim == 3) {
        // Quadrados animation (from photo viewer)
        for (int i = 0; i < 30; i++) {
          float speed = 0.5f + (i % 5) * 0.2f;
          float base_x = (i * 67) % SCREEN_WIDTH;
          float base_y = (i * 89) % SCREEN_HEIGHT;
          float size = 15.0f + (i % 10) * 3.0f;
          float x = base_x + sinf(wave_time * speed + i) * 30.0f;
          float y = base_y - (wave_time * 60.0f * speed);
          while (y < -size) y += SCREEN_HEIGHT + size * 2;
          int alpha = 10 + (int)((sinf(wave_time * 2.0f + i) + 1.0f) * 20.0f);
          vita2d_draw_rectangle(x, y, size, size, RGBA8(100, 180, 255, alpha));
          vita2d_draw_rectangle(x + 2, y + 2, size - 4, size - 4, RGBA8(150, 220, 255, alpha / 2));
        }
      }
    }

    drawShellInfo(file_list.path);

    // Clip file list area: below top bar (96px) to above status bar
    vita2d_set_clip_rectangle(0, 96, SCREEN_WIDTH, SCREEN_HEIGHT - 96 - STATUSBAR_H);

    int start_i = 0;
    if (vitashell_config.view_mode != 1) start_i = (int)(scroll_y / FONT_Y_SPACE);
    else start_i = (int)(scroll_y / GRID_CELL_H) * GRID_COLS;
    
    if (start_i < 0) start_i = 0;

    // Calculate mouse hover index for highlight
    int mouse_hover_index = -1;
    if (mouse_visible) {
      if (vitashell_config.view_mode != 1) {
        mouse_hover_index = (int)((mouse_y + scroll_y - START_Y) / FONT_Y_SPACE);
      } else {
        mouse_hover_index = ((int)((mouse_y + scroll_y - START_Y) / GRID_CELL_H)) * GRID_COLS + ((int)((mouse_x - GRID_START_X) / (GRID_CELL_W + GRID_GAP)));
      }
    }

    if (vitashell_config.view_mode == 2) {
      if (grandparent_list.length > 0) {
        FileListEntry *gp_entry = grandparent_list.head;
        int p_i = 0; float p_y = START_Y;
        
        char curr_parent[256];
        snprintf(curr_parent, 256, "%s", file_list.path);
        int l = strlen(curr_parent);
        if (l > 0 && curr_parent[l-1] == '/') curr_parent[l-1] = '\0';
        char *s = strrchr(curr_parent, '/');
        if (s) *s = '\0';
        s = strrchr(curr_parent, '/');
        if (!s) s = strchr(curr_parent, ':');
        if (s) snprintf(curr_parent, 256, "%s", s + 1);

        int gp_target = 0;
        FileListEntry *temp = gp_entry;
        while (temp) {
            if (strcmp(temp->name, curr_parent) == 0) break;
            gp_target++;
            temp = temp->next;
        }
        int gp_start = (gp_target > 5) ? gp_target - 5 : 0;
        for (int skip = 0; skip < gp_start && gp_entry; skip++) gp_entry = gp_entry->next;
        
        while (gp_entry && p_i < 13) {
            uint32_t p_color = COLOR_ALPHA(themeTextColor(vitashell_config.theme_preset), 80);
            vita2d_texture *p_icon = file_icon;
            if (gp_entry->is_folder) p_icon = folder_icon;
            else if (gp_entry->is_symlink) p_icon = folder_symlink_icon;
            
            char curr_folder[256];
            snprintf(curr_folder, 256, "%s", file_list.path);
            int len = strlen(curr_folder);
            if (len > 0 && curr_folder[len-1] == '/') curr_folder[len-1] = '\0';
            char *slash = strrchr(curr_folder, '/');
            if (slash) *slash = '\0';
            slash = strrchr(curr_folder, '/');
            if (!slash) slash = strchr(curr_folder, ':');
            if (slash) snprintf(curr_folder, 256, "%s", slash + 1);

            if (strcmp(gp_entry->name, curr_folder) == 0) {
                vita2d_draw_rectangle(SHELL_MARGIN_X - 10, p_y + 1.0f, 290.0f, FONT_Y_SPACE - 2.0f, RGBA8(255, 200, 50, 30));
                vita2d_draw_rectangle(SHELL_MARGIN_X - 10, p_y + 1.0f, 4, FONT_Y_SPACE - 2.0f, RGBA8(255, 200, 50, 180));
                p_color = RGBA8(255, 220, 100, 220);
            }
            if (p_icon) vita2d_draw_texture(p_icon, SHELL_MARGIN_X, p_y + 10.0f);
            vita2d_enable_clipping();
            float clip_gp_h = (p_y + FONT_Y_SPACE > SCREEN_HEIGHT - 60) ? (SCREEN_HEIGHT - 60) : (p_y + FONT_Y_SPACE);
            vita2d_set_clip_rectangle(SHELL_MARGIN_X + 28.0f, p_y, SHELL_MARGIN_X + 280.0f, clip_gp_h);
            pgf_draw_text(SHELL_MARGIN_X + 28.0f, p_y + 4.0f, p_color, gp_entry->name);
            vita2d_disable_clipping();
            p_y += FONT_Y_SPACE; gp_entry = gp_entry->next; p_i++;
        }
      }
      
      if (parent_list.length > 0) {
        FileListEntry *p_entry = parent_list.head;
        int p_i = 0; float p_y = START_Y;
        
        char curr_folder[256];
        snprintf(curr_folder, 256, "%s", file_list.path);
        int len = strlen(curr_folder);
        if (len > 0 && curr_folder[len-1] == '/') curr_folder[len-1] = '\0';
        char *slash = strrchr(curr_folder, '/');
        if (!slash) slash = strchr(curr_folder, ':');
        if (slash) snprintf(curr_folder, 256, "%s", slash + 1);
        
        int p_target = 0;
        FileListEntry *temp2 = p_entry;
        while (temp2) {
            if (strcmp(temp2->name, curr_folder) == 0) break;
            p_target++;
            temp2 = temp2->next;
        }
        int p_start = (p_target > 5) ? p_target - 5 : 0;
        for (int skip = 0; skip < p_start && p_entry; skip++) p_entry = p_entry->next;
        
        while (p_entry && p_i < 13) {
            uint32_t p_color = COLOR_ALPHA(themeTextColor(vitashell_config.theme_preset), 120);
            vita2d_texture *p_icon = file_icon;
            if (p_entry->is_folder) p_icon = folder_icon;
            else if (p_entry->is_symlink) p_icon = folder_symlink_icon;
            
            char curr_folder[256];
            snprintf(curr_folder, 256, "%s", file_list.path);
            int len = strlen(curr_folder);
            if (len > 0 && curr_folder[len-1] == '/') curr_folder[len-1] = '\0';
            char *slash = strrchr(curr_folder, '/');
            if (!slash) slash = strchr(curr_folder, ':');
            if (slash) snprintf(curr_folder, 256, "%s", slash + 1);
            int num_columns = 1;
            if (parent_list.length > 0) num_columns = 2;
            if (grandparent_list.length > 0) num_columns = 3;
            
            float ox = (num_columns == 3) ? (SHELL_MARGIN_X + 300.0f) : SHELL_MARGIN_X;
            if (strcmp(p_entry->name, curr_folder) == 0) {
                vita2d_draw_rectangle(ox - 10, p_y + 1.0f, 290.0f, FONT_Y_SPACE - 2.0f, RGBA8(255, 200, 50, 40));
                vita2d_draw_rectangle(ox - 10, p_y + 1.0f, 4, FONT_Y_SPACE - 2.0f, RGBA8(255, 200, 50, 200));
                p_color = RGBA8(255, 220, 100, 240);
            }
            if (p_icon) vita2d_draw_texture(p_icon, ox, p_y + 10.0f);
            vita2d_enable_clipping();
            float clip_p_h = (p_y + FONT_Y_SPACE > SCREEN_HEIGHT - 60) ? (SCREEN_HEIGHT - 60) : (p_y + FONT_Y_SPACE);
            vita2d_set_clip_rectangle(ox + 28.0f, p_y, ox + 280.0f, clip_p_h);
            pgf_draw_text(ox + 28.0f, p_y + 4.0f, p_color, p_entry->name);
            vita2d_disable_clipping();
            p_y += FONT_Y_SPACE; p_entry = p_entry->next; p_i++;
        }
      }
    }

    FileListEntry *file_entry = fileListGetNthEntry(&file_list, start_i);
    if (file_entry) {
      int i;
      int max_i = 0;
      if (vitashell_config.view_mode != 1) max_i = start_i + MAX_ENTRIES + 2;
      else max_i = start_i + (GRID_COLS * 6) + GRID_COLS;
      
      for (i = start_i; i < max_i && i < file_list.length; i++) {
        if (!file_entry) break; // CRITICAL SAFETY CHECK
        // Modern palette
        uint32_t txt_color = themeTextColor(vitashell_config.theme_preset);

        uint32_t color = txt_color;
        
        int num_columns = 1;
        if (parent_list.length > 0) num_columns = 2;
        if (grandparent_list.length > 0) num_columns = 3;

        float c_x = SHELL_MARGIN_X;
        if (num_columns == 2) c_x = SHELL_MARGIN_X + 300.0f;
        else if (num_columns == 3) c_x = SHELL_MARGIN_X + 600.0f;

        float cur_margin_x = (vitashell_config.view_mode == 2) ? c_x : SHELL_MARGIN_X;
        float list_width = (vitashell_config.view_mode == 2) ? 290.0f : (SCREEN_WIDTH - (SHELL_MARGIN_X * 2) + 18.0f);
        
        float x = (vitashell_config.view_mode == 2) ? c_x + 28.0f : FILE_X;
        float y = START_Y + (i * FONT_Y_SPACE) - scroll_y;
        
        if (vitashell_config.view_mode == 1) { // Grid View
           int row = i / GRID_COLS;
           int col = i % GRID_COLS;
           x = GRID_START_X + col * (GRID_CELL_W + GRID_GAP);
           y = START_Y + (row * GRID_CELL_H) - scroll_y + 8;
        }

        // Subtle row background (alternating)
        if (vitashell_config.view_mode != 1 && (i % 2) == 0) {
          unsigned int list_bg = (vitashell_config.background_anim >= 4) ? COLOR_ALPHA(themeListBg(vitashell_config.theme_preset), 100) : themeListBg(vitashell_config.theme_preset);
          vita2d_draw_rectangle(cur_margin_x - 10, y, list_width, FONT_Y_SPACE, list_bg);
        }

        vita2d_texture *icon = NULL;
        if (file_entry->is_symlink) {
          if (file_entry->symlink->to_file) {
            color = RGBA8(60, 180, 240, 255);
            icon = file_symlink_icon;
          } else {
            color = RGBA8(60, 200, 120, 255);
            icon = folder_symlink_icon;
          }
        }
        else if (file_entry->is_folder) {
          color = themeFolderColor(vitashell_config.theme_preset);
          icon = folder_icon;
        } else {
          switch (file_entry->type) {
            case FILE_TYPE_BMP:
            case FILE_TYPE_PNG:
            case FILE_TYPE_JPEG:
              color = RGBA8(60, 210, 120, 255);
              icon = image_icon;
              break;
              
            case FILE_TYPE_VPK:
            case FILE_TYPE_ARCHIVE:
              color = RGBA8(240, 190, 50, 255);
              icon = archive_icon;
              break;
              
            case FILE_TYPE_MP3:
            case FILE_TYPE_OGG:
              color = RGBA8(240, 120, 180, 255);
              icon = audio_icon;
              break;
              
            case FILE_TYPE_SFO:
              color = RGBA8(160, 120, 240, 255);
              icon = sfo_icon;
              break;
            
            case FILE_TYPE_INI:
            case FILE_TYPE_TXT:
            case FILE_TYPE_XML:
              color = RGBA8(160, 170, 190, 255);
              icon = text_icon;
              break;
              
            default:
              color = txt_color;
              icon = file_icon;
              break;
          }
        }

        // Selection Bar (accent blue)
        int is_selected = (i == base_pos + rel_pos);
        int is_hovered = (mouse_visible && i == mouse_hover_index && i >= 0 && i < file_list.length);
        if (is_selected || is_hovered) {
          unsigned int bar_color = is_hovered ? RGBA8(50, 120, 200, 60) : themeSelectionBg(vitashell_config.theme_preset);
          unsigned int edge_color = is_hovered ? RGBA8(80, 180, 255, 160) : themeSelectionLine(vitashell_config.theme_preset);
          if (is_selected) color = themeTextColor(vitashell_config.theme_preset);
          if (vitashell_config.view_mode != 1) {
              vita2d_draw_rectangle(cur_margin_x - 10, y + 1.0f, list_width + 2.0f, FONT_Y_SPACE - 2.0f, bar_color);
              vita2d_draw_rectangle(cur_margin_x - 10, y + 1.0f, 3, FONT_Y_SPACE - 2.0f, edge_color);
          } else {
              vita2d_draw_rectangle(x - 4, y, GRID_CELL_W, GRID_CELL_H, bar_color);
              vita2d_draw_rectangle(x - 4, y, 3, GRID_CELL_H, edge_color);
          }
        }

        // Draw icon
        if (icon) {
          if (vitashell_config.view_mode != 1) {
            vita2d_draw_texture(icon, cur_margin_x, y + 10.0f);
          } else {
            float icon_x = x + (GRID_CELL_W - GRID_ICON_SIZE) / 2.0f;
            float icon_y = y + 6;
            float s = (float)GRID_ICON_SIZE / (float)vita2d_texture_get_width(icon);
            vita2d_draw_texture_scale(icon, icon_x, icon_y, s, s);
          }
        }

        // Marked
        if (fileListFindEntry(&mark_list, file_entry->name)) {
          if (vitashell_config.view_mode != 1) {
            // Fill row with semi-transparent premium accent color
            vita2d_draw_rectangle(cur_margin_x - 10, y, list_width, FONT_Y_SPACE, (MARKED_COLOR & 0x00FFFFFF) | 0x1A000000);
            // Glowing vertical left-side highlight bar
            vita2d_draw_rectangle(cur_margin_x - 10, y, 4, FONT_Y_SPACE, (MARKED_COLOR & 0x00FFFFFF) | 0xE0000000);
          } else {
            // Fill cell with translucid background
            vita2d_draw_rectangle(x - 4, y, GRID_CELL_W, GRID_CELL_H, (MARKED_COLOR & 0x00FFFFFF) | 0x22000000);
            // Glowing full border outline around the cell
            float gx = x - 4;
            float gy = y;
            float gw = GRID_CELL_W;
            float gh = GRID_CELL_H;
            unsigned int border_color = (MARKED_COLOR & 0x00FFFFFF) | 0xC0000000;
            vita2d_draw_rectangle(gx, gy, gw, 2, border_color); // Top
            vita2d_draw_rectangle(gx, gy + gh - 2, gw, 2, border_color); // Bottom
            vita2d_draw_rectangle(gx, gy, 2, gh, border_color); // Left
            vita2d_draw_rectangle(gx + gw - 2, gy, 2, gh, border_color); // Right
          }
        }

        // Draw file name
        if (vitashell_config.view_mode != 1) {
          vita2d_enable_clipping();
          float max_w = (vitashell_config.view_mode == 2) ? 800.0f : MAX_NAME_WIDTH;
          float clip_y = (y < START_Y) ? START_Y : y;
           float clip_h = (y + FONT_Y_SPACE > SCREEN_HEIGHT - 10) ? (SCREEN_HEIGHT - 10) : (y + FONT_Y_SPACE);
          vita2d_set_clip_rectangle(x + 1.0f, clip_y, x + 1.0f + max_w, clip_h);
        }

        char file_name[MAX_PATH_LENGTH];
        memset(file_name, 0, sizeof(file_name));

        if (file_entry->is_symlink) {
          snprintf(file_name, MAX_PATH_LENGTH, "%s  -> %s",
                   file_entry->name, file_entry->symlink->target_path);
        } else {
          strncpy(file_name, file_entry->name, file_entry->name_length + 1);
          file_name[file_entry->name_length] = '\0';
        }
        
        float draw_x = x;
        float draw_y = (vitashell_config.view_mode != 1) ? y + 8.0f : y;
        if (i == base_pos + rel_pos && vitashell_config.view_mode != 1) {
          int width = (int)pgf_text_width(file_name);
          float max_w = (vitashell_config.view_mode == 2) ? 800.0f : MAX_NAME_WIDTH;
          if (width >= max_w) {
            if (scroll_count < 60) {
              scroll_x = draw_x;
            } else if (scroll_count < width + 90) {
              scroll_x--;
            } else if (scroll_count < width + 120) {
              color = (color & 0x00FFFFFF) | ((((color >> 24) * (scroll_count - width - 90)) / 30) << 24);
              scroll_x = draw_x;
            } else {
              scroll_count = 0;
            }
            scroll_count++;
            draw_x = scroll_x;
          }
        } else if (vitashell_config.view_mode == 1) {
            // Center name below icon in grid
            draw_x = x + (GRID_CELL_W - pgf_text_width(file_name)) / 2.0f;
            draw_y = y + GRID_ICON_SIZE + 10;
            if (strlen(file_name) > 16) {
                strcpy(file_name + 13, "...");
            }
        }

        pgf_draw_text(draw_x + 1.0f, draw_y + 1.0f, COLOR_ALPHA(themeTextColor(vitashell_config.theme_preset), 140), file_name);
        pgf_draw_text(draw_x, draw_y, color, file_name);

        if (vitashell_config.view_mode != 1) vita2d_disable_clipping();

        // File information
        if (strcmp(file_entry->name, DIR_UP) != 0 && vitashell_config.view_mode == 0) {
          float draw_y_info = y + 8.0f;
          if (dir_level == 0) {
            char used_size_string[16], max_size_string[16];
            int max_size_x = ALIGN_RIGHT(INFORMATION_X, pgf_text_width("0000.00 MB"));
            int separator_x = ALIGN_RIGHT(max_size_x, pgf_text_width("  /  "));
            if (file_entry->size != 0 && file_entry->size2 != 0) {
              getSizeString(used_size_string, file_entry->size2 - file_entry->size);
              getSizeString(max_size_string, file_entry->size2);
            } else {
              strcpy(used_size_string, "-");
              strcpy(max_size_string, "-");
            }
            
            float x = ALIGN_RIGHT(INFORMATION_X, pgf_text_width(max_size_string));
            pgf_draw_text(x, draw_y_info, color, max_size_string);
            pgf_draw_text(separator_x, draw_y_info, color, "  /");
            x = ALIGN_RIGHT(separator_x, pgf_text_width(used_size_string));
            pgf_draw_text(x, draw_y_info, color, used_size_string);
          } else {
            char *str = NULL;
            if (!file_entry->is_folder) {
              // Folder/size
              char string[16];
              getSizeString(string, file_entry->size);
              str = string;
            } else {
              str = language_container[FOLDER];
            }
            pgf_draw_text(ALIGN_RIGHT(INFORMATION_X, pgf_text_width(str)), draw_y_info, color, str);
          }

          // Date
          char date_string[24];
          getDateString(date_string, date_format, &file_entry->mtime);

          char time_string[24];
          getTimeString(time_string, time_format, &file_entry->mtime);

          char string[64];
          sprintf(string, "%s %s", date_string, time_string);

          float x = ALIGN_RIGHT(SCREEN_WIDTH - SHELL_MARGIN_X, pgf_text_width(string));
          pgf_draw_text(x, draw_y_info, color, string);
        }

        // Next
        file_entry = file_entry->next;
      }
    }

    // Disable clip so floating buttons and dialogs render everywhere
    vita2d_disable_clipping();

    drawStatusBar();

    // Floating action buttons (bottom-right)
    if (getDialogStep() == DIALOG_STEP_NONE && getContextMenuMode() == CONTEXT_MENU_CLOSED &&
        getSettingsMenuStatus() == SETTINGS_MENU_CLOSED) {
      int btn_size = 48, btn_gap = 12;
      int btn_rx = SCREEN_WIDTH - 20 - btn_size;
      int btn_plus_y = SCREEN_HEIGHT - STATUSBAR_H - 12 - btn_size;
      int btn_bmk_y = btn_plus_y - btn_size - btn_gap;
      // Floating + button
      vita2d_draw_rectangle(btn_rx, btn_plus_y, btn_size, btn_size, RGBA8(50, 140, 220, 220));
      vita2d_draw_rectangle(btn_rx, btn_plus_y, btn_size, 2, RGBA8(255, 255, 255, 30));
      pgf_draw_text(btn_rx + (btn_size - pgf_text_width("+")) / 2.0f, btn_plus_y + 13, RGBA8(255, 255, 255, 255), "+");
      // Floating bookmark button
      vita2d_draw_rectangle(btn_rx, btn_bmk_y, btn_size, btn_size, RGBA8(60, 65, 80, 220));
      vita2d_draw_rectangle(btn_rx, btn_bmk_y, btn_size, 2, RGBA8(255, 255, 255, 20));
      pgf_draw_text(btn_rx + (btn_size - pgf_text_width("*")) / 2.0f, btn_bmk_y + 13, RGBA8(200, 205, 220, 255), "*");
    }

    // Draw
    drawSettingsMenu();
    drawContextMenu();
    drawDeleteConfirmDialog();
    drawTouchConfirmDialog();
    drawFtpTouchDialog();
    drawAdhocDialog();
    drawPropertyDialog();

    // Drag indicator (Square + touch) — floating label at touch position
    if (drag_file_active) {
      float dx = (float)touch_x_last, dy = (float)touch_y_last;
      vita2d_draw_rectangle(dx - 40, dy - 14, 80, 24, RGBA8(255, 200, 50, 200));
      pgf_draw_text(dx - pgf_text_width(drag_file_name) / 2.0f, dy - 8, RGBA8(0, 0, 0, 255), drag_file_name);
    }

    // Mouse cursor (drawn last, on top of everything) — 30% maior
    if (mouse_visible && mouse_tex) {
      float ms = 1.3f;
      vita2d_draw_texture_scale(mouse_tex, mouse_x, mouse_y, ms, ms);
    }

    // End drawing
    endDrawing();
  }

  // Empty lists
  fileListEmpty(&copy_list);
  fileListEmpty(&mark_list);
  fileListEmpty(&file_list);

  return 0;
}
