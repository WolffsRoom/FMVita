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
#include "init.h"
#include "theme.h"
#include "language.h"
#include "utils.h"
#include "uncommon_dialog.h"
#include "qr.h"
#include "buttons.h"

typedef struct {
  int dialog_status;
  int status;
  int mode;
  int buttonType;
  int buttonId;
  char msg[512];
  char info[64];
  float x;
  float y;
  float width;
  float height;
  float scale;
  int progress;

  int touch_active;
  float touch_x, touch_y;
  int touch_button_count;
  float touch_btn_x0, touch_btn_y0, touch_btn_x1, touch_btn_y1;
  float touch_btn2_x0, touch_btn2_y0, touch_btn2_x1, touch_btn2_y1;
  int touch_btn1_result, touch_btn2_result;
} UncommonDialog;

static UncommonDialog uncommon_dialog;

static void calculateDialogBoxSize() {
  int len = strlen(uncommon_dialog.msg);
  char *string = uncommon_dialog.msg;
  float text_width = 0;
  
  // Get width and height
  uncommon_dialog.width = 0.0f;
  uncommon_dialog.height = 0.0f;

  int i;
  for (i = 0; i < len + 1; i++) {
    if (uncommon_dialog.msg[i] == '\n') {
      uncommon_dialog.msg[i] = '\0';

      float width = pgf_text_width(string);
      if (width > uncommon_dialog.width)
        uncommon_dialog.width = width;
      
      uncommon_dialog.msg[i] = '\n';

      string = uncommon_dialog.msg + i;
    }

    if (uncommon_dialog.msg[i] == '\0') {
      float width = pgf_text_width(string);
      if (width > uncommon_dialog.width)
        uncommon_dialog.width = width;
    }

    char tmp = uncommon_dialog.msg[i + 1];
    uncommon_dialog.msg[i + 1] = '\0';
    text_width = pgf_text_width(uncommon_dialog.msg);
    uncommon_dialog.msg[i + 1] = tmp;

    if (text_width > UNCOMMON_DIALOG_MAX_WIDTH) {
      int lastSpace = i - 1;
      while (uncommon_dialog.msg[lastSpace] != ' ' && uncommon_dialog.msg[lastSpace] != '\n' && lastSpace > 0)
        lastSpace--;
      
      if (lastSpace == 0 || uncommon_dialog.msg[lastSpace] == '\n') {
        memmove(uncommon_dialog.msg + i + 1, uncommon_dialog.msg + i, strlen(uncommon_dialog.msg) - (i + 1));
        uncommon_dialog.msg[i] = '\n';
      } else {
        uncommon_dialog.msg[lastSpace] = '\n';
      }
      
      uncommon_dialog.width = text_width;
      string = uncommon_dialog.msg + i;
    }    
  }

  uncommon_dialog.height += FONT_Y_SPACE;
  for (i = 0; uncommon_dialog.msg[i]; i++)
    uncommon_dialog.height += FONT_Y_SPACE * (uncommon_dialog.msg[i] == '\n');
  
  // Margin
  uncommon_dialog.width += 2.0f * SHELL_MARGIN_X;
  uncommon_dialog.height += 2.0f * SHELL_MARGIN_Y;

  // Progress bar box width
  if (uncommon_dialog.mode == SCE_MSG_DIALOG_MODE_PROGRESS_BAR) {
    uncommon_dialog.width = UNCOMMON_DIALOG_PROGRESS_BAR_BOX_WIDTH;
    uncommon_dialog.height += 2.0f * FONT_Y_SPACE;
  }
  
  if (uncommon_dialog.mode == MSG_DIALOG_MODE_QR_SCAN) {
    uncommon_dialog.width = CAM_WIDTH + 30;
    uncommon_dialog.height += CAM_HEIGHT;
  }
  
  // More space for buttons
  if (uncommon_dialog.buttonType != SCE_MSG_DIALOG_BUTTON_TYPE_NONE)
    uncommon_dialog.height += 2.0f * FONT_Y_SPACE;

  // Position
  uncommon_dialog.x = ALIGN_CENTER(SCREEN_WIDTH, uncommon_dialog.width);
  uncommon_dialog.y = ALIGN_CENTER(SCREEN_HEIGHT, uncommon_dialog.height);

  // Align
  int y_n = (int)((float)(uncommon_dialog.y - 2.0f) / FONT_Y_SPACE);
  uncommon_dialog.y = (float)y_n * FONT_Y_SPACE + 2.0f;

  // Scale
  uncommon_dialog.scale = 0;
}

