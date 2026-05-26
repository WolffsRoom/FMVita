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

#include "network_download.h"
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
#include "ime_dialog.h"
#include "theme.h"
#include "language.h"
#include "utils.h"
#include "sfo.h"
#include "coredump.h"
#include "usb.h"
#include "qr.h"
#include "pfs.h"
#include "buttons.h"

int _newlib_heap_size_user = 128 * 1024 * 1024;

static volatile int dialog_step = DIALOG_STEP_NONE;

static char install_path[MAX_PATH_LENGTH];
static char compress_name[MAX_NAME_LENGTH];

static SceUID usbdevice_modid = -1;

static SceKernelLwMutexWork dialog_mutex;

char vita_ip[16];
unsigned short int vita_port;

char ftp_status_msg[256] = "";

VitaShellConfig vitashell_config;

int SCE_CTRL_ENTER = SCE_CTRL_CROSS, SCE_CTRL_CANCEL = SCE_CTRL_CIRCLE;

int use_custom_config = 1;

char delete_confirm_message[512] = "";
int delete_confirm_is_folder = 0;

char touch_confirm_message[MAX_CONFIRM_MSG] = "";
void (*touch_confirm_yes_cb)(void) = NULL;
void (*touch_confirm_no_cb)(void) = NULL;

void setTouchConfirm(const char *msg, void (*yes_cb)(void), void (*no_cb)(void)) {
  strncpy(touch_confirm_message, msg, MAX_CONFIRM_MSG - 1);
  touch_confirm_message[MAX_CONFIRM_MSG - 1] = '\0';
  touch_confirm_yes_cb = yes_cb;
  touch_confirm_no_cb = no_cb;
  setDialogStep(DIALOG_STEP_TOUCH_CONFIRM);
}

static void draw_centered_text(float x, float y, unsigned int color, const char *text) {
  if (text) pgf_draw_text(x - pgf_text_width(text) / 2.0f, y, color, text);
}

void drawTouchConfirmDialog() {
  if (getDialogStep() != DIALOG_STEP_TOUCH_CONFIRM)
    return;

  vita2d_common_dialog_update();

  vita2d_draw_rectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, RGBA8(0, 0, 0, 190));

  int cw = 540, ch = 180;
  int cx = (SCREEN_WIDTH - cw) / 2, cy = (SCREEN_HEIGHT - ch) / 2;
  vita2d_draw_rectangle(cx, cy, cw, ch, themeDialogBg(vitashell_config.theme_preset));
  vita2d_draw_rectangle(cx, cy, cw, 1, COLOR_ALPHA(themeTopbarText(vitashell_config.theme_preset), 25));
  vita2d_draw_rectangle(cx, cy + ch - 1, cw, 1, COLOR_ALPHA(themeTopbarText(vitashell_config.theme_preset), 15));

  char msg_display[512];
  strncpy(msg_display, touch_confirm_message, 80);
  msg_display[80] = '\0';
  draw_centered_text(SCREEN_WIDTH / 2.0f, cy + 52, themeTextColor(vitashell_config.theme_preset), msg_display);

  int enter_btn = (enter_button == SCE_SYSTEM_PARAM_ENTER_BUTTON_CIRCLE) ? BUTTON_CIRCLE : BUTTON_CROSS;
  int cancel_btn = (enter_button == SCE_SYSTEM_PARAM_ENTER_BUTTON_CIRCLE) ? BUTTON_CROSS : BUTTON_CIRCLE;

  int sim_x = cx + 50, sim_y = cy + 105, sim_w = 190, sim_h = 52;
  vita2d_draw_rectangle(sim_x, sim_y, sim_w, sim_h, themeButtonSuccess(vitashell_config.theme_preset));
  vita2d_draw_rectangle(sim_x, sim_y, sim_w, 2, COLOR_ALPHA(themeTopbarText(vitashell_config.theme_preset), 60));
  drawButton(enter_btn, sim_x + 20, sim_y + 16);
  draw_centered_text(sim_x + sim_w / 2.0f + 15, sim_y + 16, themeTopbarText(vitashell_config.theme_preset), language_container[CONFIRM_YES_BTN]);

  int nao_x = cx + cw - 50 - 190, nao_y = cy + 105, nao_w = 190, nao_h = 52;
  vita2d_draw_rectangle(nao_x, nao_y, nao_w, nao_h, themeButtonDanger(vitashell_config.theme_preset));
  vita2d_draw_rectangle(nao_x, nao_y, nao_w, 2, COLOR_ALPHA(themeTopbarText(vitashell_config.theme_preset), 50));
  drawButton(cancel_btn, nao_x + 20, nao_y + 16);
  draw_centered_text(nao_x + nao_w / 2.0f + 15, nao_y + 16, themeTopbarText(vitashell_config.theme_preset), language_container[CONFIRM_NO_BTN]);
}

void touchInfoOk(void) {
  setDialogStep(DIALOG_STEP_NONE);
}

void drawFtpTouchDialog() {
  if (getDialogStep() != DIALOG_STEP_FTP_TOUCH)
    return;

  vita2d_common_dialog_update();

  vita2d_draw_rectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, RGBA8(0, 0, 0, 180));

  int cw = 520, ch = 240;
  int cx = (SCREEN_WIDTH - cw) / 2, cy = (SCREEN_HEIGHT - ch) / 2;
  vita2d_draw_rectangle(cx, cy, cw, ch, themeDialogBg(vitashell_config.theme_preset));
  vita2d_draw_rectangle(cx, cy, cw, 1, COLOR_ALPHA(themeTopbarText(vitashell_config.theme_preset), 18));
  vita2d_draw_rectangle(cx, cy + ch - 1, cw, 1, COLOR_ALPHA(themeTopbarText(vitashell_config.theme_preset), 12));

  draw_centered_text(SCREEN_WIDTH / 2.0f, cy + 20, themeAccentColor(vitashell_config.theme_preset), language_container[FTP_SERVER_TITLE]);

  vita2d_draw_rectangle(cx + 40, cy + 45, cw - 80, 1, COLOR_ALPHA(themeTopbarText(vitashell_config.theme_preset), 30));

  char ip_text[64];
  snprintf(ip_text, 64, "IP: %s", vita_ip);
  if (vita_port != 0) {
    char port_str[16];
    snprintf(port_str, 16, ":%d", vita_port);
    strcat(ip_text, port_str);
  }
  draw_centered_text(SCREEN_WIDTH / 2.0f, cy + 65, themeTextColor(vitashell_config.theme_preset), ip_text);

  draw_centered_text(SCREEN_WIDTH / 2.0f, cy + 95, themeTextDim(vitashell_config.theme_preset), language_container[FTP_CONNECT_LABEL]);

  if (ftp_status_msg[0] != '\0') {
    char status_buf[128];
    strncpy(status_buf, ftp_status_msg, sizeof(status_buf) - 1);
    status_buf[sizeof(status_buf) - 1] = '\0';
    int slen = strlen(status_buf);
    if (slen > 90) {
      memmove(status_buf, status_buf + slen - 90, 90);
      status_buf[90] = '\0';
    }
    draw_centered_text(SCREEN_WIDTH / 2.0f, cy + 120, themeAccentColor(vitashell_config.theme_preset), status_buf);
  }

  int ok_x = cx + 60, ok_y = cy + 170, ok_w = 180, ok_h = 44;
  vita2d_draw_rectangle(ok_x, ok_y, ok_w, ok_h, themeButtonAccent(vitashell_config.theme_preset));
  vita2d_draw_rectangle(ok_x, ok_y, ok_w, 2, COLOR_ALPHA(themeTopbarText(vitashell_config.theme_preset), 30));
  draw_centered_text(ok_x + ok_w / 2.0f, ok_y + 12, themeTopbarText(vitashell_config.theme_preset), language_container[OK]);

  int stop_x = cx + cw - 60 - 180, stop_y = cy + 170, stop_w = 180, stop_h = 44;
  vita2d_draw_rectangle(stop_x, stop_y, stop_w, stop_h, themeButtonDanger(vitashell_config.theme_preset));
  vita2d_draw_rectangle(stop_x, stop_y, stop_w, 2, COLOR_ALPHA(themeTopbarText(vitashell_config.theme_preset), 30));
  draw_centered_text(stop_x + stop_w / 2.0f, stop_y + 12, themeTopbarText(vitashell_config.theme_preset), language_container[FTP_STOP_LABEL]);
}

