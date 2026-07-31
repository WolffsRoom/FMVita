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
#include "context_menu.h"
#include "theme.h"
#include "language.h"
#include "utils.h"

static int ctx_menu_mode = CONTEXT_MENU_CLOSED;
static float ctx_cur_menu_width = 0.0f;

static ContextMenu *cur_ctx = NULL;

int getContextMenuMode() {
  return ctx_menu_mode;
}

void setContextMenu(ContextMenu *ctx) {
  cur_ctx = ctx;
}

void setContextMenuMode(int mode) {
  ctx_menu_mode = mode;
}

void drawContextMenu() {
  if (!cur_ctx)
    return;

  // Closing context menu
  if (ctx_menu_mode == CONTEXT_MENU_CLOSING) {
    if (ctx_cur_menu_width > 0.0f) {
      ctx_cur_menu_width -= easeOut(0.0f, ctx_cur_menu_width, 0.375f, 0.5f);
    } else {
      ctx_menu_mode = CONTEXT_MENU_CLOSED;
    }
  }

  // Opening context menu
  if (ctx_menu_mode == CONTEXT_MENU_OPENING) {
    if (ctx_cur_menu_width < cur_ctx->max_width) {
      ctx_cur_menu_width += easeOut(ctx_cur_menu_width, cur_ctx->max_width, 0.375f, 0.5f);
    } else {
      ctx_menu_mode = CONTEXT_MENU_OPENED;
    }
  }

  if (cur_ctx->parent) {
    // Closing context menu 'More'
    if (ctx_menu_mode == CONTEXT_MENU_MORE_CLOSING) {
      if (ctx_cur_menu_width > cur_ctx->parent->max_width) {
        ctx_cur_menu_width -= easeOut(cur_ctx->parent->max_width, ctx_cur_menu_width, 0.375f, 0.5f);
      } else {
        cur_ctx = cur_ctx->parent;
        ctx_menu_mode = CONTEXT_MENU_MORE_CLOSED;
      }
    }

    // Opening context menu 'More'
    if (ctx_menu_mode == CONTEXT_MENU_MORE_OPENING) {
      if (ctx_cur_menu_width < cur_ctx->max_width + cur_ctx->parent->max_width) {
        ctx_cur_menu_width += easeOut(ctx_cur_menu_width, cur_ctx->max_width+cur_ctx->parent->max_width, 0.375f, 0.5f);
      } else {
        ctx_menu_mode = CONTEXT_MENU_MORE_OPENED;
      }
    }
  }

  // Draw context menu
  if (ctx_menu_mode != CONTEXT_MENU_CLOSED) {
    float menu_alpha_ratio = ctx_cur_menu_width / cur_ctx->max_width;
    if (menu_alpha_ratio > 1.0f) menu_alpha_ratio = 1.0f;
    int bg_alpha = (int)(220 * menu_alpha_ratio);
    int text_alpha = (int)(255 * menu_alpha_ratio);
    int overlay_alpha = (int)(140 * menu_alpha_ratio);

    // Full-screen dark overlay
    vita2d_draw_rectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, RGBA8(0, 0, 0, overlay_alpha));

    ContextMenu *ctx = cur_ctx;
    if (ctx->parent)
      ctx = ctx->parent;

    // Count only VISIBLE entries so the menu packs them contiguously.
    // (Fixed .pos slots + invisible gaps used to push entries off-screen.)
    int j, parent_visible = 0, sub_visible = 0;
    for (j = 0; j < ctx->n_entries; j++)
      if (ctx->entries[j].visibility == CTX_VISIBLE) parent_visible++;
    if (cur_ctx->parent) {
      for (j = 0; j < cur_ctx->n_entries; j++)
        if (cur_ctx->entries[j].visibility == CTX_VISIBLE) sub_visible++;
    }
    int rows = parent_visible;
    if (cur_ctx->parent)
      rows = MAX(parent_visible, sub_visible);

    float menu_width = ctx_cur_menu_width + 24.0f;
    float menu_height = rows * FONT_Y_SPACE + 16;
    float start_x = SCREEN_WIDTH - menu_width;
    float start_y = (SCREEN_HEIGHT - menu_height) / 2.0f;
    if (start_y < 2.0f) start_y = 2.0f;

    // Card shadow
    vita2d_draw_rectangle(start_x + 4, start_y + 4, menu_width, menu_height, RGBA8(0, 0, 0, (int)(100 * menu_alpha_ratio)));
    // Card background
    vita2d_draw_rectangle(start_x, start_y, menu_width, menu_height, themeDialogBg(vitashell_config.theme_preset));
    // Subtle border - top edge highlight
    vita2d_draw_rectangle(start_x, start_y, menu_width, 1, COLOR_ALPHA(themeTopbarText(vitashell_config.theme_preset), (int)(25 * menu_alpha_ratio)));
    // Bottom edge accent
    vita2d_draw_rectangle(start_x, start_y + menu_height - 1, menu_width, 1, COLOR_ALPHA(themeAccentColor(vitashell_config.theme_preset), (int)(60 * menu_alpha_ratio)));
    // Left/right subtle borders
    vita2d_draw_rectangle(start_x, start_y, 1, menu_height, COLOR_ALPHA(themeTopbarText(vitashell_config.theme_preset), (int)(10 * menu_alpha_ratio)));
    vita2d_draw_rectangle(start_x + menu_width - 1, start_y, 1, menu_height, COLOR_ALPHA(themeTopbarText(vitashell_config.theme_preset), (int)(10 * menu_alpha_ratio)));

    int i;
    int row = 0;
    for (i = 0; i < ctx->n_entries; i++) {
      if (ctx->entries[i].visibility == CTX_INVISIBLE) continue;
      float y = start_y + 8 + (row * FONT_Y_SPACE);
      row++;

      uint32_t color = COLOR_ALPHA(themeTextColor(vitashell_config.theme_preset), (text_alpha > 255) ? 255 : text_alpha);
      uint32_t bullet = COLOR_ALPHA(themeAccentColor(vitashell_config.theme_preset), (int)(120 * menu_alpha_ratio));

      // Distinguish entry types by color
      int entry_name = ctx->entries[i].name;
      if (entry_name == DELETE || entry_name == CUT) {
        bullet = RGBA8(220, 60, 60, (int)(160 * menu_alpha_ratio));
      } else if (entry_name == COPY || entry_name == PASTE || entry_name == MOVE) {
        bullet = RGBA8(60, 200, 120, (int)(160 * menu_alpha_ratio));
      } else if (entry_name == UNDO_ACTION) {
        bullet = RGBA8(255, 200, 50, (int)(180 * menu_alpha_ratio));
        color = RGBA8(255, 215, 100, text_alpha);
      } else if (entry_name == NEW) {
        bullet = RGBA8(60, 180, 255, (int)(160 * menu_alpha_ratio));
      }

      // Modern selection: full-row accent bar
      if (i == ctx->sel) {
        if (ctx_menu_mode != CONTEXT_MENU_MORE_OPENED && ctx_menu_mode != CONTEXT_MENU_MORE_OPENING) {
          vita2d_draw_rectangle(start_x + 4, y + 2, menu_width - 8, FONT_Y_SPACE - 4, themeSelectionBg(vitashell_config.theme_preset));
          vita2d_draw_rectangle(start_x + 4, y + 2, 3, FONT_Y_SPACE - 4, themeSelectionLine(vitashell_config.theme_preset));
          color = COLOR_ALPHA(themeTopbarText(vitashell_config.theme_preset), (text_alpha > 255) ? 255 : text_alpha);
        }
      }

      // Colored bullet indicator
      vita2d_draw_fill_circle(start_x + 9, y + FONT_Y_SPACE / 2.0f - 6, 3.5f, bullet);
      pgf_draw_text(start_x + 20, y, color, language_container[ctx->entries[i].name]);

      if (ctx->entries[i].flags & CTX_FLAG_MORE) {
        char *arrow = RIGHT_ARROW;
        if (ctx->sel == i && (ctx_menu_mode == CONTEXT_MENU_MORE_OPENED || ctx_menu_mode == CONTEXT_MENU_MORE_OPENING)) arrow = LEFT_ARROW;
        pgf_draw_text(start_x + menu_width - pgf_text_width(arrow) - 12, y, color, arrow);
      }
    }

    // Sub-menu entries
    if (ctx_menu_mode == CONTEXT_MENU_MORE_CLOSING ||
        ctx_menu_mode == CONTEXT_MENU_MORE_OPENED ||
        ctx_menu_mode == CONTEXT_MENU_MORE_OPENING) {
      // Separator line
      vita2d_draw_rectangle(start_x + cur_ctx->parent->max_width + 22, start_y + 8, 1, sub_visible * FONT_Y_SPACE, COLOR_ALPHA(themeTopbarText(vitashell_config.theme_preset), (int)(15 * menu_alpha_ratio)));

      int sub_row = 0;
      for (i = 0; i < cur_ctx->n_entries; i++) {
        if (cur_ctx->entries[i].visibility == CTX_INVISIBLE) continue;
        float y = start_y + 8 + (sub_row * FONT_Y_SPACE);
        sub_row++;
        uint32_t color = COLOR_ALPHA(themeTextColor(vitashell_config.theme_preset), (text_alpha > 255) ? 255 : text_alpha);
        uint32_t bullet = COLOR_ALPHA(themeAccentColor(vitashell_config.theme_preset), (int)(100 * menu_alpha_ratio));

        int entry_name = cur_ctx->entries[i].name;
        if (entry_name == UNDO_ACTION) {
          bullet = RGBA8(255, 200, 50, (int)(160 * menu_alpha_ratio));
          color = RGBA8(255, 215, 100, text_alpha);
        }

        if (i == cur_ctx->sel) {
          if (ctx_menu_mode != CONTEXT_MENU_MORE_CLOSING) {
            vita2d_draw_rectangle(start_x + cur_ctx->parent->max_width + 26, y + 2, menu_width - cur_ctx->parent->max_width - 30, FONT_Y_SPACE - 4, themeSelectionBg(vitashell_config.theme_preset));
            color = COLOR_ALPHA(themeTopbarText(vitashell_config.theme_preset), (text_alpha > 255) ? 255 : text_alpha);
          }
        }

        vita2d_draw_fill_circle(start_x + cur_ctx->parent->max_width + 27, y + FONT_Y_SPACE / 2.0f - 6, 3.5f, bullet);
        pgf_draw_text(start_x + cur_ctx->parent->max_width + 38, y, color, language_container[cur_ctx->entries[i].name]);
      }
    }
  }
}