int sceMsgDialogInit(const SceMsgDialogParam *param) {
  if (!param)
    return VITASHELL_ERROR_ILLEGAL_ADDR;

  memset(&uncommon_dialog, 0, sizeof(UncommonDialog));

  switch (param->mode) {
    case SCE_MSG_DIALOG_MODE_USER_MSG:
    {
      if (!param->userMsgParam || !param->userMsgParam->msg)
        return VITASHELL_ERROR_ILLEGAL_ADDR;

      strncpy(uncommon_dialog.msg, (char *)param->userMsgParam->msg, sizeof(uncommon_dialog.msg) - 1);
      uncommon_dialog.buttonType = param->userMsgParam->buttonType;
      break;
    }
    
    case SCE_MSG_DIALOG_MODE_PROGRESS_BAR:
    {
      if (!param->progBarParam || !param->progBarParam->msg)
        return VITASHELL_ERROR_ILLEGAL_ADDR;

      strncpy(uncommon_dialog.msg, (char *)param->progBarParam->msg, sizeof(uncommon_dialog.msg) - 1);
      uncommon_dialog.buttonType = SCE_MSG_DIALOG_BUTTON_TYPE_CANCEL;
      break;
    }
    
    case MSG_DIALOG_MODE_QR_SCAN:
    {
      strncpy(uncommon_dialog.msg, (char *)param->userMsgParam->msg, sizeof(uncommon_dialog.msg) - 1);
       uncommon_dialog.buttonType = SCE_MSG_DIALOG_BUTTON_TYPE_CANCEL;
      break;
    }
    
    default:
      return VITASHELL_ERROR_INVALID_ARGUMENT;
  }

  uncommon_dialog.dialog_status = UNCOMMON_DIALOG_OPENING;
  uncommon_dialog.mode = param->mode;
  uncommon_dialog.status = SCE_COMMON_DIALOG_STATUS_RUNNING;

  calculateDialogBoxSize();

  return 0;
}

static int hitTestButton(float tx, float ty, float x0, float y0, float x1, float y1) {
  return (tx >= x0 && tx < x1 && ty >= y0 && ty < y1);
}

void checkDialogTouch() {
  if (uncommon_dialog.status != SCE_COMMON_DIALOG_STATUS_RUNNING ||
      uncommon_dialog.dialog_status != UNCOMMON_DIALOG_OPENED)
    return;

  if (touch.reportNum > 0) {
    float tx = (touch.report[0].x * 960.0f) / 1920.0f;
    float ty = (touch.report[0].y * 544.0f) / 1088.0f;
    if (!uncommon_dialog.touch_active) {
      uncommon_dialog.touch_active = 1;
      uncommon_dialog.touch_x = tx;
      uncommon_dialog.touch_y = ty;
    }
  } else if (uncommon_dialog.touch_active) {
    uncommon_dialog.touch_active = 0;
    float tx = uncommon_dialog.touch_x;
    float ty = uncommon_dialog.touch_y;

    int hit_first = 0, hit_second = 0;
    if (uncommon_dialog.touch_button_count >= 1 &&
        hitTestButton(tx, ty,
          uncommon_dialog.touch_btn_x0, uncommon_dialog.touch_btn_y0,
          uncommon_dialog.touch_btn_x1, uncommon_dialog.touch_btn_y1)) {
      hit_first = 1;
    } else if (uncommon_dialog.touch_button_count >= 2 &&
        hitTestButton(tx, ty,
          uncommon_dialog.touch_btn2_x0, uncommon_dialog.touch_btn2_y0,
          uncommon_dialog.touch_btn2_x1, uncommon_dialog.touch_btn2_y1)) {
      hit_second = 1;
    }

    if (hit_first || hit_second) {
      switch (uncommon_dialog.buttonType) {
        case SCE_MSG_DIALOG_BUTTON_TYPE_OK:
          pressed_pad[PAD_ENTER] = 1;
          break;
        case SCE_MSG_DIALOG_BUTTON_TYPE_YESNO:
          if (hit_first)
            pressed_pad[PAD_ENTER] = 1;
          else
            pressed_pad[PAD_CANCEL] = 1;
          break;
        case SCE_MSG_DIALOG_BUTTON_TYPE_OK_CANCEL:
          if (hit_first)
            pressed_pad[PAD_ENTER] = 1;
          else
            pressed_pad[PAD_CANCEL] = 1;
          break;
        case SCE_MSG_DIALOG_BUTTON_TYPE_CANCEL:
          pressed_pad[PAD_CANCEL] = 1;
          break;
      }
    }
  }
}