int getDialogStep() {
  sceKernelLockLwMutex(&dialog_mutex, 1, NULL);
  volatile int step = dialog_step;
  sceKernelUnlockLwMutex(&dialog_mutex, 1);
  return step;
}

void setDialogStep(int step) {
  sceKernelLockLwMutex(&dialog_mutex, 1, NULL);
  dialog_step = step;
  sceKernelUnlockLwMutex(&dialog_mutex, 1);
}

void drawStatusBar() {
  unsigned int sb_bg = COLOR_ALPHA(themeCardBg(vitashell_config.theme_preset), 200);
  unsigned int sb_col = themeTextDim(vitashell_config.theme_preset);
  unsigned int sb_acc = themeAccentColor(vitashell_config.theme_preset);

  int y = SCREEN_HEIGHT - STATUSBAR_H;
  vita2d_draw_rectangle(0, y, SCREEN_WIDTH, STATUSBAR_H, sb_bg);
  vita2d_draw_rectangle(0, y, SCREEN_WIDTH, 1, COLOR_ALPHA(themeTopbarText(vitashell_config.theme_preset), 16));

  float ty = y + STATUSBAR_H - 8;

  char cnt[32];
  snprintf(cnt, sizeof(cnt), "%d %s", file_list.length, "itens");
  pgf_draw_text(SHELL_MARGIN_X, ty, sb_col, cnt);

  const char *hints = "▲▼ navega  ✕ abre  ○ volta";
  pgf_draw_text(ALIGN_RIGHT(SCREEN_WIDTH-SHELL_MARGIN_X, pgf_text_width(hints)), ty, sb_col, hints);

  int si = base_pos + rel_pos;
  if (file_list.length > 0 && si >= 0 && si < file_list.length) {
    FileListEntry *e = fileListGetNthEntry(&file_list, si);
    if (e && !e->is_folder && e->size >= 0) {
      char sz[16];
      getSizeString(sz, e->size);
      float cw = pgf_text_width(sz) + 16;
      float cx = (SCREEN_WIDTH - cw) / 2.0f;
      vita2d_draw_rectangle(cx, y+4, cw, STATUSBAR_H-8, COLOR_ALPHA(sb_acc, 30));
      pgf_draw_text(cx+8, ty, sb_acc, sz);
    }
  }
}

void drawScrollBar(int pos, int n) {
  int limit = (vitashell_config.view_mode != 1) ? MAX_ENTRIES : 16;
  if (n > limit) {
    // Thin, minimal scrollbar track
    vita2d_draw_rectangle(SCROLL_BAR_X, START_Y, SCROLL_BAR_WIDTH, limit * FONT_Y_SPACE, COLOR_ALPHA(themeTextDim(vitashell_config.theme_preset), 30));

    float y = START_Y + ((pos * FONT_Y_SPACE) / (n * FONT_Y_SPACE)) * (limit * FONT_Y_SPACE);
    float height = ((limit * FONT_Y_SPACE) / (n * FONT_Y_SPACE)) * (limit * FONT_Y_SPACE);

    float scroll_bar_y = MIN(y, (START_Y + limit * FONT_Y_SPACE - height));
    // Modern rounded-rect scrollbar thumb
    int thumb_h = MAX(height, SCROLL_BAR_MIN_HEIGHT);
    vita2d_draw_rectangle(SCROLL_BAR_X + 2, scroll_bar_y + 2, SCROLL_BAR_WIDTH - 4, thumb_h - 4, COLOR_ALPHA(themeAccentColor(vitashell_config.theme_preset), 120));
  }
}

