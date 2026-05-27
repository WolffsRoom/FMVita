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
#include "config.h"
#include "init.h"
#include "theme.h"
#include "language.h"
#include "settings.h"
#include "message_dialog.h"
#include "ime_dialog.h"
#include "utils.h"

static void restartShell();
static void rebootDevice();
static void shutdownDevice();
static void suspendDevice();

static int changed = 0;
static int theme = 0;

static char spoofed_version[6];

static SettingsMenuEntry *settings_menu_entries = NULL;
static int n_settings_entries = 0;

static char *usbdevice_options[4];
static char *select_button_options[2];
static char *bg_anim_options[9];
static char *view_mode_options[3];
static char *theme_preset_options[7];

static char *language_options[19];

static char **theme_options = NULL;
static int theme_count = 0;
static char *theme_name = NULL;

static int settings_touch_active = 0;
static float settings_touch_x = 0, settings_touch_y = 0;
static float settings_touch_start_y = 0;
static float settings_scroll_start = 0;

static float settings_scroll = 0.0f;
static float settings_scroll_target = 0.0f;

static void refreshSettingsLangStrings();

static ConfigEntry settings_entries[] = {
  { "USBDEVICE",          CONFIG_TYPE_DECIMAL, (int *)&vitashell_config.usbdevice },
  { "SELECT_BUTTON",      CONFIG_TYPE_DECIMAL, (int *)&vitashell_config.select_button },
  { "VIEW_MODE",          CONFIG_TYPE_DECIMAL, (int *)&vitashell_config.view_mode },
  { "BACKGROUND_ANIM",    CONFIG_TYPE_DECIMAL, (int *)&vitashell_config.background_anim },
  { "THEME_PRESET",       CONFIG_TYPE_DECIMAL, (int *)&vitashell_config.theme_preset },
  { "LANGUAGE",           CONFIG_TYPE_DECIMAL, (int *)&language },
};

static ConfigEntry theme_entries[] = {
  { "THEME_NAME", CONFIG_TYPE_STRING, (void *)&theme_name },
};

static void refreshLanguageOptions() {
  const char *lang_names[] = {
    "Japanese", "English (US)", "French", "Spanish", "German", "Italian",
    "Dutch", "Portuguese (PT)", "Russian", "Korean", "Chinese (T)", "Chinese (S)",
    "Finnish", "Swedish", "Danish", "Norwegian", "Polish", "Portuguese (BR)", "Turkish"
  };
  int n = sizeof(lang_names) / sizeof(lang_names[0]);
  for (int i = 0; i < n && i < 19; i++)
    language_options[i] = (char *)lang_names[i];
}

SettingsMenuOption main_settings[] = {
  { VITASHELL_SETTINGS_LANGUAGE,        SETTINGS_OPTION_TYPE_OPTIONS, NULL, NULL, 0,
    language_options, 19, &language },

  { VITASHELL_SETTINGS_USBDEVICE,       SETTINGS_OPTION_TYPE_OPTIONS, NULL, NULL, 0,
    usbdevice_options, sizeof(usbdevice_options) / sizeof(char **), &vitashell_config.usbdevice },
  { VITASHELL_SETTINGS_SELECT_BUTTON,   SETTINGS_OPTION_TYPE_OPTIONS, NULL, NULL, 0,
    select_button_options, sizeof(select_button_options) / sizeof(char **), &vitashell_config.select_button },
  { VITASHELL_SETTINGS_THEME,           SETTINGS_OPTION_TYPE_OPTIONS, NULL, NULL, 0,
    NULL, 0, NULL },

  { VITASHELL_SETTINGS_VIEW_MODE,       SETTINGS_OPTION_TYPE_OPTIONS, NULL, NULL, 0,
    view_mode_options, sizeof(view_mode_options) / sizeof(char **), &vitashell_config.view_mode },

  { VITASHELL_SETTINGS_BG_ANIM,         SETTINGS_OPTION_TYPE_OPTIONS, NULL, NULL, 0,
    bg_anim_options, sizeof(bg_anim_options) / sizeof(char **), &vitashell_config.background_anim },
  { VITASHELL_SETTINGS_THEME_PRESET,    SETTINGS_OPTION_TYPE_OPTIONS, NULL, NULL, 0,
    theme_preset_options, sizeof(theme_preset_options) / sizeof(char **), &vitashell_config.theme_preset },

  { VITASHELL_SETTINGS_RESTART_SHELL,   SETTINGS_OPTION_TYPE_CALLBACK, (void *)restartShell, NULL, 0, NULL, 0, NULL },
};