void contextMenuCtrl() {
  if (!cur_ctx)
    return;

  // Up - single step per press
  if (pressed_pad[PAD_UP]) {
    if (ctx_menu_mode == CONTEXT_MENU_OPENED || ctx_menu_mode == CONTEXT_MENU_MORE_OPENED) {
      int i;
      for (i = cur_ctx->n_entries - 1; i >= 0; i--) {
        if (cur_ctx->entries[i].visibility == CTX_VISIBLE) {
          if (i < cur_ctx->sel) {
            cur_ctx->sel = i;
            break;
          }
        }
      }
    }
  } else if (hold_pad[PAD_UP] || hold2_pad[PAD_LEFT_ANALOG_UP]) {
    // Hold auto-repeat (slower, only after initial press)
    if (ctx_menu_mode == CONTEXT_MENU_OPENED || ctx_menu_mode == CONTEXT_MENU_MORE_OPENED) {
      if (hold_count[PAD_UP] > 5 || hold2_count[PAD_LEFT_ANALOG_UP] > 5) {
        int i;
        for (i = cur_ctx->n_entries - 1; i >= 0; i--) {
          if (cur_ctx->entries[i].visibility == CTX_VISIBLE) {
            if (i < cur_ctx->sel) {
              cur_ctx->sel = i;
              break;
            }
          }
        }
      }
    }
  }

  // Down - single step per press
  if (pressed_pad[PAD_DOWN]) {
    if (ctx_menu_mode == CONTEXT_MENU_OPENED || ctx_menu_mode == CONTEXT_MENU_MORE_OPENED) {
      int i;
      for (i = 0; i < cur_ctx->n_entries; i++) {
        if (cur_ctx->entries[i].visibility == CTX_VISIBLE) {
          if (i > cur_ctx->sel) {
            if (!(cur_ctx->entries[i].flags & CTX_FLAG_BARRIER) || (hold_count[PAD_DOWN] <= 1 && hold2_count[PAD_LEFT_ANALOG_DOWN] <= 1))
              cur_ctx->sel = i;
            break;
          }
        }
      }
    }
  } else if (hold_pad[PAD_DOWN] || hold2_pad[PAD_LEFT_ANALOG_DOWN]) {
    // Hold auto-repeat (slower, only after initial press)
    if (ctx_menu_mode == CONTEXT_MENU_OPENED || ctx_menu_mode == CONTEXT_MENU_MORE_OPENED) {
      if (hold_count[PAD_DOWN] > 5 || hold2_count[PAD_LEFT_ANALOG_DOWN] > 5) {
        int i;
        for (i = 0; i < cur_ctx->n_entries; i++) {
          if (cur_ctx->entries[i].visibility == CTX_VISIBLE) {
            if (i > cur_ctx->sel) {
              if (!(cur_ctx->entries[i].flags & CTX_FLAG_BARRIER) || (hold_count[PAD_DOWN] <= 1 && hold2_count[PAD_LEFT_ANALOG_DOWN] <= 1))
                cur_ctx->sel = i;
              break;
            }
          }
        }
      }
    }
  }

  // Close
  if (pressed_pad[PAD_TRIANGLE]) {
    ctx_menu_mode = CONTEXT_MENU_CLOSING;
  }

  // Back
  if (pressed_pad[PAD_CANCEL] || pressed_pad[PAD_LEFT]) {
    if (ctx_menu_mode == CONTEXT_MENU_MORE_OPENED) {
      ctx_menu_mode = CONTEXT_MENU_MORE_CLOSING;
    } else {
      ctx_menu_mode = CONTEXT_MENU_CLOSING;
    }
  }

  // Handle
  if (pressed_pad[PAD_ENTER] || pressed_pad[PAD_RIGHT]) {
    if (ctx_menu_mode == CONTEXT_MENU_OPENED || ctx_menu_mode == CONTEXT_MENU_MORE_OPENED) {
      if (cur_ctx->callback)
        ctx_menu_mode = cur_ctx->callback(cur_ctx->sel, cur_ctx->context);
    }
  }
}