void drawShellInfo(const char *path) {
  int is_img_bg = (vitashell_config.background_anim >= 4);
  unsigned int t_top = is_img_bg ? COLOR_ALPHA(themeTopbarBg(vitashell_config.theme_preset), 200) : themeTopbarBg(vitashell_config.theme_preset);
  unsigned int t_txt = themeTopbarText(vitashell_config.theme_preset);
  unsigned int t_card = is_img_bg ? COLOR_ALPHA(themeCardBg(vitashell_config.theme_preset), 160) : themeCardBg(vitashell_config.theme_preset);

  int full_h = 96;
  vita2d_draw_rectangle(0, 0, SCREEN_WIDTH, full_h, t_top);
  vita2d_draw_rectangle(0, full_h, SCREEN_WIDTH, 1, COLOR_ALPHA(t_txt, 16));
  vita2d_draw_rectangle(0, 0, SCREEN_WIDTH, 1, COLOR_ALPHA(t_txt, 8));

  // ---- Row 1: Title area ----
  {
    char short_path[256];
    int plen = strlen(path);
    int max_ch = 48;
    if (plen > max_ch)
      snprintf(short_path, sizeof(short_path), "...%s", path + plen - (max_ch - 3));
    else
      snprintf(short_path, sizeof(short_path), "%s", path);
    pgf_draw_text(SHELL_MARGIN_X, 8, t_txt, short_path);

    float sx = SCREEN_WIDTH - 10;
    SceDateTime time;
    sceRtcGetCurrentClock(&time, 0);
    char ts[24];
    getTimeString(ts, time_format, &time);
    sx -= pgf_text_width(ts);
    pgf_draw_text(sx, 8, DATE_TIME_COLOR, ts);
    sx -= STATUS_BAR_SPACE_X;
    if (ftpvita_is_initialized() && ftp_image) {
      sx -= vita2d_texture_get_width(ftp_image);
      vita2d_draw_texture(ftp_image, sx, 4);
      sx -= STATUS_BAR_SPACE_X;
    }
    if (sceKernelGetModel() == SCE_KERNEL_MODEL_VITA) {
      sx -= vita2d_texture_get_width(battery_image);
      vita2d_draw_texture(battery_image, sx, 5);
      vita2d_texture *bbi = battery_bar_green_image;
      if (scePowerIsLowBattery() && !scePowerIsBatteryCharging()) bbi = battery_bar_red_image;
      float pct = scePowerGetBatteryLifePercent() / 100.0f;
      float bw = vita2d_texture_get_width(bbi);
      vita2d_draw_texture_part(bbi, sx+3.0f+(1.0f-pct)*bw, 7.0f, (1.0f-pct)*bw, 0, pct*bw, vita2d_texture_get_height(bbi));
      if (scePowerIsBatteryCharging()) vita2d_draw_texture(battery_bar_charge_image, sx+3.0f, 7.0f);
    }
  }

  // ---- Separator ----
  vita2d_draw_rectangle(10, 29, SCREEN_WIDTH-20, 1, COLOR_ALPHA(t_txt, 10));

  // ---- Row 2: Toolbar ----
  {
    int aby = 32, abh = 38;
    const char *ab_labels[] = {language_container[MOVE], language_container[COPY], language_container[PASTE], language_container[DELETE], language_container[RENAME], language_container[FILTER], language_container[GROUP], language_container[SEARCH], language_container[NEW]};
    int is_root = (strchr(path, '/') == NULL);
    unsigned int t_btn_acc = is_img_bg ? COLOR_ALPHA(themeButtonAccent(vitashell_config.theme_preset), 180) : themeButtonAccent(vitashell_config.theme_preset);
    unsigned int t_btn_suc = is_img_bg ? COLOR_ALPHA(themeButtonSuccess(vitashell_config.theme_preset), 180) : themeButtonSuccess(vitashell_config.theme_preset);
    unsigned int t_btn_dng = is_img_bg ? COLOR_ALPHA(themeButtonDanger(vitashell_config.theme_preset), 180) : themeButtonDanger(vitashell_config.theme_preset);
    unsigned int t_btn_def = is_img_bg ? COLOR_ALPHA(themeButtonDefault(vitashell_config.theme_preset), 180) : themeButtonDefault(vitashell_config.theme_preset);
    unsigned int ab_disabled = COLOR_ALPHA(themeTextDim(vitashell_config.theme_preset), 80);
    unsigned int t_gold = (vitashell_config.theme_preset == THEME_PRESET_LIGHT) ? RGBA8(180,150,30,220) : RGBA8(200,170,40,210);
    unsigned int t_orange = (vitashell_config.theme_preset == THEME_PRESET_LIGHT) ? RGBA8(180,100,40,220) : RGBA8(200,120,50,230);
    unsigned int t_purple = (vitashell_config.theme_preset == THEME_PRESET_LIGHT) ? RGBA8(110,60,180,220) : RGBA8(130,80,200,210);
    unsigned int t_teal = (vitashell_config.theme_preset == THEME_PRESET_LIGHT) ? RGBA8(30,140,170,220) : RGBA8(40,160,190,210);
    unsigned int t_newg = (vitashell_config.theme_preset == THEME_PRESET_LIGHT) ? RGBA8(50,160,60,220) : RGBA8(60,180,70,210);
    unsigned int t_amber = (vitashell_config.theme_preset == THEME_PRESET_LIGHT) ? RGBA8(200,140,30,220) : RGBA8(230,160,40,220);
    unsigned int ab_colors[9] = {is_root?ab_disabled:t_amber, is_root?ab_disabled:t_btn_acc, is_root?ab_disabled:t_btn_suc, is_root?ab_disabled:t_btn_dng, is_root?ab_disabled:t_gold, (filter_mode>0)?t_orange:t_purple, t_teal, search_active?t_btn_suc:t_btn_def, t_newg};

    char search_lbl[16];
    int n = 9, bw = 93, gap = 5;

    for (int bi = 0; bi < n; bi++) {
      int bx = 10 + bi * (bw + gap);
      int disabled = (is_root && bi < 5);

      if (!disabled) {
        vita2d_draw_rectangle(bx, aby, bw, abh, COLOR_ALPHA(themeCardBg(vitashell_config.theme_preset), 160));
        vita2d_draw_rectangle(bx, aby, bw, 2, ab_colors[bi]);
        vita2d_draw_rectangle(bx, aby+abh-1, bw, 1, COLOR_ALPHA(t_txt, 8));
      } else {
        vita2d_draw_rectangle(bx, aby, bw, abh, COLOR_ALPHA(themeTextDim(vitashell_config.theme_preset), 30));
        vita2d_draw_rectangle(bx, aby, bw, 1, COLOR_ALPHA(t_txt, 5));
      }

      const char *lbl = ab_labels[bi];
      if (bi == 5) {
        const char *fm[] = {language_container[FILTER_ALL], language_container[FILTER_FOLDERS], language_container[FILTER_FILES]};
        lbl = fm[filter_mode];
      } else if (bi == 6) {
        lbl = (sort_mode == SORT_BY_NAME) ? language_container[BY_NAME] : language_container[BY_SIZE];
      } else if (bi == 7 && search_active) {
        snprintf(search_lbl, sizeof(search_lbl), "%.11s...", search_term);
        if (strlen(search_term) <= 11) lbl = search_term;
        else lbl = search_lbl;
      }
      unsigned int tc = disabled ? COLOR_ALPHA(themeTextDim(vitashell_config.theme_preset), 120) : themeTextColor(vitashell_config.theme_preset);
      float lw = pgf_text_width(lbl);
      pgf_draw_text(bx + (bw - lw)/2.0f, aby+8, tc, lbl);
    }
  }

  // ---- Row 3: Header row ----
  {
    int hy = 74;
    vita2d_draw_rectangle(0, hy, SCREEN_WIDTH, HEADER_H, t_card);
    vita2d_draw_rectangle(0, hy+HEADER_H-1, SCREEN_WIDTH, 1, COLOR_ALPHA(t_txt, 10));

    unsigned int hc = COLOR_ALPHA(themeTextDim(vitashell_config.theme_preset), 200);
    float ty = hy + HEADER_H - 7;

    if (vitashell_config.view_mode != 1) {
      pgf_draw_text(FILE_X, ty, hc, "Nome");
      if (vitashell_config.view_mode == 0) {
        pgf_draw_text(ALIGN_RIGHT(INFORMATION_X, pgf_text_width("Tamanho")), ty, hc, "Tamanho");
        pgf_draw_text(ALIGN_RIGHT(SCREEN_WIDTH-SHELL_MARGIN_X, pgf_text_width("Data")), ty, hc, "Data");
      }
    }
  }
}

void drawDeleteConfirmDialog() {
  if (getDialogStep() != DIALOG_STEP_DELETE_CONFIRM_TOUCH)
    return;

  vita2d_common_dialog_update();

  vita2d_draw_rectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, RGBA8(0, 0, 0, 190));

  int cw = 540, ch = 180;
  int cx = (SCREEN_WIDTH - cw) / 2, cy = (SCREEN_HEIGHT - ch) / 2;
  vita2d_draw_rectangle(cx, cy, cw, ch, themeDialogBg(vitashell_config.theme_preset));
  vita2d_draw_rectangle(cx, cy, cw, 1, COLOR_ALPHA(themeTopbarText(vitashell_config.theme_preset), 25));
  vita2d_draw_rectangle(cx, cy + ch - 1, cw, 1, COLOR_ALPHA(themeTopbarText(vitashell_config.theme_preset), 15));

  char dlt_msg[512];
  strncpy(dlt_msg, delete_confirm_message, 80);
  dlt_msg[80] = '\0';
  draw_centered_text(SCREEN_WIDTH / 2.0f, cy + 52, themeTextColor(vitashell_config.theme_preset), dlt_msg);

  int enter_btn_d = (enter_button == SCE_SYSTEM_PARAM_ENTER_BUTTON_CIRCLE) ? BUTTON_CIRCLE : BUTTON_CROSS;
  int cancel_btn_d = (enter_button == SCE_SYSTEM_PARAM_ENTER_BUTTON_CIRCLE) ? BUTTON_CROSS : BUTTON_CIRCLE;

  int sim_x = cx + 50, sim_y = cy + 105, sim_w = 190, sim_h = 52;
  vita2d_draw_rectangle(sim_x, sim_y, sim_w, sim_h, themeButtonSuccess(vitashell_config.theme_preset));
  vita2d_draw_rectangle(sim_x, sim_y, sim_w, 2, COLOR_ALPHA(themeTopbarText(vitashell_config.theme_preset), 60));
  drawButton(enter_btn_d, sim_x + 20, sim_y + 16);
  draw_centered_text(sim_x + sim_w / 2.0f + 15, sim_y + 16, themeTopbarText(vitashell_config.theme_preset), language_container[CONFIRM_YES_BTN]);

  int nao_x = cx + cw - 50 - 190, nao_y = cy + 105, nao_w = 190, nao_h = 52;
  vita2d_draw_rectangle(nao_x, nao_y, nao_w, nao_h, themeButtonDanger(vitashell_config.theme_preset));
  vita2d_draw_rectangle(nao_x, nao_y, nao_w, 2, COLOR_ALPHA(themeTopbarText(vitashell_config.theme_preset), 50));
  drawButton(cancel_btn_d, nao_x + 20, nao_y + 16);
  draw_centered_text(nao_x + nao_w / 2.0f + 15, nao_y + 16, themeTopbarText(vitashell_config.theme_preset), language_container[CONFIRM_NO_BTN]);
}

static void ftp_log_cb(const char *msg);

void initFtp() {
  ftpvita_set_info_log_cb(ftp_log_cb);

  // Add all the current mountpoints to ftpvita
  int i;
  for (i = 0; i < getNumberOfDevices(); i++) {
    char **devices = getDevices();
    if (devices[i]) {
      if (isSafeMode() && strcmp(devices[i], "ux0:") != 0)
        continue;

      ftpvita_add_device(devices[i]);
    }
  }

  ftpvita_ext_add_custom_command("PROM", ftpvita_PROM);
}