SceCommonDialogStatus sceMsgDialogGetStatus(void) {
  if (uncommon_dialog.status == SCE_COMMON_DIALOG_STATUS_RUNNING &&
      uncommon_dialog.dialog_status == UNCOMMON_DIALOG_OPENED) {

    switch (uncommon_dialog.buttonType) {
      case SCE_MSG_DIALOG_BUTTON_TYPE_OK:
      {
        if (pressed_pad[PAD_ENTER]) {
          uncommon_dialog.dialog_status = UNCOMMON_DIALOG_CLOSING;
          uncommon_dialog.buttonId = SCE_MSG_DIALOG_BUTTON_ID_OK;
        }

        break;
      }
      
      case SCE_MSG_DIALOG_BUTTON_TYPE_YESNO:
      {
        if (pressed_pad[PAD_ENTER]) {
          uncommon_dialog.dialog_status = UNCOMMON_DIALOG_CLOSING;
          uncommon_dialog.buttonId = SCE_MSG_DIALOG_BUTTON_ID_YES;
        }

        if (pressed_pad[PAD_CANCEL]) {
          uncommon_dialog.dialog_status = UNCOMMON_DIALOG_CLOSING;
          uncommon_dialog.buttonId = SCE_MSG_DIALOG_BUTTON_ID_NO;
        }

        break;
      }
      
      case SCE_MSG_DIALOG_BUTTON_TYPE_OK_CANCEL:
      {
        if (pressed_pad[PAD_ENTER]) {
          uncommon_dialog.dialog_status = UNCOMMON_DIALOG_CLOSING;
          uncommon_dialog.buttonId = SCE_MSG_DIALOG_BUTTON_ID_YES;
        }

        if (pressed_pad[PAD_CANCEL]) {
          uncommon_dialog.dialog_status = UNCOMMON_DIALOG_CLOSING;
          uncommon_dialog.buttonId = SCE_MSG_DIALOG_BUTTON_ID_NO;
        }

        break;
      }
      
      case SCE_MSG_DIALOG_BUTTON_TYPE_CANCEL:
      {
        if (pressed_pad[PAD_CANCEL]) {
          uncommon_dialog.dialog_status = UNCOMMON_DIALOG_CLOSING;
        }

        break;
      }
    }
  }

  return uncommon_dialog.status;
}

int sceMsgDialogClose(void) {
  if (uncommon_dialog.status != SCE_COMMON_DIALOG_STATUS_RUNNING)
    return VITASHELL_ERROR_NOT_RUNNING;

  uncommon_dialog.dialog_status = UNCOMMON_DIALOG_CLOSING;
  return 0;
}

int sceMsgDialogTerm(void) {
  if (uncommon_dialog.status == SCE_COMMON_DIALOG_STATUS_NONE)
    return VITASHELL_ERROR_NOT_RUNNING;

  uncommon_dialog.status = SCE_COMMON_DIALOG_STATUS_NONE;
  return 0;
}

int sceMsgDialogGetResult(SceMsgDialogResult *result) {
  if (!result)
    return VITASHELL_ERROR_ILLEGAL_ADDR;

  result->buttonId = uncommon_dialog.buttonId;
  return 0;
}

int sceMsgDialogProgressBarSetInfo(SceMsgDialogProgressBarTarget target, const SceChar8 *barInfo) {
  strncpy(uncommon_dialog.info, (char *)barInfo, sizeof(uncommon_dialog.info) - 1);
  return 0;
}

int sceMsgDialogProgressBarSetMsg(SceMsgDialogProgressBarTarget target, const SceChar8 *barMsg) {
  strncpy(uncommon_dialog.msg, (char *)barMsg, sizeof(uncommon_dialog.msg) - 1);
  return 0;
}

int sceMsgDialogProgressBarSetValue(SceMsgDialogProgressBarTarget target, SceUInt32 rate) {
  if (rate > 100)
    return VITASHELL_ERROR_INVALID_ARGUMENT;

  uncommon_dialog.progress = rate;
  return 0;
}