static void restartShell() {
  closeSettingsMenu();
  sceAppMgrLoadExec("app0:eboot.bin", NULL, NULL);
}

static void rebootDevice() {
  scePowerRequestColdReset();
}

static void shutdownDevice() {
  scePowerRequestStandby();
}

static void suspendDevice() {
  scePowerRequestSuspend();
}

static void confirmReboot(SettingsMenuOption **opt) {
  closeSettingsMenu();
  setTouchConfirm(language_container[CONFIRM_REBOOT], rebootDevice, NULL);
}

static void confirmShutdown(SettingsMenuOption **opt) {
  closeSettingsMenu();
  setTouchConfirm(language_container[CONFIRM_POWEROFF], shutdownDevice, NULL);
}

static void confirmSuspend(SettingsMenuOption **opt) {
  closeSettingsMenu();
  setTouchConfirm(language_container[CONFIRM_STANDBY], suspendDevice, NULL);
}

SettingsMenuOption power_settings[] = {
  { VITASHELL_SETTINGS_REBOOT,    SETTINGS_OPTION_TYPE_CALLBACK, (void *)confirmReboot, NULL, 0, NULL, 0, NULL },
  { VITASHELL_SETTINGS_POWEROFF,  SETTINGS_OPTION_TYPE_CALLBACK, (void *)confirmShutdown, NULL, 0, NULL, 0, NULL },
  { VITASHELL_SETTINGS_STANDBY,   SETTINGS_OPTION_TYPE_CALLBACK, (void *)confirmSuspend, NULL, 0, NULL, 0, NULL },
};

SettingsMenuEntry vitashell_settings_menu_entries[] = {
  { VITASHELL_SETTINGS_MAIN,  main_settings,  sizeof(main_settings) / sizeof(SettingsMenuOption) },
  { VITASHELL_SETTINGS_POWER, power_settings, sizeof(power_settings) / sizeof(SettingsMenuOption) },
};

static SettingsMenu settings_menu;

void loadSettingsConfig() {
  memset(&vitashell_config, 0, sizeof(VitaShellConfig));
  readConfig("ux0:FMVita/settings.txt", settings_entries, sizeof(settings_entries) / sizeof(ConfigEntry));
}

void saveSettingsConfig() {
  writeConfig("ux0:FMVita/settings.txt", settings_entries, sizeof(settings_entries) / sizeof(ConfigEntry));

  if (sceKernelGetModel() == SCE_KERNEL_MODEL_VITATV) {
    vitashell_config.select_button = SELECT_BUTTON_MODE_FTP;
  }
}

void initSettingsMenu() {
  memset(&settings_menu, 0, sizeof(SettingsMenu));
  settings_menu.status = SETTINGS_MENU_CLOSED;

  n_settings_entries = sizeof(vitashell_settings_menu_entries) / sizeof(SettingsMenuEntry);
  settings_menu_entries = vitashell_settings_menu_entries;

  for (int i = 0; i < n_settings_entries; i++)
    settings_menu.n_options += settings_menu_entries[i].n_options;

  refreshLanguageOptions();
  refreshSettingsLangStrings();
  
  if (!theme_options) {
    theme_options = malloc(MAX_THEMES * sizeof(char *));
    for (int i = 0; i < MAX_THEMES; i++)
      theme_options[i] = malloc(MAX_THEME_LENGTH);
  }
}