void initUsb() {
  char *path = NULL;

  if (vitashell_config.usbdevice == USBDEVICE_MODE_MEMORY_CARD) {
    if (checkFileExist("sdstor0:xmc-lp-ign-userext"))
      path = "sdstor0:xmc-lp-ign-userext";
    else if (checkFileExist("sdstor0:int-lp-ign-userext"))
      path = "sdstor0:int-lp-ign-userext";
    else
      infoDialog(language_container[MEMORY_CARD_NOT_FOUND]);
  } else if (vitashell_config.usbdevice == USBDEVICE_MODE_GAME_CARD) {
    if (checkFileExist("sdstor0:gcd-lp-ign-gamero"))
      path = "sdstor0:gcd-lp-ign-gamero";
    else
      infoDialog(language_container[GAME_CARD_NOT_FOUND]);
  } else if (vitashell_config.usbdevice == USBDEVICE_MODE_SD2VITA) {
    if (checkFileExist("sdstor0:gcd-lp-ign-entire"))
      path = "sdstor0:gcd-lp-ign-entire";
    else
      infoDialog(language_container[MICROSD_NOT_FOUND]);
  } else if (vitashell_config.usbdevice == USBDEVICE_MODE_PSVSD) {
    if (checkFileExist("sdstor0:uma-pp-act-a"))
      path = "sdstor0:uma-pp-act-a";
    else if (checkFileExist("sdstor0:uma-lp-act-entire"))
      path = "sdstor0:uma-lp-act-entire";
    else
      infoDialog(language_container[MICROSD_NOT_FOUND]);
  }

  if (!path)
    return;

  usbdevice_modid = startUsb("ux0:FMVita/module/usbdevice.skprx", path, SCE_USBSTOR_VSTOR_TYPE_FAT);
  if (usbdevice_modid >= 0) {
    // Lock power timers
    powerLock();
    
    initMessageDialog(SCE_MSG_DIALOG_BUTTON_TYPE_CANCEL, language_container[USB_CONNECTED]);
    setDialogStep(DIALOG_STEP_USB);
  } else {
    errorDialog(usbdevice_modid);
  }
}