int drawUncommonDialog() {
  if (uncommon_dialog.status == SCE_COMMON_DIALOG_STATUS_NONE)
    return 0;

  // Dialog background
  vita2d_draw_texture_scale_rotate_hotspot(dialog_image, uncommon_dialog.x + uncommon_dialog.width / 2.0f,
                                           uncommon_dialog.y + uncommon_dialog.height / 2.0f,
                                           uncommon_dialog.scale * (uncommon_dialog.width / vita2d_texture_get_width(dialog_image)),
                                           uncommon_dialog.scale * (uncommon_dialog.height / vita2d_texture_get_height(dialog_image)),
                                           0.0f, vita2d_texture_get_width(dialog_image) / 2.0f, vita2d_texture_get_height(dialog_image) / 2.0f);

  // Easing out
  if (uncommon_dialog.dialog_status == UNCOMMON_DIALOG_CLOSING) {
    if (uncommon_dialog.scale > 0.0f) {
      uncommon_dialog.scale -= easeOut(0.0f, uncommon_dialog.scale, 0.25f, 0.01f);
    } else {
      uncommon_dialog.dialog_status = UNCOMMON_DIALOG_CLOSED;
      uncommon_dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
    }
  }

  if (uncommon_dialog.dialog_status == UNCOMMON_DIALOG_OPENING) {
    if (uncommon_dialog.scale < 1.0f) {
      uncommon_dialog.scale += easeOut(uncommon_dialog.scale, 1.0f, 0.25f, 0.01f);
    } else {
      uncommon_dialog.dialog_status = UNCOMMON_DIALOG_OPENED;
    }
  }

  if (uncommon_dialog.dialog_status == UNCOMMON_DIALOG_OPENED) {
    float string_y = uncommon_dialog.y + SHELL_MARGIN_Y - 2.0f;

    // Draw info
    if (uncommon_dialog.mode == SCE_MSG_DIALOG_MODE_PROGRESS_BAR) {
      if (uncommon_dialog.info[0] != '\0') {
        float x = ALIGN_RIGHT(uncommon_dialog.x + uncommon_dialog.width - SHELL_MARGIN_X, pgf_text_width(uncommon_dialog.info));
        pgf_draw_text(x, string_y, DIALOG_COLOR, uncommon_dialog.info);
      }
    }

    // Draw message
    int len = strlen(uncommon_dialog.msg);
    char *string = uncommon_dialog.msg;

    int i;
    for (i = 0; i < len + 1; i++) {
      if (uncommon_dialog.msg[i] == '\n') {
        uncommon_dialog.msg[i] = '\0';
        pgf_draw_text(uncommon_dialog.x + SHELL_MARGIN_X, string_y, DIALOG_COLOR, string);
        uncommon_dialog.msg[i] = '\n';

        string = uncommon_dialog.msg+i + 1;
        string_y += FONT_Y_SPACE;
      }

      if (uncommon_dialog.msg[i] == '\0') {
        pgf_draw_text(uncommon_dialog.x + SHELL_MARGIN_X, string_y, DIALOG_COLOR, string);
        string_y += FONT_Y_SPACE;
      }
    }
    
    // Dialog type - build label strings with button positions
    int enter_btn = (enter_button == SCE_SYSTEM_PARAM_ENTER_BUTTON_CIRCLE) ? BUTTON_CIRCLE : BUTTON_CROSS;
    int cancel_btn = (enter_button == SCE_SYSTEM_PARAM_ENTER_BUTTON_CIRCLE) ? BUTTON_CROSS : BUTTON_CIRCLE;
    int btn_gap = 4;
    int btn_label_gap = 6;

    char btn_string1[64] = "", btn_string2[64] = "";
    int has_first = 0, has_second = 0;
    int first_btn = 0, second_btn = 0;

    switch (uncommon_dialog.buttonType) {
      case SCE_MSG_DIALOG_BUTTON_TYPE_OK:
        strcpy(btn_string1, language_container[OK]);
        first_btn = enter_btn;
        has_first = 1;
        break;
      
      case SCE_MSG_DIALOG_BUTTON_TYPE_YESNO:
        strcpy(btn_string1, language_container[YES]);
        first_btn = enter_btn;
        has_first = 1;
        strcpy(btn_string2, language_container[NO]);
        second_btn = cancel_btn;
        has_second = 1;
        break;
        
      case SCE_MSG_DIALOG_BUTTON_TYPE_OK_CANCEL:
        strcpy(btn_string1, language_container[OK]);
        first_btn = enter_btn;
        has_first = 1;
        strcpy(btn_string2, language_container[CANCEL]);
        second_btn = cancel_btn;
        has_second = 1;
        break;
        
      case SCE_MSG_DIALOG_BUTTON_TYPE_CANCEL:
        strcpy(btn_string1, language_container[CANCEL]);
        first_btn = cancel_btn;
        has_first = 1;
        break;
    }

    // Progress bar
    if (uncommon_dialog.mode == SCE_MSG_DIALOG_MODE_PROGRESS_BAR) {
      float width = uncommon_dialog.width - 2.0f * SHELL_MARGIN_X;
      float x = uncommon_dialog.x + SHELL_MARGIN_X;
      vita2d_draw_rectangle(x, string_y + 10.0f, width, UNCOMMON_DIALOG_PROGRESS_BAR_HEIGHT, PROGRESS_BAR_BG_COLOR);
      vita2d_draw_rectangle(x, string_y + 10.0f, uncommon_dialog.progress * width / 100.0f, UNCOMMON_DIALOG_PROGRESS_BAR_HEIGHT, PROGRESS_BAR_COLOR);

      char string[8];
      sprintf(string, "%d%%", uncommon_dialog.progress);
      pgf_draw_text(ALIGN_CENTER(SCREEN_WIDTH, pgf_text_width(string)), string_y + FONT_Y_SPACE, DIALOG_COLOR, string);

      string_y += 2.0f * FONT_Y_SPACE;
    }
    
    if (uncommon_dialog.mode == MSG_DIALOG_MODE_QR_SCAN) {
      renderCameraQR(uncommon_dialog.x + 15, string_y + 10);
      string_y += CAM_HEIGHT;
    }

    uncommon_dialog.touch_button_count = 0;

    if (has_first || has_second) {
      float total_w = 0;
      float first_w = 0, second_w = 0;
      if (has_first) first_w = BUTTON_SIZE + btn_label_gap + pgf_text_width(btn_string1);
      if (has_second) second_w = BUTTON_SIZE + btn_label_gap + pgf_text_width(btn_string2);
      total_w = first_w + (has_second ? btn_gap + second_w : 0);
      float bx = (SCREEN_WIDTH - total_w) / 2.0f;
      float by = string_y + FONT_Y_SPACE;
      float bh = FONT_Y_SPACE;

      if (has_first) {
        float bw = first_w + 10;
        uncommon_dialog.touch_btn_x0 = bx - 5;
        uncommon_dialog.touch_btn_y0 = by - 4;
        uncommon_dialog.touch_btn_x1 = bx + bw;
        uncommon_dialog.touch_btn_y1 = by + bh;
        uncommon_dialog.touch_btn1_result = (uncommon_dialog.buttonType == SCE_MSG_DIALOG_BUTTON_TYPE_OK) ? SCE_MSG_DIALOG_BUTTON_ID_OK : SCE_MSG_DIALOG_BUTTON_ID_YES;
        uncommon_dialog.touch_button_count = 1;

        drawButton(first_btn, bx, by + 1);
        pgf_draw_text(bx + BUTTON_SIZE + btn_label_gap, by, DIALOG_COLOR, btn_string1);
        bx += first_w + btn_gap;
      }
      if (has_second) {
        float bw = second_w + 10;
        uncommon_dialog.touch_btn2_x0 = bx - 5;
        uncommon_dialog.touch_btn2_y0 = by - 4;
        uncommon_dialog.touch_btn2_x1 = bx + bw;
        uncommon_dialog.touch_btn2_y1 = by + bh;
        uncommon_dialog.touch_btn2_result = (uncommon_dialog.buttonType == SCE_MSG_DIALOG_BUTTON_TYPE_YESNO) ? SCE_MSG_DIALOG_BUTTON_ID_NO : SCE_MSG_DIALOG_BUTTON_ID_NO;
        uncommon_dialog.touch_button_count = 2;

        drawButton(second_btn, bx, by + 1);
        pgf_draw_text(bx + BUTTON_SIZE + btn_label_gap, by, DIALOG_COLOR, btn_string2);
      }
    }
  }

  return 0;
}