static void refreshSettingsLangStrings() {
  usbdevice_options[0] = language_container[VITASHELL_SETTINGS_USB_MEMORY_CARD];
  usbdevice_options[1] = language_container[VITASHELL_SETTINGS_USB_GAME_CARD];
  usbdevice_options[2] = language_container[VITASHELL_SETTINGS_USB_SD2VITA];
  usbdevice_options[3] = language_container[VITASHELL_SETTINGS_USB_PSVSD];

  select_button_options[0] = language_container[VITASHELL_SETTINGS_SELECT_BUTTON_USB];
  select_button_options[1] = language_container[VITASHELL_SETTINGS_SELECT_BUTTON_FTP];

  bg_anim_options[0] = language_container[BG_ANIM_PARTICLES];
  bg_anim_options[1] = language_container[BG_ANIM_WAVES];
  bg_anim_options[2] = language_container[BG_ANIM_STARS];
  bg_anim_options[3] = language_container[BG_ANIM_SQUARES];
  bg_anim_options[4] = language_container[BG_ANIM_SPARK];
  bg_anim_options[5] = language_container[BG_ANIM_MATRIX];
  bg_anim_options[6] = language_container[BG_ANIM_RAIN];
  bg_anim_options[7] = language_container[BG_ANIM_GIF];
  bg_anim_options[8] = language_container[BG_ANIM_PNG];

  view_mode_options[0] = language_container[VIEW_MODE_LIST];
  view_mode_options[1] = language_container[VIEW_MODE_GRID];
  view_mode_options[2] = language_container[VIEW_MODE_COLUMN];

  theme_preset_options[0] = language_container[THEME_DARK];
  theme_preset_options[1] = language_container[THEME_LIGHT];
  theme_preset_options[2] = language_container[THEME_BLUE];
  theme_preset_options[3] = language_container[THEME_RED];
  theme_preset_options[4] = language_container[THEME_PURPLE];
  theme_preset_options[5] = language_container[THEME_BROWN];
  theme_preset_options[6] = language_container[THEME_GRAY];
}

void openSettingsMenu() {
  settings_menu.status = SETTINGS_MENU_OPENING;
  settings_menu.entry_sel = 0;
  settings_menu.option_sel = 0;

  // Get current theme
  if (theme_name)
    free(theme_name);

  readConfig("ux0:FMVita/theme/theme.txt", theme_entries, sizeof(theme_entries) / sizeof(ConfigEntry));

  // Get theme index in main tab
  int theme_index = -1;

  int i;
  for (i = 0; i < (sizeof(main_settings) / sizeof(SettingsMenuOption)); i++) {
    if (main_settings[i].name == VITASHELL_SETTINGS_THEME) {
      theme_index = i;
      break;
    }
  }

  // Find all themes
  if (theme_index >= 0) {
    SceUID dfd = sceIoDopen("ux0:FMVita/theme");
    if (dfd >= 0) {
      theme_count = 0;
      theme = 0;

      int res = 0;

      do {
        SceIoDirent dir;
        memset(&dir, 0, sizeof(SceIoDirent));

        res = sceIoDread(dfd, &dir);
        if (res > 0) {
          if (SCE_S_ISDIR(dir.d_stat.st_mode)) {
            if (theme_name && strcasecmp(dir.d_name, theme_name) == 0)
              theme = theme_count;
            
            strncpy(theme_options[theme_count], dir.d_name, MAX_THEME_LENGTH);
            theme_count++;
          }
        }
      } while (res > 0 && theme_count < MAX_THEMES);
      
      sceIoDclose(dfd);
      
      main_settings[theme_index].options = theme_options;
      main_settings[theme_index].n_options = theme_count;
      main_settings[theme_index].value = &theme;
    }
  }

  changed = 0;
  settings_scroll = 0.0f;
  settings_scroll_target = 0.0f;
}

void closeSettingsMenu() {
  settings_menu.status = SETTINGS_MENU_CLOSING;

  // Save settings
  if (changed) {
    saveSettingsConfig();
      
    // Save theme config file
    theme_entries[0].value = &theme_options[theme];
    writeConfig("ux0:FMVita/theme/theme.txt", theme_entries, sizeof(theme_entries) / sizeof(ConfigEntry));
    theme_entries[0].value = (void *)&theme_name;
  }
}

int getSettingsMenuStatus() {
  return settings_menu.status;
}