void contextMenuTouch(int tx, int ty) {
  if (!cur_ctx || ctx_menu_mode == CONTEXT_MENU_CLOSED)
    return;

  ContextMenu *ctx = cur_ctx;
  if (ctx->parent)
    ctx = ctx->parent;

  // Match drawContextMenu(): pack visible entries contiguously
  int j, parent_visible = 0, sub_visible = 0;
  for (j = 0; j < ctx->n_entries; j++)
    if (ctx->entries[j].visibility == CTX_VISIBLE) parent_visible++;
  if (cur_ctx->parent) {
    for (j = 0; j < cur_ctx->n_entries; j++)
      if (cur_ctx->entries[j].visibility == CTX_VISIBLE) sub_visible++;
  }
  int rows = parent_visible;
  if (cur_ctx->parent)
    rows = MAX(parent_visible, sub_visible);

  float menu_width = ctx_cur_menu_width + 24.0f;
  float menu_height = rows * FONT_Y_SPACE + 16;
  float start_x = SCREEN_WIDTH - menu_width;
  float start_y = (SCREEN_HEIGHT - menu_height) / 2.0f;
  if (start_y < 2.0f) start_y = 2.0f;

  // Tapped outside -> close
  if (tx < start_x || tx > SCREEN_WIDTH || ty < start_y || ty > start_y + menu_height) {
    ctx_menu_mode = CONTEXT_MENU_CLOSING;
    return;
  }

  // Determine if click was in parent menu or submenu
  ContextMenu *active_ctx = cur_ctx;
  if (cur_ctx->parent) {
    if (tx >= start_x + cur_ctx->parent->max_width + 22) {
      active_ctx = cur_ctx;
    } else {
      active_ctx = cur_ctx->parent;
    }
  }

  // Find tapped entry by contiguous visible row
  int rel_y = ty - start_y - 8;
  if (rel_y < 0) return;
  int tapped_row = rel_y / (int)FONT_Y_SPACE;

  int i, row = 0, found = -1;
  for (i = 0; i < active_ctx->n_entries; i++) {
    if (active_ctx->entries[i].visibility == CTX_INVISIBLE) continue;
    if (row == tapped_row) { found = i; break; }
    row++;
  }
  if (found < 0) return;

  active_ctx->sel = found;
  if (active_ctx->callback)
    ctx_menu_mode = active_ctx->callback(found, active_ctx->context);
}