int dialogSteps() {
  int refresh = REFRESH_MODE_NONE;

  int msg_result = updateMessageDialog();
  int netcheck_result = updateNetCheckDialog();
  int ime_result = updateImeDialog();

  switch (getDialogStep()) {
    case DIALOG_STEP_ERROR:
    case DIALOG_STEP_INFO:
    case DIALOG_STEP_SYSTEM:
    {
      if (msg_result == MESSAGE_DIALOG_RESULT_NONE ||
          msg_result == MESSAGE_DIALOG_RESULT_FINISHED) {
        refresh = REFRESH_MODE_NORMAL;
        setDialogStep(DIALOG_STEP_NONE);
      }

      break;
    }
    
    case DIALOG_STEP_CANCELED:
      refresh = REFRESH_MODE_NORMAL;
      setDialogStep(DIALOG_STEP_NONE);
      break;
      
    case DIALOG_STEP_DELETED:
    {
      if (msg_result == MESSAGE_DIALOG_RESULT_NONE ||
          msg_result == MESSAGE_DIALOG_RESULT_FINISHED) {
        FileListEntry *file_entry = fileListGetNthEntry(&file_list, base_pos + rel_pos);
        if (file_entry) {
          // Empty mark list if on marked entry
          if (fileListFindEntry(&mark_list, file_entry->name)) {
            fileListEmpty(&mark_list);
          }

          refresh = REFRESH_MODE_NORMAL;
        }
        
        setDialogStep(DIALOG_STEP_NONE);
      }

      break;
    }
    
    case DIALOG_STEP_COMPRESSED:
    {
      if (msg_result == MESSAGE_DIALOG_RESULT_NONE ||
          msg_result == MESSAGE_DIALOG_RESULT_FINISHED) {
        FileListEntry *file_entry = fileListGetNthEntry(&file_list, base_pos + rel_pos);
        if (file_entry) {
          // Empty mark list if on marked entry
          if (fileListFindEntry(&mark_list, file_entry->name)) {
            fileListEmpty(&mark_list);
          }
          
          // Focus
          setFocusName(compress_name);
          
          refresh = REFRESH_MODE_SETFOCUS;
        }
        
        setDialogStep(DIALOG_STEP_NONE);
      }

      break;
    }
    
    case DIALOG_STEP_COPIED:
    case DIALOG_STEP_MOVED:
    {
      if (msg_result == MESSAGE_DIALOG_RESULT_NONE ||
          msg_result == MESSAGE_DIALOG_RESULT_FINISHED) {
        // Empty mark list
        fileListEmpty(&mark_list);
        
        // Copy copy list to mark list
        FileListEntry *copy_entry = copy_list.head;
        
        int i;
        for (i = 0; i < copy_list.length; i++) {
          fileListAddEntry(&mark_list, fileListCopyEntry(copy_entry), SORT_NONE);

          // Next
          copy_entry = copy_entry->next;
        }
        
        // Store undo for the first copied item
        if (getDialogStep() == DIALOG_STEP_COPIED && copy_list.length > 0) {
          snprintf(undo_dst, MAX_PATH_LENGTH, "%s%s", file_list.path, copy_list.head->name);
          snprintf(undo_src, MAX_PATH_LENGTH, "%s%s", copy_list.path, copy_list.head->name);
          undo_type = UNDO_COPY;
          undo_available = 1;
        }

        // Focus
        setFocusName(copy_list.head->name);

        // Empty copy list when moved
        if (getDialogStep() == DIALOG_STEP_MOVED)
          fileListEmpty(&copy_list);
        
        // Umount and remove from clipboard after pasting
        if (pfs_mounted_path[0] &&
            !strstr(file_list.path, pfs_mounted_path) &&
             strstr(copy_list.path, pfs_mounted_path)) {
          pfsUmount();
          fileListEmpty(&copy_list);
        }

        refresh = REFRESH_MODE_SETFOCUS;
        setDialogStep(DIALOG_STEP_NONE);
      }

      break;
    }
    
    case DIALOG_STEP_REFRESH_LIVEAREA_QUESTION:
    {
      if (msg_result == MESSAGE_DIALOG_RESULT_YES) {
        initMessageDialog(MESSAGE_DIALOG_PROGRESS_BAR, language_container[REFRESHING]);
        setDialogStep(DIALOG_STEP_REFRESHING);

        SceUID thid = sceKernelCreateThread("refresh_thread", (SceKernelThreadEntry)refresh_thread, 0x40, 0x100000, 0, 0, NULL);
        if (thid >= 0)
          sceKernelStartThread(thid, 0, NULL);
      } else if (msg_result == MESSAGE_DIALOG_RESULT_NO) {
        setDialogStep(DIALOG_STEP_NONE);
      }

      break;
    }
    
    case DIALOG_STEP_REFRESH_LICENSE_DB_QUESTION:
    {
      if (msg_result == MESSAGE_DIALOG_RESULT_YES) {
        initMessageDialog(MESSAGE_DIALOG_PROGRESS_BAR, language_container[REFRESHING]);
        setDialogStep(DIALOG_STEP_REFRESHING);

        SceUID thid = sceKernelCreateThread("license_thread", (SceKernelThreadEntry)license_thread, 0x40, 0x100000, 0, 0, NULL);
        if (thid >= 0)
          sceKernelStartThread(thid, 0, NULL);
      } else if (msg_result == MESSAGE_DIALOG_RESULT_NO) {
        setDialogStep(DIALOG_STEP_NONE);
      }

      break;
    }

    case DIALOG_STEP_USB_ATTACH_WAIT:
    {
      if (msg_result == MESSAGE_DIALOG_RESULT_RUNNING) {
        if (checkFileExist("sdstor0:uma-lp-act-entire")) {
          sceMsgDialogClose();
        }
      } else {
        if (msg_result == MESSAGE_DIALOG_RESULT_NONE ||
            msg_result == MESSAGE_DIALOG_RESULT_FINISHED) {
          setDialogStep(DIALOG_STEP_NONE);
          
          if (checkFileExist("sdstor0:uma-lp-act-entire")) {
            int res = vshIoMount(0xF00, NULL, 0, 0, 0, 0);
            if (res < 0)
              errorDialog(res);
            else
              infoDialog(language_container[UMA0_MOUNTED]);
            refresh = REFRESH_MODE_NORMAL;
          }
        }
      }
      
      break;
    }
    
    case DIALOG_STEP_FTP_WAIT:
    {
      if (msg_result == MESSAGE_DIALOG_RESULT_RUNNING) {
        int state = 0;
        sceNetCtlInetGetState(&state);
        if (state == 3) {
          int res = ftpvita_init(vita_ip, &vita_port);
          if (res >= 0) {
            initFtp();
            sceMsgDialogClose();
          }
        }
      } else {
        if (msg_result == MESSAGE_DIALOG_RESULT_NONE ||
            msg_result == MESSAGE_DIALOG_RESULT_FINISHED) {
          setDialogStep(DIALOG_STEP_NONE);

          // Dialog - custom touch FTP dialog
          if (ftpvita_is_initialized()) {
            setDialogStep(DIALOG_STEP_FTP_TOUCH);
          } else {
            powerUnlock();
          }
        }
      }
      
      break;
    }
    
    case DIALOG_STEP_FTP:
    {
      if (msg_result == MESSAGE_DIALOG_RESULT_YES) {
        refresh = REFRESH_MODE_NORMAL;
        setDialogStep(DIALOG_STEP_NONE);
      } else if (msg_result == MESSAGE_DIALOG_RESULT_NO) {
        powerUnlock();
        ftpvita_fini();
        refresh = REFRESH_MODE_NORMAL;
        setDialogStep(DIALOG_STEP_NONE);
      }

      break;
    }
    
    case DIALOG_STEP_USB_WAIT:
    {
      if (msg_result == MESSAGE_DIALOG_RESULT_RUNNING) {
        SceUdcdDeviceState state;
        sceUdcdGetDeviceState(&state);
        
        if (state.cable & SCE_UDCD_STATUS_CABLE_CONNECTED) {
          sceMsgDialogClose();
        }
      } else {
        if (msg_result == MESSAGE_DIALOG_RESULT_NONE ||
            msg_result == MESSAGE_DIALOG_RESULT_FINISHED) {
          setDialogStep(DIALOG_STEP_NONE);

          SceUdcdDeviceState state;
          sceUdcdGetDeviceState(&state);
          
          if (state.cable & SCE_UDCD_STATUS_CABLE_CONNECTED) {
            initUsb();
          }
        }
      }
      
      break;
    }
    
    case DIALOG_STEP_USB:
    {
      if (msg_result == MESSAGE_DIALOG_RESULT_RUNNING) {
        SceUdcdDeviceState state;
        sceUdcdGetDeviceState(&state);
        
        if (state.cable & SCE_UDCD_STATUS_CABLE_DISCONNECTED) {
          sceMsgDialogClose();
        }
      } else if (msg_result == MESSAGE_DIALOG_RESULT_FINISHED) {
        powerUnlock();
        stopUsb(usbdevice_modid);
        refresh = REFRESH_MODE_NORMAL;
        setDialogStep(DIALOG_STEP_NONE);
      }

      break;
    }
    
    case DIALOG_STEP_PASTE:
    {
      if (msg_result == MESSAGE_DIALOG_RESULT_RUNNING) {
        CopyArguments args;
        args.file_list = &file_list;
        args.copy_list = &copy_list;
        args.archive_path = archive_copy_path;
        args.copy_mode = copy_mode;

        setDialogStep(DIALOG_STEP_COPYING);

        SceUID thid = sceKernelCreateThread("copy_thread", (SceKernelThreadEntry)copy_thread, 0x40, 0x100000, 0, 0, NULL);
        if (thid >= 0)
          sceKernelStartThread(thid, sizeof(CopyArguments), &args);
      }

      break;
    }
    
    case DIALOG_STEP_TOUCH_CONFIRM:
    {
      if (pressed_pad[PAD_ENTER]) {
        if (touch_confirm_yes_cb) {
          touch_confirm_yes_cb();
        }
        touch_confirm_yes_cb = NULL;
        touch_confirm_no_cb = NULL;
        setDialogStep(DIALOG_STEP_NONE);
      } else if (pressed_pad[PAD_CANCEL]) {
        if (touch_confirm_no_cb) {
          touch_confirm_no_cb();
        }
        touch_confirm_yes_cb = NULL;
        touch_confirm_no_cb = NULL;
        setDialogStep(DIALOG_STEP_NONE);
      }
      break;
    }

    case DIALOG_STEP_FTP_TOUCH:
    {
      if (pressed_pad[PAD_CANCEL]) {
        setDialogStep(DIALOG_STEP_NONE);
      } else if (pressed_pad[PAD_ENTER]) {
        powerUnlock();
        ftpvita_fini();
        setDialogStep(DIALOG_STEP_NONE);
      }
      break;
    }

    case DIALOG_STEP_DELETE_CONFIRM_TOUCH:
    {
      if (pressed_pad[PAD_ENTER]) {
        initMessageDialog(MESSAGE_DIALOG_PROGRESS_BAR, language_container[DELETING]);
        setDialogStep(DIALOG_STEP_DELETE_CONFIRMED);
      } else if (pressed_pad[PAD_CANCEL]) {
        setDialogStep(DIALOG_STEP_NONE);
      }
      break;
    }
    
    case DIALOG_STEP_DELETE_QUESTION:
    {
      if (msg_result == MESSAGE_DIALOG_RESULT_YES) {
        initMessageDialog(MESSAGE_DIALOG_PROGRESS_BAR, language_container[DELETING]);
        setDialogStep(DIALOG_STEP_DELETE_CONFIRMED);
      } else if (msg_result == MESSAGE_DIALOG_RESULT_NO) {
        setDialogStep(DIALOG_STEP_NONE);
      }

      break;
    }
    
    case DIALOG_STEP_DELETE_CONFIRMED:
    {
      if (msg_result == MESSAGE_DIALOG_RESULT_RUNNING) {
        DeleteArguments args;
        args.file_list = &file_list;
        args.mark_list = &mark_list;
        args.index = base_pos + rel_pos;

        setDialogStep(DIALOG_STEP_DELETING);

        SceUID thid = sceKernelCreateThread("delete_thread", (SceKernelThreadEntry)delete_thread, 0x40, 0x100000, 0, 0, NULL);
        if (thid >= 0)
          sceKernelStartThread(thid, sizeof(DeleteArguments), &args);
      }

      break;
    }
    
    case DIALOG_STEP_EXPORT_QUESTION:
    {
      if (msg_result == MESSAGE_DIALOG_RESULT_YES) {
        initMessageDialog(MESSAGE_DIALOG_PROGRESS_BAR, language_container[EXPORTING]);
        setDialogStep(DIALOG_STEP_EXPORT_CONFIRMED);
      } else if (msg_result == MESSAGE_DIALOG_RESULT_NO) {
        setDialogStep(DIALOG_STEP_NONE);
      }

      break;
    }
    
    case DIALOG_STEP_EXPORT_CONFIRMED:
    {
      if (msg_result == MESSAGE_DIALOG_RESULT_RUNNING) {
        ExportArguments args;
        args.file_list = &file_list;
        args.mark_list = &mark_list;
        args.index = base_pos + rel_pos;

        setDialogStep(DIALOG_STEP_EXPORTING);

        SceUID thid = sceKernelCreateThread("export_thread", (SceKernelThreadEntry)export_thread, 0x40, 0x100000, 0, 0, NULL);
        if (thid >= 0)
          sceKernelStartThread(thid, sizeof(ExportArguments), &args);
      }

      break;
    }
    
    case DIALOG_STEP_SEARCH:
    {
      if (ime_result == IME_DIALOG_RESULT_FINISHED) {
        char *name = (char *)getImeDialogInputTextUTF8();
        if (name[0] == '\0') {
          setDialogStep(DIALOG_STEP_NONE);
        } else {
          strncpy(search_term, name, 255);
          search_term[255] = '\0';
          search_active = 1;
          refresh = REFRESH_MODE_NORMAL;
          setDialogStep(DIALOG_STEP_NONE);
        }
      } else if (ime_result == IME_DIALOG_RESULT_CANCELED) {
        setDialogStep(DIALOG_STEP_NONE);
      }
      break;
    }

    case DIALOG_STEP_RENAME:
    {
      if (ime_result == IME_DIALOG_RESULT_FINISHED) {
        char *name = (char *)getImeDialogInputTextUTF8();
        if (name[0] == '\0') {
          setDialogStep(DIALOG_STEP_NONE);
        } else {
          FileListEntry *file_entry = fileListGetNthEntry(&file_list, base_pos + rel_pos);
          if (!file_entry) {
            setDialogStep(DIALOG_STEP_NONE);
            break;
          }
          
          char old_name[MAX_NAME_LENGTH];
          strcpy(old_name, file_entry->name);
          removeEndSlash(old_name);

          if (strcasecmp(old_name, name) == 0) { // No change
            setDialogStep(DIALOG_STEP_NONE);
          } else {
            char old_path[MAX_PATH_LENGTH];
            char new_path[MAX_PATH_LENGTH];

            snprintf(old_path, MAX_PATH_LENGTH, "%s%s", file_list.path, old_name);
            snprintf(new_path, MAX_PATH_LENGTH, "%s%s", file_list.path, name);

            if (isProtectedPath(old_path)) {
              infoDialog("Erro: Pasta de sistema protegida.");
            } else {
              int res = sceIoRename(old_path, new_path);
              if (res < 0) {
                errorDialog(res);
              } else {
                snprintf(undo_src, MAX_PATH_LENGTH, "%s", old_path);
                snprintf(undo_dst, MAX_PATH_LENGTH, "%s", new_path);
                undo_type = UNDO_MOVE;
                undo_available = 1;
                refresh = REFRESH_MODE_NORMAL;
                setDialogStep(DIALOG_STEP_NONE);
              }
            }
          }
        }
      } else if (ime_result == IME_DIALOG_RESULT_CANCELED) {
        setDialogStep(DIALOG_STEP_NONE);
      }

      break;
    }

    case DIALOG_STEP_NEW_FOLDER:
    {
      if (ime_result == IME_DIALOG_RESULT_FINISHED) {
        char *name = (char *)getImeDialogInputTextUTF8();
        if (name[0] == '\0') {
          setDialogStep(DIALOG_STEP_NONE);
        } else {
          char path[MAX_PATH_LENGTH];
          snprintf(path, MAX_PATH_LENGTH, "%s%s", file_list.path, name);

          int res = sceIoMkdir(path, 0777);
          if (res < 0) {
            errorDialog(res);
          } else {
            // Focus
            char focus_name[MAX_NAME_LENGTH];
            strcpy(focus_name, name);
            addEndSlash(focus_name);
            setFocusName(focus_name);
            
            refresh = REFRESH_MODE_SETFOCUS;
            setDialogStep(DIALOG_STEP_NONE);
          }
        }
      } else if (ime_result == IME_DIALOG_RESULT_CANCELED) {
        setDialogStep(DIALOG_STEP_NONE);
      }

      break;
    }

    case DIALOG_STEP_NEW_FILE:
    {
      if (ime_result == IME_DIALOG_RESULT_FINISHED) {
        char *name = (char *)getImeDialogInputTextUTF8();
        if (name[0] == '\0') {
          setDialogStep(DIALOG_STEP_NONE);
        } else {
          char path[MAX_PATH_LENGTH];
          snprintf(path, MAX_PATH_LENGTH, "%s%s", file_list.path, name);

          SceUID fd = sceIoOpen(path, SCE_O_WRONLY | SCE_O_CREAT, 0777);
          if (fd < 0) {
            errorDialog(fd);
          } else {
            sceIoClose(fd);

            // Focus
            char focus_name[MAX_NAME_LENGTH];
            strcpy(focus_name, name);
            addEndSlash(focus_name);
            setFocusName(focus_name);

            refresh = REFRESH_MODE_SETFOCUS;
            setDialogStep(DIALOG_STEP_NONE);
            refreshFileList();
          }
        }
      } else if (ime_result == IME_DIALOG_RESULT_CANCELED) {
        setDialogStep(DIALOG_STEP_NONE);
      }
      break;
    }

    case DIALOG_STEP_COMPRESS_NAME:
    {
      if (ime_result == IME_DIALOG_RESULT_FINISHED) {
        char *name = (char *)getImeDialogInputTextUTF8();
        if (name[0] == '\0') {
          setDialogStep(DIALOG_STEP_NONE);
        } else {
          strcpy(compress_name, name);

          initImeDialog(language_container[COMPRESSION_LEVEL], "6", 1, SCE_IME_TYPE_NUMBER, 0, 0);
          setDialogStep(DIALOG_STEP_COMPRESS_LEVEL);
        }
      } else if (ime_result == IME_DIALOG_RESULT_CANCELED) {
        setDialogStep(DIALOG_STEP_NONE);
      }
      
      break;
    }

    case DIALOG_STEP_COMPRESS_LEVEL:
    {
      if (ime_result == IME_DIALOG_RESULT_FINISHED) {
        char *level = (char *)getImeDialogInputTextUTF8();
        if (level[0] == '\0') {
          setDialogStep(DIALOG_STEP_NONE);
        } else {
          snprintf(cur_file, MAX_PATH_LENGTH, "%s%s", file_list.path, compress_name);

          CompressArguments args;
          args.file_list = &file_list;
          args.mark_list = &mark_list;
          args.index = base_pos + rel_pos;
          args.level = atoi(level);
          args.path = cur_file;

          initMessageDialog(MESSAGE_DIALOG_PROGRESS_BAR, language_container[COMPRESSING]);
          setDialogStep(DIALOG_STEP_COMPRESSING);

          SceUID thid = sceKernelCreateThread("compress_thread", (SceKernelThreadEntry)compress_thread, 0x40, 0x100000, 0, 0, NULL);
          if (thid >= 0)
            sceKernelStartThread(thid, sizeof(CompressArguments), &args);
        }
      } else if (ime_result == IME_DIALOG_RESULT_CANCELED) {
        setDialogStep(DIALOG_STEP_NONE);
      }
      
      break;
    }
    
    case DIALOG_STEP_HASH_QUESTION:
    {
      if (msg_result == MESSAGE_DIALOG_RESULT_YES) {
        // Throw up the progress bar, enter hashing state
        initMessageDialog(MESSAGE_DIALOG_PROGRESS_BAR, language_container[HASHING]);
        setDialogStep(DIALOG_STEP_HASH_CONFIRMED);
      } else if (msg_result == MESSAGE_DIALOG_RESULT_NO) {
        // Quit
        setDialogStep(DIALOG_STEP_NONE);
      }

      break;
    }
    
    case DIALOG_STEP_HASH_CONFIRMED:
    {
      if (msg_result == MESSAGE_DIALOG_RESULT_RUNNING) {
        // User has confirmed desire to hash, get requested file entry
        FileListEntry *file_entry = fileListGetNthEntry(&file_list, base_pos + rel_pos);
        if (!file_entry) {
          setDialogStep(DIALOG_STEP_NONE);
          break;
        }
        
        // Place the full file path in cur_file
        snprintf(cur_file, MAX_PATH_LENGTH, "%s%s", file_list.path, file_entry->name);

        HashArguments args;
        args.file_path = cur_file;

        setDialogStep(DIALOG_STEP_HASHING);

        // Create a thread to run out actual sum
        SceUID thid = sceKernelCreateThread("hash_thread", (SceKernelThreadEntry)hash_thread, 0x40, 0x100000, 0, 0, NULL);
        if (thid >= 0)
          sceKernelStartThread(thid, sizeof(HashArguments), &args);
      }

      break;
    }
    
    case DIALOG_STEP_INSTALL_QUESTION:
    {
      if (msg_result == MESSAGE_DIALOG_RESULT_YES) {
        initMessageDialog(MESSAGE_DIALOG_PROGRESS_BAR, language_container[INSTALLING]);
        setDialogStep(DIALOG_STEP_INSTALL_CONFIRMED);
      } else if (msg_result == MESSAGE_DIALOG_RESULT_NO) {
        setDialogStep(DIALOG_STEP_NONE);
      }

      break;
    }
    
    case DIALOG_STEP_INSTALL_CONFIRMED:
    {
      if (msg_result == MESSAGE_DIALOG_RESULT_RUNNING) {
        InstallArguments args;

        if (install_list.length > 0) {
          FileListEntry *entry = install_list.head;
          snprintf(install_path, MAX_PATH_LENGTH, "%s%s", install_list.path, entry->name);
          args.file = install_path;

          // Focus
          setFocusOnFilename(entry->name);

          // Remove entry
          fileListRemoveEntry(&install_list, entry);
        } else {
          args.file = cur_file;
        }

        setDialogStep(DIALOG_STEP_INSTALLING);

        SceUID thid = sceKernelCreateThread("install_thread", (SceKernelThreadEntry)install_thread, 0x40, 0x100000, 0, 0, NULL);
        if (thid >= 0)
          sceKernelStartThread(thid, sizeof(InstallArguments), &args);
      }

      break;
    }
    
    case DIALOG_STEP_INSTALL_CONFIRMED_QR:
    {
      if (msg_result == MESSAGE_DIALOG_RESULT_RUNNING) {
        InstallArguments args;
        args.file = getLastDownloadQR();

        setDialogStep(DIALOG_STEP_INSTALLING);

        SceUID thid = sceKernelCreateThread("install_thread", (SceKernelThreadEntry)install_thread, 0x40, 0x100000, 0, 0, NULL);
        if (thid >= 0)
          sceKernelStartThread(thid, sizeof(InstallArguments), &args);
      }

      break;
    }
 
    case DIALOG_STEP_INSTALL_WARNING:
    {
      if (msg_result == MESSAGE_DIALOG_RESULT_YES) {
        setDialogStep(DIALOG_STEP_INSTALL_WARNING_AGREED);
      } else if (msg_result == MESSAGE_DIALOG_RESULT_NO) {
        setDialogStep(DIALOG_STEP_CANCELED);
      }

      break;
    }
    
    case DIALOG_STEP_INSTALLED:
    {
      if (msg_result == MESSAGE_DIALOG_RESULT_NONE ||
          msg_result == MESSAGE_DIALOG_RESULT_FINISHED) {
        if (install_list.length > 0) {
          initMessageDialog(MESSAGE_DIALOG_PROGRESS_BAR, language_container[INSTALLING]);
          setDialogStep(DIALOG_STEP_INSTALL_CONFIRMED);
          break;
        }

        refresh = REFRESH_MODE_NORMAL;
        setDialogStep(DIALOG_STEP_NONE);
      }

      break;
    }
    
    case DIALOG_STEP_SETTINGS_AGREEMENT:
    {
      if (msg_result == MESSAGE_DIALOG_RESULT_YES) {
        settingsAgree();
        setDialogStep(DIALOG_STEP_NONE);
      } else if (msg_result == MESSAGE_DIALOG_RESULT_NO) {
        settingsDisagree();
        setDialogStep(DIALOG_STEP_NONE);
      } else if (msg_result == MESSAGE_DIALOG_RESULT_FINISHED) {
        settingsAgree();
        setDialogStep(DIALOG_STEP_NONE);
      }
      
      break;
    }
    
    case DIALOG_STEP_SETTINGS_STRING:
    {
      if (ime_result == IME_DIALOG_RESULT_FINISHED) {
        char *string = (char *)getImeDialogInputTextUTF8();
        if (string[0] != '\0') {
          strcpy((char *)getImeDialogInitialText(), string);
        }

        setDialogStep(DIALOG_STEP_NONE);
      } else if (ime_result == IME_DIALOG_RESULT_CANCELED) {
        setDialogStep(DIALOG_STEP_NONE);
      }
      
      break;
    }
    
    case DIALOG_STEP_QR:
    {
      if (msg_result == MESSAGE_DIALOG_RESULT_FINISHED) {
        setDialogStep(DIALOG_STEP_NONE);
        setScannedQR(0);
      } else if (scannedQR()) {
        setDialogStep(DIALOG_STEP_QR_DONE);
        sceMsgDialogClose();
        setScannedQR(0);
        stopQR();
      }
      
      break;
    }
    
    case DIALOG_STEP_QR_DONE:
    {
      if (msg_result == MESSAGE_DIALOG_RESULT_FINISHED) {
        setDialogStep(DIALOG_STEP_QR_WAITING);
        stopQR();
        SceUID thid = sceKernelCreateThread("qr_scan_thread", (SceKernelThreadEntry)qr_scan_thread, 0x10000100, 0x100000, 0, 0, NULL);
        if (thid >= 0)
          sceKernelStartThread(thid, 0, NULL);
      }
      
      break;
    }
    
    case DIALOG_STEP_QR_WAITING:
    {
      break;
    }
    
    case DIALOG_STEP_QR_CONFIRM:
    {
      if (msg_result == MESSAGE_DIALOG_RESULT_YES) {
        initMessageDialog(MESSAGE_DIALOG_PROGRESS_BAR, language_container[DOWNLOADING]);
        setDialogStep(DIALOG_STEP_QR_DOWNLOADING);
      } else if (msg_result == MESSAGE_DIALOG_RESULT_NO) {
        setDialogStep(DIALOG_STEP_NONE);
      }
      
      break;
    }
    
    case DIALOG_STEP_QR_DOWNLOADED:
    {
      if (msg_result == MESSAGE_DIALOG_RESULT_FINISHED) {
        setDialogStep(DIALOG_STEP_NONE);
      }
      
      break;
    }
    
    case DIALOG_STEP_QR_DOWNLOADED_VPK:
    {
      if (msg_result == MESSAGE_DIALOG_RESULT_FINISHED) {
        initMessageDialog(MESSAGE_DIALOG_PROGRESS_BAR, language_container[INSTALLING]);
        setDialogStep(DIALOG_STEP_INSTALL_CONFIRMED_QR);
      }
      
      break;
    }
    
    case DIALOG_STEP_QR_OPEN_WEBSITE:
    {
      if (msg_result == MESSAGE_DIALOG_RESULT_YES) {
        setDialogStep(DIALOG_STEP_NONE);
        sceAppMgrLaunchAppByUri(0xFFFFF, getLastQR());
      } else if (msg_result == MESSAGE_DIALOG_RESULT_NO) {
        setDialogStep(DIALOG_STEP_NONE);
      } else if (msg_result == MESSAGE_DIALOG_RESULT_FINISHED) {
        setDialogStep(DIALOG_STEP_NONE);
        sceAppMgrLaunchAppByUri(0xFFFFF, getLastQR());
      }
      
      break;
    }
    
    case DIALOG_STEP_QR_SHOW_CONTENTS:
    {
      if (msg_result == MESSAGE_DIALOG_RESULT_FINISHED) {
        setDialogStep(DIALOG_STEP_NONE);
      }
      
      break;
    }
    
    case DIALOG_STEP_ENTER_PASSWORD:
    {
      if (ime_result == IME_DIALOG_RESULT_FINISHED) {
        char *password = (char *)getImeDialogInputTextUTF8();
        if (password[0] == '\0') {
          setDialogStep(DIALOG_STEP_NONE);
        } else {
          // TODO: verify password
          archiveSetPassword(password);
          
          FileListEntry *file_entry = fileListGetNthEntry(&file_list, base_pos + rel_pos);
          if (!file_entry) {
            setDialogStep(DIALOG_STEP_NONE);
            break;
          }
          
          setInArchive();
          setDirArchiveLevel();

          snprintf(archive_path, MAX_PATH_LENGTH, "%s%s", file_list.path, file_entry->name);

          strcat(file_list.path, file_entry->name);
          addEndSlash(file_list.path);

          dirLevelUp();
          
          refresh = REFRESH_MODE_NORMAL;
          setDialogStep(DIALOG_STEP_NONE);
        }
      } else if (ime_result == IME_DIALOG_RESULT_CANCELED) {
        setDialogStep(DIALOG_STEP_NONE);
      }

      break;
    }
    
    case DIALOG_STEP_ADHOC_SEND_NETCHECK:
    {
      if (netcheck_result == NETCHECK_DIALOG_RESULT_CONNECTED) {
        initAdhocDialog();
        setDialogStep(DIALOG_STEP_NONE);
      } else if (netcheck_result == NETCHECK_DIALOG_RESULT_NOT_CONNECTED) {
        setDialogStep(DIALOG_STEP_NONE);
      }
      
      break;
    }
    
    case DIALOG_STEP_ADHOC_SEND_WAITING:
    {
      if (msg_result == MESSAGE_DIALOG_RESULT_RUNNING) {
        // Wait for a response, then close
        if (strcmp(adhocReceiveClientReponse(), "YES") == 0 ||
            strcmp(adhocReceiveClientReponse(), "NO") == 0) {
          sceMsgDialogClose();
        }
      } else if (msg_result == MESSAGE_DIALOG_RESULT_FINISHED) {
        if (strcmp(adhocReceiveClientReponse(), "YES") == 0) {
          SendArguments args;
          args.file_list = &file_list;
          args.mark_list = &mark_list;
          args.index = base_pos + rel_pos;

          initMessageDialog(MESSAGE_DIALOG_PROGRESS_BAR, language_container[SENDING]);
          setDialogStep(DIALOG_STEP_ADHOC_SENDING);

          SceUID thid = sceKernelCreateThread("send_thread", (SceKernelThreadEntry)send_thread, 0x40, 0x100000, 0, 0, NULL);
          if (thid >= 0)
            sceKernelStartThread(thid, sizeof(SendArguments), &args);
        } else if (strcmp(adhocReceiveClientReponse(), "NO") == 0) {
          initMessageDialog(SCE_MSG_DIALOG_BUTTON_TYPE_CANCEL, language_container[ADHOC_CLIENT_DECLINED]);
          setDialogStep(DIALOG_STEP_ADHOC_SEND_CLIENT_DECLINED);
        } else {
          // Return to select menu
          adhocCloseSockets();
          initAdhocDialog();
          setDialogStep(DIALOG_STEP_NONE);
        }
      }
      
      break;
    }
    
    case DIALOG_STEP_ADHOC_SEND_CLIENT_DECLINED:
    {
      if (msg_result == MESSAGE_DIALOG_RESULT_FINISHED) {
        // Return to select menu
        adhocCloseSockets();
        initAdhocDialog();
        setDialogStep(DIALOG_STEP_NONE);
      }
      
      break;
    }
    
    case DIALOG_STEP_ADHOC_RECEIVE_NETCHECK:
    {
      if (netcheck_result == NETCHECK_DIALOG_RESULT_CONNECTED) {
        adhocWaitingForServerRequest();
        initMessageDialog(SCE_MSG_DIALOG_BUTTON_TYPE_CANCEL, language_container[ADHOC_RECEIVE_SEARCHING_PSVITA]);
        setDialogStep(DIALOG_STEP_ADHOC_RECEIVE_SEARCHING);
      } else if (netcheck_result == NETCHECK_DIALOG_RESULT_NOT_CONNECTED) {
        setDialogStep(DIALOG_STEP_NONE);
      }
      
      break;
    }
    
    case DIALOG_STEP_ADHOC_RECEIVE_SEARCHING:
    {
      if (msg_result == MESSAGE_DIALOG_RESULT_RUNNING) {
        // Wait for a request, then close
        if (adhocReceiveServerRequest() == 1) {
          sceMsgDialogClose();
        }
      } else if (msg_result == MESSAGE_DIALOG_RESULT_FINISHED) {
        // If the dialog is closed and we got a request, go to question state, otherwise end waiting
        if (adhocReceiveServerRequest() == 1) {
          initMessageDialog(SCE_MSG_DIALOG_BUTTON_TYPE_YESNO, language_container[ADHOC_RECEIVE_QUESTION], adhocGetServerNickname());
          setDialogStep(DIALOG_STEP_ADHOC_RECEIVE_QUESTION);
        } else {
          adhocCloseSockets();
          sceNetCtlAdhocDisconnect();
          setDialogStep(DIALOG_STEP_NONE);
        }
      }
      
      break;
    }
    
    case DIALOG_STEP_ADHOC_RECEIVE_QUESTION:
    {
      if (msg_result == MESSAGE_DIALOG_RESULT_YES) {
        int res = adhocSendServerResponse("YES");
        if (res < 0) {
          adhocCloseSockets();
          sceNetCtlAdhocDisconnect();
          errorDialog(res);
        } else {
          ReceiveArguments args;
          args.file_list = &file_list;
          args.mark_list = &mark_list;
          args.index = base_pos + rel_pos;

          initMessageDialog(MESSAGE_DIALOG_PROGRESS_BAR, language_container[RECEIVING]);
          setDialogStep(DIALOG_STEP_ADHOC_RECEIVING);

          SceUID thid = sceKernelCreateThread("receive_thread", (SceKernelThreadEntry)receive_thread, 0x40, 0x100000, 0, 0, NULL);
          if (thid >= 0)
            sceKernelStartThread(thid, sizeof(ReceiveArguments), &args);
        }
      } else if (msg_result == MESSAGE_DIALOG_RESULT_NO) {
        adhocSendServerResponse("NO"); // Do not check result
        
        // Go back to searching
        adhocCloseSockets();
        adhocWaitingForServerRequest();
        initMessageDialog(SCE_MSG_DIALOG_BUTTON_TYPE_CANCEL, language_container[ADHOC_RECEIVE_SEARCHING_PSVITA]);
        setDialogStep(DIALOG_STEP_ADHOC_RECEIVE_SEARCHING);
      }

      break;
    }
    
    case DIALOG_STEP_ADHOC_SENDED:
    {
      if (msg_result == MESSAGE_DIALOG_RESULT_FINISHED) {
        refresh = REFRESH_MODE_NORMAL;
        setDialogStep(DIALOG_STEP_NONE);
      }
      
      break;
    }
    
    case DIALOG_STEP_ADHOC_RECEIVED:
    {
      if (msg_result == MESSAGE_DIALOG_RESULT_FINISHED) {
        refresh = REFRESH_MODE_NORMAL;
        setDialogStep(DIALOG_STEP_NONE);
      }
      
      break;
    }
    
    case DIALOG_STEP_ADHOC_SENDING:
    case DIALOG_STEP_ADHOC_RECEIVING:
    {
      if (msg_result == MESSAGE_DIALOG_RESULT_FINISHED) {
        // Alert sockets
        adhocAlertSockets();
      }
      
      break;
    }
  }

  return refresh;
}