void drawSettingsMenu() {
  if (settings_menu.status == SETTINGS_MENU_CLOSED)
    return;

  // Closing settings menu
  if (settings_menu.status == SETTINGS_MENU_CLOSING) {
    if (settings_menu.cur_pos > 0.0f) {
      settings_menu.cur_pos -= easeOut(0.0f, settings_menu.cur_pos, 0.25f, 0.01f);
    } else {
      settings_menu.status = SETTINGS_MENU_CLOSED;
    }
  }

  // Opening settings menu
  if (settings_menu.status == SETTINGS_MENU_OPENING) {
    if (settings_menu.cur_pos < SCREEN_HEIGHT) {
      settings_menu.cur_pos += easeOut(settings_menu.cur_pos, SCREEN_HEIGHT, 0.25f, 0.01f);
    } else {
      settings_menu.status = SETTINGS_MENU_OPENED;
    }
  }

  // Premium Glassmorphism background
  // Main background overlay (darker)
  vita2d_draw_rectangle(0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, RGBA8(0, 0, 0, (int)(180.0f * (settings_menu.cur_pos / SCREEN_HEIGHT))));
  
  // Settings Window Background
  float y_start = SCREEN_HEIGHT - settings_menu.cur_pos;
  float window_h = settings_menu.cur_pos;
  vita2d_draw_rectangle(0.0f, y_start, SCREEN_WIDTH, window_h, themeDialogBg(vitashell_config.theme_preset));
  
  // Top Border highlight (glow effect)
  vita2d_draw_rectangle(0.0f, y_start, SCREEN_WIDTH, 2.0f, COLOR_ALPHA(themeAccentColor(vitashell_config.theme_preset), 180));
  vita2d_draw_rectangle(0.0f, y_start + 2.0f, SCREEN_WIDTH, 1.0f, COLOR_ALPHA(themeAccentColor(vitashell_config.theme_preset), 60));

  // Calculate scroll
  float total_h = 0.0f;
  for (int i = 0; i < n_settings_entries; i++) {
    total_h += FONT_Y_SPACE; // title
    total_h += settings_menu_entries[i].n_options * FONT_Y_SPACE; // options
    total_h += FONT_Y_SPACE; // gap after entry
  }
  float visible_area = window_h - START_Y - FONT_Y_SPACE;
  if (visible_area < FONT_Y_SPACE) visible_area = FONT_Y_SPACE;
  float max_scroll = (total_h > visible_area) ? total_h - visible_area : 0.0f;

  // Smooth scroll towards target
  settings_scroll += (settings_scroll_target - settings_scroll) * 0.15f;
  if (fabs(settings_scroll - settings_scroll_target) < 0.5f)
    settings_scroll = settings_scroll_target;

  // Clamp
  if (settings_scroll < 0) settings_scroll = 0;
  if (settings_scroll > max_scroll) settings_scroll = max_scroll;
  if (settings_scroll_target < 0) settings_scroll_target = 0;
  if (settings_scroll_target > max_scroll) settings_scroll_target = max_scroll;

  float y = y_start + START_Y - settings_scroll;

  // Content clipping region
  vita2d_set_clip_rectangle(0, y_start + START_Y - 10, SCREEN_WIDTH, visible_area + 20);

  int i;
  for (i = 0; i < n_settings_entries; i++) {
    // Title with thin separator line above
    float sep_y = y + 20;
    if (i > 0) {
      vita2d_draw_rectangle(SHELL_MARGIN_X + 40, sep_y, SCREEN_WIDTH - 2 * (SHELL_MARGIN_X + 40), 1, COLOR_ALPHA(themeTextDim(vitashell_config.theme_preset), 20));
    }
    float tx = pgf_text_width(language_container[settings_menu_entries[i].name]);
    pgf_draw_text(ALIGN_CENTER(SCREEN_WIDTH, tx), y, themeAccentColor(vitashell_config.theme_preset), language_container[settings_menu_entries[i].name]);

    y += FONT_Y_SPACE;

    SettingsMenuOption *options = settings_menu_entries[i].options;

    int j;
    for (j = 0; j < settings_menu_entries[i].n_options; j++) {
      // Focus highlight: full-row accent bar
      if (settings_menu.entry_sel == i && settings_menu.option_sel == j) {
        vita2d_draw_rectangle(SHELL_MARGIN_X, y + 4, MARK_WIDTH, FONT_Y_SPACE - 8, themeSelectionBg(vitashell_config.theme_preset));
        vita2d_draw_rectangle(SHELL_MARGIN_X, y + 4, 3, FONT_Y_SPACE - 8, themeSelectionLine(vitashell_config.theme_preset));
      }

      if (options[j].type == SETTINGS_OPTION_TYPE_CALLBACK) {
        float x = pgf_text_width(language_container[options[j].name]);
        pgf_draw_text(ALIGN_CENTER(SCREEN_WIDTH, x), y, themeButtonSuccess(vitashell_config.theme_preset), language_container[options[j].name]);
      } else {
        float x = pgf_text_width(language_container[options[j].name]);
        pgf_draw_text(ALIGN_RIGHT(SCREEN_HALF_WIDTH - 10.0f, x), y, themeTextColor(vitashell_config.theme_preset), language_container[options[j].name]);

        switch (options[j].type) {
          case SETTINGS_OPTION_TYPE_BOOLEAN:
            pgf_draw_text(SCREEN_HALF_WIDTH + 10.0f, y, themeAccentColor(vitashell_config.theme_preset),
                          (options[j].value && *(options[j].value)) ? language_container[ON] : language_container[OFF]);
            break;

          case SETTINGS_OPTION_TYPE_STRING:
            pgf_draw_text(SCREEN_HALF_WIDTH + 10.0f, y, themeTextDim(vitashell_config.theme_preset), options[j].string);
            break;

          case SETTINGS_OPTION_TYPE_INTEGER:
          {
            int val = options[j].value ? *(options[j].value) : 0;
            char buf[16];
            snprintf(buf, sizeof(buf), "%d", val);
            pgf_draw_text(SCREEN_HALF_WIDTH + 10.0f, y, themeAccentColor(vitashell_config.theme_preset), buf);
            break;
          }

          case SETTINGS_OPTION_TYPE_OPTIONS:
          {
            int value = 0;
            if (options[j].value)
              value = *(options[j].value);
            pgf_draw_text(SCREEN_HALF_WIDTH + 10.0f, y, themeAccentColor(vitashell_config.theme_preset), options[j].options ? options[j].options[value] : "");
            break;
          }
        }
      }

      y += FONT_Y_SPACE;
    }

    y += FONT_Y_SPACE;
  }

  // Restore clip
  vita2d_disable_clipping();

  // Scrollbar
  if (max_scroll > 0) {
    float bar_h = visible_area * (visible_area / total_h);
    if (bar_h < 20) bar_h = 20;
    float bar_y = y_start + START_Y + (settings_scroll / max_scroll) * (visible_area - bar_h);
    // Track
    vita2d_draw_rectangle(SCREEN_WIDTH - 10, y_start + START_Y, 4, visible_area, COLOR_ALPHA(themeTextDim(vitashell_config.theme_preset), 30));
    // Thumb
    vita2d_draw_rectangle(SCREEN_WIDTH - 10, bar_y, 4, bar_h, COLOR_ALPHA(themeAccentColor(vitashell_config.theme_preset), 140));
  }
}

static int agreement = SETTINGS_AGREEMENT_NONE;

void settingsAgree() {
  agreement = SETTINGS_AGREEMENT_AGREE;
}

void settingsDisagree() {
  agreement = SETTINGS_AGREEMENT_DISAGREE;
}

static int hitTestSettingsOption(float ty, int *out_entry, int *out_opt) {
  float y_start = SCREEN_HEIGHT - settings_menu.cur_pos;
  float y = y_start + START_Y - settings_scroll;
  for (int i = 0; i < n_settings_entries; i++) {
    y += FONT_Y_SPACE;
    for (int j = 0; j < settings_menu_entries[i].n_options; j++) {
      if (ty >= y && ty < y + FONT_Y_SPACE) {
        *out_entry = i;
        *out_opt = j;
        return 1;
      }
      y += FONT_Y_SPACE;
    }
    y += FONT_Y_SPACE;
  }
  return 0;
}

void settingsMenuCtrl() {
  // Touch handling
  if (getSettingsMenuStatus() == SETTINGS_MENU_OPENED) {
    if (touch.reportNum > 0) {
      float tx = (touch.report[0].x * 960.0f) / 1920.0f;
      float ty = (touch.report[0].y * 544.0f) / 1088.0f;
      if (!settings_touch_active) {
        settings_touch_active = 1;
        settings_touch_start_y = ty;
        settings_scroll_start = settings_scroll;
        settings_touch_y = ty;
        settings_touch_x = tx;
      } else {
        // Touch drag for scroll
        float dy = ty - settings_touch_start_y;
        settings_scroll_target = settings_scroll_start - dy;
        settings_touch_y = ty;
        settings_touch_x = tx;
      }
    } else if (settings_touch_active) {
      settings_touch_active = 0;
      int entry, opt;
      if (hitTestSettingsOption(settings_touch_y, &entry, &opt)) {
        if (entry == settings_menu.entry_sel && opt == settings_menu.option_sel) {
          pressed_pad[PAD_ENTER] = 1;
        } else {
          settings_menu.entry_sel = entry;
          settings_menu.option_sel = opt;
        }
      }
    }
  }

  SettingsMenuOption *option = &settings_menu_entries[settings_menu.entry_sel].options[settings_menu.option_sel];

  // Agreement
  if (agreement != SETTINGS_AGREEMENT_NONE) {
    agreement = SETTINGS_AGREEMENT_NONE;
  }

  // Change options
  if (pressed_pad[PAD_ENTER] || pressed_pad[PAD_LEFT] || pressed_pad[PAD_RIGHT]) {
    changed = 1;

    switch (option->type) {
      case SETTINGS_OPTION_TYPE_BOOLEAN:
        if (option->value)
          *(option->value) = !*(option->value);
        break;
      
      case SETTINGS_OPTION_TYPE_STRING:
        initImeDialog(language_container[option->name], option->string, option->size_string, SCE_IME_TYPE_EXTENDED_NUMBER, 0, 0);
        setDialogStep(DIALOG_STEP_SETTINGS_STRING);
        break;
        
      case SETTINGS_OPTION_TYPE_CALLBACK:
        if (option->callback)
          option->callback(&option);
        break;
        
      case SETTINGS_OPTION_TYPE_OPTIONS:
      {
        if (option->value) {
          if (pressed_pad[PAD_LEFT]) {
            if (*(option->value) > 0)
              (*(option->value))--;
            else
              *(option->value) = option->n_options - 1;
          } else if (pressed_pad[PAD_ENTER] || pressed_pad[PAD_RIGHT]) {
            if (*(option->value) < option->n_options - 1)
              (*(option->value))++;
            else
              *(option->value) = 0;
          }
        }
        break;
      }

      case SETTINGS_OPTION_TYPE_INTEGER:
      {
        if (option->value) {
          if (pressed_pad[PAD_LEFT]) {
            *(option->value) -= 5;
            if (*(option->value) < 0) *(option->value) = 0;
          } else if (pressed_pad[PAD_ENTER] || pressed_pad[PAD_RIGHT]) {
            *(option->value) += 5;
            if (*(option->value) > 255) *(option->value) = 255;
          }
        }
        break;
      }
    }

    // If language changed, reload UI strings
    if (option->name == VITASHELL_SETTINGS_LANGUAGE) {
      loadLanguage(language);
      refreshSettingsLangStrings();
    }
  }

  // Move
  if (hold_pad[PAD_UP] || hold2_pad[PAD_LEFT_ANALOG_UP]) {
    if (settings_menu.option_sel > 0) {
      settings_menu.option_sel--;
    } else if (settings_menu.entry_sel > 0) {
      settings_menu.entry_sel--;
      settings_menu.option_sel = settings_menu_entries[settings_menu.entry_sel].n_options - 1;
    }
  } else if (hold_pad[PAD_DOWN] || hold2_pad[PAD_LEFT_ANALOG_DOWN]) {
    if (settings_menu.option_sel < settings_menu_entries[settings_menu.entry_sel].n_options - 1) {
      settings_menu.option_sel++;
    } else if (settings_menu.entry_sel < n_settings_entries - 1) {
      settings_menu.entry_sel++;
      settings_menu.option_sel = 0;
    }
  }

  // Auto-scroll to keep selection visible
  if (getSettingsMenuStatus() == SETTINGS_MENU_OPENED || getSettingsMenuStatus() == SETTINGS_MENU_OPENING) {
    float y_start = SCREEN_HEIGHT - settings_menu.cur_pos;
    float visible_area = settings_menu.cur_pos - START_Y - FONT_Y_SPACE;
    float total_h = 0.0f;
    for (int i = 0; i < n_settings_entries; i++) {
      total_h += FONT_Y_SPACE;
      total_h += settings_menu_entries[i].n_options * FONT_Y_SPACE;
      total_h += FONT_Y_SPACE;
    }
    float max_scroll = (total_h > visible_area) ? total_h - visible_area : 0.0f;

    // Calculate Y position of current selection
    float sel_y = FONT_Y_SPACE; // first title
    for (int i = 0; i < settings_menu.entry_sel; i++) {
      sel_y += FONT_Y_SPACE + settings_menu_entries[i].n_options * FONT_Y_SPACE + FONT_Y_SPACE;
    }
    sel_y += FONT_Y_SPACE + settings_menu.option_sel * FONT_Y_SPACE; // title + options

    if (sel_y - settings_scroll_target < 0) {
      settings_scroll_target = sel_y;
    } else if (sel_y - settings_scroll_target + FONT_Y_SPACE > visible_area) {
      settings_scroll_target = sel_y + FONT_Y_SPACE - visible_area;
    }
  }

  // Close
  if (pressed_pad[PAD_START] || pressed_pad[PAD_CANCEL]) {
    closeSettingsMenu();
  }
}