static void ftp_log_cb(const char *msg) {
  if (msg) {
    strncpy(ftp_status_msg, msg, 255);
    ftp_status_msg[255] = '\0';
  }
}

void ftpvita_PROM(ftpvita_client_info_t *client) {
  char cmd[64];
  char path[MAX_PATH_LENGTH];
  sscanf(client->recv_buffer, "%s %s", cmd, path);

  if (installPackage(path) == 0) {
    ftpvita_ext_client_send_ctrl_msg(client, "200 OK PROMOTING\r\n");
  } else {
    ftpvita_ext_client_send_ctrl_msg(client, "500 ERROR PROMOTING\r\n");
  }
}

int main(int argc, const char *argv[]) {  
  // Create mutex
  sceKernelCreateLwMutex(&dialog_mutex, "dialog_mutex", 2, 0, NULL);

  // Init VitaShell
  initVitaShell();

  // No custom config, in case they are damaged or unuseable
  readPad();
  if (current_pad[PAD_LTRIGGER])
    use_custom_config = 0;
  
  // Load stuff
  loadSettingsConfig();
  loadTheme();
  loadLanguage(language);

  // Init context menu width
  initContextMenuWidth();
  initTextContextMenuWidth();
  
  // File browser
  browserMain();

  // Finish VitaShell
  finishVitaShell();
  
  return 0;
}

