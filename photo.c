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
#include "browser.h"
#include "archive.h"
#include "photo.h"
#include "file.h"
#include "language.h"
#include "theme.h"
#include "utils.h"
#include "uncommon_dialog.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"

static vita2d_texture *loadImage(const char *file, int type, char *buffer) {
  vita2d_texture *tex = NULL;

  int w, h, channels;
  unsigned char *img_data = NULL;
  
  if (isInArchive()) {
    int size = ReadArchiveFile(file, buffer, BIG_BUFFER_SIZE);
    if (size > 0) {
       img_data = stbi_load_from_memory((unsigned char*)buffer, size, &w, &h, &channels, 4);
    }
  } else {
    img_data = stbi_load(file, &w, &h, &channels, 4);
  }

  if (!img_data) return NULL;

  if (w > 960 || h > 544) {
      int new_w = 960;
      int new_h = (h * 960) / w;
      if (new_h > 544) {
          new_h = 544;
          new_w = (w * 544) / h;
      }
      unsigned char *resized_data = malloc(new_w * new_h * 4);
      if (resized_data) {
          stbir_resize_uint8_linear(img_data, w, h, 0, resized_data, new_w, new_h, 0, (stbir_pixel_layout)4);
          stbi_image_free(img_data);
          img_data = resized_data;
          w = new_w;
          h = new_h;
      }
  }

  tex = vita2d_create_empty_texture_format(w, h, SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR);
  if (tex) {
      void *tex_data = vita2d_texture_get_datap(tex);
      memcpy(tex_data, img_data, w * h * 4);
      vita2d_texture_set_filters(tex, SCE_GXM_TEXTURE_FILTER_LINEAR, SCE_GXM_TEXTURE_FILTER_LINEAR);
  }

  free(img_data);

  return tex;
}

static int isHorizontal(float rad) {
  return ((int)sinf(rad) == 0) ? 1 : 0;
}

static void photoMode(float *zoom, float width, float height, float rad, int mode) {
  int horizontal = isHorizontal(rad);
  float h = horizontal ? height : width;
  float w = horizontal ? width : height;

  switch (mode) {
    case MODE_CUSTOM:
      break;
      
    case MODE_PERFECT: // this is only used for showing image the first time
      if (h > SCREEN_HEIGHT) { // first priority, fit height
        *zoom = SCREEN_HEIGHT / h;
      } else if (w > SCREEN_WIDTH) { // second priority, fit screen
        *zoom = SCREEN_WIDTH / w;
      } else { // otherwise, original size
        *zoom = 1.0f;
      }

      break;
    
    case MODE_ORIGINAL:
      *zoom = 1.0f;
      break;
      
    case MODE_FIT_HEIGHT:
      *zoom = SCREEN_HEIGHT / h;
      break;
      
    case MODE_FIT_WIDTH:
      *zoom = SCREEN_WIDTH / w;
      break;
  }
}

static int getNextZoomMode(float *zoom, float width, float height, float rad, int mode) {
  float next_zoom = ZOOM_MAX, smallest_zoom = ZOOM_MAX;;
  int next_mode = MODE_ORIGINAL, smallest_mode = MODE_ORIGINAL;

  int i = 0;
  while (i < 3) {
    if (mode == MODE_CUSTOM || mode == MODE_PERFECT || mode == MODE_FIT_WIDTH) {
      mode = MODE_ORIGINAL;
    } else {
      mode++;
    }

    float new_zoom = 0.0f;
    photoMode(&new_zoom, width, height, rad, mode);

    if (new_zoom < smallest_zoom) {
      smallest_zoom = new_zoom;
      smallest_mode = mode;
    }

    if (new_zoom > *zoom && new_zoom < next_zoom) {
      next_zoom = new_zoom;
      next_mode = mode;
    }

    i++;
  }

  // Get smallest then
  if (next_zoom == ZOOM_MAX) {
    next_zoom = smallest_zoom;
    next_mode = smallest_mode;
  }

  *zoom = next_zoom;
  return next_mode;
}

static void resetImageInfo(vita2d_texture *tex, float *width, float *height, float *x, float *y, float *rad, float *zoom, int *mode, uint64_t *time) {
  *width = vita2d_texture_get_width(tex);
  *height = vita2d_texture_get_height(tex);

  *x = *width/2.0f;
  *y = *height/2.0f;

  *rad = 0;
  *zoom = 1.0f;

  *mode = MODE_PERFECT;
  photoMode(zoom, *width, *height, *rad, *mode);

  *time = 0;
}

int photoViewer(const char *file, int type, FileList *list, FileListEntry *entry, int *base_pos, int *rel_pos) {
  char *buffer = memalign(4096, BIG_BUFFER_SIZE);
  if (!buffer)
    return VITASHELL_ERROR_NO_MEMORY;

  vita2d_texture *tex = loadImage(file, type, buffer);
  if (!tex) {
    free(buffer);
    return VITASHELL_ERROR_NO_MEMORY;
  }

  // Variables
  float width = 0.0f, height = 0.0f, x = 0.0f, y = 0.0f, rad = 0.0f, zoom = 1.0f;
  int mode = MODE_PERFECT;
  uint64_t time = 0;

  // Reset image
  resetImageInfo(tex, &width, &height, &x, &y, &rad, &zoom, &mode, &time);

  // Touch state for photo viewer
  int photo_touch_active = 0;
  float photo_touch_sx = 0.0f, photo_touch_sy = 0.0f;
  float photo_touch_pinch_dist = 0.0f;
  float photo_touch_pinch_zoom = 1.0f;
  float photo_touch_pinch_x = 0.0f, photo_touch_pinch_y = 0.0f;

  // Helper for centered text drawing
  #define photo_center_text(x, y, col, txt) \
    do { const char *_t = (txt); if (_t) pgf_draw_text((x) - pgf_text_width(_t) / 2.0f, (y), (col), _t); } while(0)

  // Swipe & toolbar
  int mirrored = 0;
  int photo_swipe_active = 0;
  float photo_swipe_sx = 0.0f, photo_swipe_sy = 0.0f;
  float photo_last_tx = 0.0f, photo_last_ty = 0.0f;
  uint64_t photo_swipe_time = 0;
  const char *fname = strrchr(file, '/');
  if (fname) fname++; else fname = file;

  char current_file[MAX_PATH_LENGTH];
  strncpy(current_file, file, MAX_PATH_LENGTH - 1);
  current_file[MAX_PATH_LENGTH - 1] = '\0';

  while (1) {
    readPad();

    // Touch handling for pan, pinch zoom, swipe, and toolbar
    if (touch.reportNum > 0) {
      int tx = (touch.report[0].x * 960) / 1920;
      int ty = (touch.report[0].y * 544) / 1088;
      if (!photo_touch_active) {
        photo_touch_active = 1;
        photo_touch_sx = tx;
        photo_touch_sy = ty;
        photo_touch_pinch_dist = 0.0f;
        photo_swipe_active = 1;
        photo_swipe_sx = tx;
        photo_swipe_sy = ty;
        photo_swipe_time = sceKernelGetProcessTimeWide();
        photo_last_tx = tx;
        photo_last_ty = ty;
        if (touch.reportNum > 1) {
          int tx2 = (touch.report[1].x * 960) / 1920;
          int ty2 = (touch.report[1].y * 544) / 1088;
          float dx = tx2 - tx;
          float dy = ty2 - ty;
          photo_touch_pinch_dist = sqrtf(dx*dx + dy*dy);
          photo_touch_pinch_zoom = zoom;
          photo_touch_pinch_x = x;
          photo_touch_pinch_y = y;
          photo_swipe_active = 0;
        }
      } else {
        // Track drift for swipe detection
        float drift_y = fabsf(ty - photo_swipe_sy);
        if (drift_y > 60.0f || touch.reportNum > 1) photo_swipe_active = 0;
        photo_last_tx = tx;
        photo_last_ty = ty;

        // Two-finger pinch
        if (touch.reportNum > 1 && photo_touch_pinch_dist > 0.0f) {
          int tx2 = (touch.report[1].x * 960) / 1920;
          int ty2 = (touch.report[1].y * 544) / 1088;
          float dx = tx2 - tx;
          float dy = ty2 - ty;
          float dist = sqrtf(dx*dx + dy*dy);
          float scale = dist / photo_touch_pinch_dist;
          mode = MODE_CUSTOM;
          zoom = photo_touch_pinch_zoom * scale;
          if (zoom < ZOOM_MIN) zoom = ZOOM_MIN;
          if (zoom > ZOOM_MAX) zoom = ZOOM_MAX;
          time = sceKernelGetProcessTimeWide();
        } else if (touch.reportNum == 1) {
          // Pan (middle area, between top bar and bottom buttons)
          if (ty >= 45 && ty < SCREEN_HEIGHT - 50) {
            float dx = (tx - photo_touch_sx);
            float dy = (ty - photo_touch_sy);
            int horizontal = isHorizontal(rad);
            if (horizontal) {
              x -= cosf(rad) * dx / zoom;
              y -= cosf(rad) * dy / zoom;
            } else {
              y -= sinf(rad) * dx / zoom;
              x -= sinf(rad) * dy / zoom;
            }
            photo_touch_sx = tx;
            photo_touch_sy = ty;
          }
        }
      }
    } else if (photo_touch_active) {
      float tx = photo_last_tx, ty = photo_last_ty;
      // Toolbar button taps (bottom region)
      int btn_region_y = SCREEN_HEIGHT - 50;
      if (ty >= btn_region_y) {
        int btn_w = 110, btn_gap = 8;
        int total_btn_w = 3 * btn_w + 2 * btn_gap;
        int btn_start_x = (SCREEN_WIDTH - total_btn_w) / 2;
        int btn_idx = -1;
        if (tx >= btn_start_x && tx < btn_start_x + btn_w) btn_idx = 0;
        else if (tx >= btn_start_x + (btn_w + btn_gap) && tx < btn_start_x + 2 * btn_w + btn_gap) btn_idx = 1;
        else if (tx >= btn_start_x + 2 * (btn_w + btn_gap) && tx < btn_start_x + 3 * btn_w + 2 * btn_gap) btn_idx = 2;

        if (btn_idx == 0) {
          // Copy file to clipboard
          fileListEmpty(&copy_list);
          FileListEntry *ce = fileListCopyEntry(entry);
          if (ce) {
            fileListAddEntry(&copy_list, ce, SORT_NONE);
            strcpy(copy_list.path, list->path);
            copy_list.is_in_archive = 0;
            copy_mode = 0;
          }
        } else if (btn_idx == 1) {
          // Delete file
          sceIoRemove(file);
          break;
        } else if (btn_idx == 2) {
          mirrored = !mirrored;
        }
      } else {
        // Check for swipe on release (middle/top area)
        if (photo_swipe_active) {
          float dx = tx - photo_swipe_sx;
          float dy = fabsf(ty - photo_swipe_sy);
          if (dy < 60.0f) {
            if (dx > 150.0f) {
              pressed_pad[PAD_LEFT] = 1;
            } else if (dx < -150.0f) {
              pressed_pad[PAD_RIGHT] = 1;
            }
          }
        }
      }
      photo_touch_active = 0;
    }

    // Cancel
    if (pressed_pad[PAD_CANCEL]) {
      break;
    }

    // Previous/next image.
    if (pressed_pad[PAD_LEFT] || pressed_pad[PAD_RIGHT]) {
      int available = 0;

      int old_base_pos = *base_pos;
      int old_rel_pos = *rel_pos;
      FileListEntry *old_entry = entry;
      char old_file[MAX_PATH_LENGTH];
      strncpy(old_file, current_file, MAX_PATH_LENGTH);
      old_file[MAX_PATH_LENGTH - 1] = '\0';

      int previous = pressed_pad[PAD_LEFT];
      while (previous ? entry->previous : entry->next) {
        entry = previous ? entry->previous : entry->next;

        if (previous) {
          if (*rel_pos > 0) {
            (*rel_pos)--;
          } else if (*base_pos > 0) {
            (*base_pos)--;
          }
        } else {
          if ((*rel_pos + 1) < list->length) {
            if ((*rel_pos + 1) < MAX_POSITION) {
              (*rel_pos)++;
            } else if ((*base_pos + *rel_pos + 1) < list->length) {
              (*base_pos)++;
            }
          }
        }

        if (!entry->is_folder) {
          char path[MAX_PATH_LENGTH];
          snprintf(path, MAX_PATH_LENGTH, "%s%s", list->path, entry->name);
          int type = getFileType(path);
          if (type == FILE_TYPE_BMP || type == FILE_TYPE_JPEG || type == FILE_TYPE_PNG || type == FILE_TYPE_GIF) {
            vita2d_wait_rendering_done();
            vita2d_free_texture(tex);

            vita2d_texture *new_tex = loadImage(path, type, buffer);
            if (!new_tex) {
              // Restore previous entry and image
              entry = old_entry;
              *base_pos = old_base_pos;
              *rel_pos = old_rel_pos;
              strncpy(current_file, old_file, MAX_PATH_LENGTH);
              tex = loadImage(current_file, type, buffer);
              if (!tex) {
                free(buffer);
                return VITASHELL_ERROR_NO_MEMORY;
              }
              resetImageInfo(tex, &width, &height, &x, &y, &rad, &zoom, &mode, &time);
              available = 0;
              break;
            }
            tex = new_tex;
            strncpy(current_file, path, MAX_PATH_LENGTH);

            // Reset image
            resetImageInfo(tex, &width, &height, &x, &y, &rad, &zoom, &mode, &time);
            available = 1;
            break;
          }
        }
      }

      if (!available) {
        *base_pos = old_base_pos;
        *rel_pos = old_rel_pos;
        entry = old_entry;
        strncpy(current_file, old_file, MAX_PATH_LENGTH);
      }
    }

    // Photo mode
    if (pressed_pad[PAD_ENTER]) {
      time = sceKernelGetProcessTimeWide();

      x = width / 2.0f;
      y = height / 2.0f;

      // Find next mode
      mode = getNextZoomMode(&zoom, width, height, rad, mode);
    }

    // Rotate
    if (pressed_pad[PAD_LTRIGGER]) {
      rad -= M_PI_2;
      if (rad < 0)
        rad += M_TWOPI;

      photoMode(&zoom, width, height, rad, mode);
    } else if (pressed_pad[PAD_RTRIGGER]) {
      rad += M_PI_2;
      if (rad >= M_TWOPI)
        rad -= M_TWOPI;

      photoMode(&zoom, width, height, rad, mode);
    }

    // Zoom
    if (current_pad[PAD_RIGHT_ANALOG_DOWN]) {
      time = sceKernelGetProcessTimeWide();
      mode = MODE_CUSTOM;
      zoom /= ZOOM_FACTOR;
    } else if (current_pad[PAD_RIGHT_ANALOG_UP]) {
      time = sceKernelGetProcessTimeWide();
      mode = MODE_CUSTOM;
      zoom *= ZOOM_FACTOR;
    }

    if (zoom < ZOOM_MIN) {
      zoom = ZOOM_MIN;
    }

    if (zoom > ZOOM_MAX) {
      zoom = ZOOM_MAX;
    }

    // Move
    if (pad.lx < (ANALOG_CENTER - ANALOG_SENSITIVITY) || pad.lx > (ANALOG_CENTER + ANALOG_SENSITIVITY)) {
      float d = ((pad.lx - ANALOG_CENTER) / MOVE_DIVISION) / zoom;

      if (isHorizontal(rad)) {
        x += cosf(rad) * d;
      } else {
        y += -sinf(rad) * d;
      }
    }

    if (pad.ly < (ANALOG_CENTER - ANALOG_SENSITIVITY) || pad.ly > (ANALOG_CENTER + ANALOG_SENSITIVITY)) {
      float d = ((pad.ly - ANALOG_CENTER) / MOVE_DIVISION) / zoom;

      if (isHorizontal(rad)) {
        y += cosf(rad) * d;
      } else {
        x += sinf(rad) * d;
      }
    }

    // Limit
    int horizontal = isHorizontal(rad);
    float w = horizontal ? SCREEN_HALF_WIDTH : SCREEN_HALF_HEIGHT;
    float h = horizontal ? SCREEN_HALF_HEIGHT : SCREEN_HALF_WIDTH;

    if ((zoom *  width) > 2.0f * w) {
      if (x < (w / zoom)) {
        x = w / zoom;
      } else if (x > (width - w / zoom)) {
        x = width - w / zoom;
      }
    } else {
      x = width / 2.0f;
    }

    if ((zoom * height) > 2.0f * h) {
      if (y < (h / zoom)) {
        y = h / zoom;
      } else if (y > (height - h / zoom)) {
        y = height - h/zoom;
      }
    } else {
      y = height / 2.0f;
    }

    // Start drawing
    startDrawing(NULL); // overlay

    static float photo_wave_time = 0.0f;
    photo_wave_time += 0.018f;
    
    if (vitashell_config.background_anim >= 7) {
      drawGifBackground();
    } else {
      drawGifBackground();
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
          float x = base_x + sinf(photo_wave_time * speed + i * 1.7f) * 35.0f;
          float y = base_y - (photo_wave_time * 55.0f * speed);
          while (y < -sz) y += SCREEN_HEIGHT + sz * 2;
          int alpha = 30 + (int)((sinf(photo_wave_time * 1.8f + i * 2.3f) + 1.0f) * 25.0f);
          vita2d_pgf_draw_text(font, x, y + sz, btn_base[si] | (alpha << 24), sz / 13.0f, btn_sym[si]);
        }
      } else if (vitashell_config.background_anim == 1) {
        // PS Vita/PSP Style Flowing Waves of Light (Nebula/Ribbons)
        // Wave 1 (Indigo/Blue) - refined with smoother motion
        for (int x = 0; x < SCREEN_WIDTH + 20; x += 10) {
          float y = 240.0f + sinf(x * 0.005f + photo_wave_time * 0.6f) * 55.0f + cosf(x * 0.003f - photo_wave_time * 0.35f) * 25.0f;
          int alpha = 14 + (int)((sinf(photo_wave_time * 0.8f + x * 0.002f) + 1.0f) * 8.0f);
          vita2d_draw_fill_circle(x, y, 20.0f, RGBA8(40, 120, 255, alpha));
        }
        // Wave 2 (Purple/Violet) - richer
        for (int x = 0; x < SCREEN_WIDTH + 20; x += 10) {
          float y = 290.0f + sinf(x * 0.006f - photo_wave_time * 0.7f + 1.5f) * 45.0f + cosf(x * 0.004f + photo_wave_time * 0.45f) * 20.0f;
          int alpha = 12 + (int)((cosf(photo_wave_time * 0.7f - x * 0.003f) + 1.0f) * 7.0f);
          vita2d_draw_fill_circle(x, y, 24.0f, RGBA8(200, 80, 255, alpha));
        }
        // Wave 3 (Cyan/Green-Blue) - smoother flow
        for (int x = 0; x < SCREEN_WIDTH + 20; x += 10) {
          float y = 180.0f + sinf(x * 0.004f + photo_wave_time * 0.45f - 1.0f) * 65.0f + cosf(x * 0.002f + photo_wave_time * 0.25f) * 18.0f;
          int alpha = 10 + (int)((sinf(photo_wave_time * 0.6f + x * 0.004f) + 1.0f) * 6.0f);
          vita2d_draw_fill_circle(x, y, 16.0f, RGBA8(60, 230, 255, alpha));
        }
        // Sparkle highlights on wave peaks
        for (int s = 0; s < 8; s++) {
          float sx = (float)((s * 137 + 53) % SCREEN_WIDTH);
          float sy = 240.0f + sinf(sx * 0.005f + photo_wave_time * 0.6f + s * 2.1f) * 55.0f;
          int sa = (int)((sinf(photo_wave_time * 1.5f + s * 3.7f) + 1.0f) * 35.0f);
          vita2d_draw_fill_circle(sx, sy, 4.0f, RGBA8(255, 255, 255, sa));
        }
      } else if (vitashell_config.background_anim == 2) {
        // Stars: Particle Starfield with Parallax
        for (int i = 0; i < 60; i++) {
          float speed = 0.15f + (i % 8) * 0.06f;
          float base_x = (i * 41 + 17) % SCREEN_WIDTH;
          float base_y = (i * 53 + 31) % SCREEN_HEIGHT;
          float twinkle = sinf(photo_wave_time * (1.5f + speed * 2.0f) + i * 4.1f) * 0.5f + 0.5f;
          float x = base_x + sinf(photo_wave_time * speed * 0.3f + i * 1.3f) * 12.0f;
          float y = base_y - (photo_wave_time * 30.0f * speed);
          while (y < -6.0f) y += SCREEN_HEIGHT + 12.0f;
          int alpha = 20 + (int)(twinkle * 80.0f);
          float sz = 2.0f + twinkle * 3.0f;
          unsigned int star_color = RGBA8(180 + (int)(75 * twinkle), 200 + (int)(55 * twinkle), 255, alpha);
          vita2d_draw_fill_circle(x, y, sz, star_color);
        }
      } else if (vitashell_config.background_anim == 3) {
        // Quadrados
        for (int i = 0; i < 30; i++) {
          float speed = 0.5f + (i % 5) * 0.2f;
          float base_x = (i * 67) % SCREEN_WIDTH;
          float base_y = (i * 89) % SCREEN_HEIGHT;
          float size = 15.0f + (i % 10) * 3.0f;
          float x = base_x + sinf(photo_wave_time * speed + i) * 30.0f;
          float y = base_y - (photo_wave_time * 60.0f * speed);
          while (y < -size) y += SCREEN_HEIGHT + size * 2;
          int alpha = 10 + (int)((sinf(photo_wave_time * 2.0f + i) + 1.0f) * 20.0f);
          vita2d_draw_rectangle(x, y, size, size, RGBA8(100, 180, 255, alpha));
          vita2d_draw_rectangle(x + 2, y + 2, size - 4, size - 4, RGBA8(150, 220, 255, alpha / 2));
        }
      } else if (vitashell_config.background_anim == 4) {
        // Spark — faiscas brancas ascendentes
        for (int i = 0; i < 40; i++) {
          float speed = 0.6f + (i % 7) * 0.2f;
          float base_x = (i * 53 + 19) % SCREEN_WIDTH;
          float base_y = (i * 71 + 37) % SCREEN_HEIGHT;
          float sz = 3.0f + (i % 5) * 2.5f;
          float x = base_x + sinf(photo_wave_time * speed * 0.7f + i * 2.1f) * 20.0f;
          float y = base_y - (photo_wave_time * 70.0f * speed);
          while (y < -sz) y += SCREEN_HEIGHT + sz * 2;
          int alpha = 40 + (int)((sinf(photo_wave_time * 3.0f + i * 1.7f) + 1.0f) * 60.0f);
          unsigned int c = RGBA8(220 + (i % 3) * 15, 200 + (i % 4) * 10, 180 + (i % 5) * 8, alpha);
          vita2d_draw_fill_circle(x, y, sz, c);
        }
        // Brilho extra
        for (int i = 0; i < 15; i++) {
          float x = (i * 97 + 11) % SCREEN_WIDTH;
          float y = (i * 83 + 53) % SCREEN_HEIGHT;
          float sz = 10.0f + (i % 3) * 5.0f;
          int a = 10 + (int)((sinf(photo_wave_time * 1.5f + i * 3.3f) + 1.0f) * 30.0f);
          vita2d_draw_fill_circle(x, y, sz, RGBA8(255, 255, 200, a));
        }
      } else if (vitashell_config.background_anim == 5) {
        // Matrix — caracteres katakana caindo em estilo Matrix
        static const char *matrix_chars[] = {
          "\xe3\x82\xbf", "\xe3\x83\x8f", "\xe3\x83\x9f", "\xe3\x83\xa4",
          "\xe3\x83\xa9", "\xe3\x83\xaf", "\xe3\x83\xb3", "\xe3\x82\xa6",
          "\xe3\x82\xa8", "\xe3\x83\xa2", "\xe3\x83\xab", "\xe3\x83\x87",
          "\xe3\x82\xb1", "\xe3\x83\x8b", "\xe3\x83\x81", "\xe3\x83\x84",
          "\xe3\x83\x8a", "\xe3\x83\x8c", "\xe3\x83\x8d", "\xe3\x83\x8e",
          "\xe3\x83\x8f", "\xe3\x83\x92", "\xe3\x83\x95", "\xe3\x83\x98",
          "\xe3\x83\x9b", "\xe3\x83\x9e", "\xe3\x83\x9f", "\xe3\x83\xa0",
          "\xe3\x83\xa1", "\xe3\x83\xa2"
        };
        int n_chars = sizeof(matrix_chars) / sizeof(matrix_chars[0]);
        for (int i = 0; i < 45; i++) {
          float speed = 0.5f + (i % 8) * 0.25f;
          float base_x = (i * 22 + 5) % SCREEN_WIDTH;
          float base_y = (i * 67 + 13) % SCREEN_HEIGHT;
          float x = base_x;
          float y = base_y + (photo_wave_time * 50.0f * speed);
          while (y > SCREEN_HEIGHT + 30) y -= SCREEN_HEIGHT + 60;
          int head_idx = i % n_chars;
          unsigned int head_color = RGBA8(180, 255, 180, 200);
          float sz = 1.0f;
          vita2d_pgf_draw_text(font, x - 5, y - 12, head_color, sz, matrix_chars[head_idx]);
          for (int j = 1; j < 6; j++) {
            int ci = (head_idx + j) % n_chars;
            int a = 60 - j * 10;
            if (a < 10) a = 10;
            vita2d_pgf_draw_text(font, x - 5, y - 12 - j * 12, RGBA8(40, 180, 80, a), sz, matrix_chars[ci]);
          }
        }
      } else if (vitashell_config.background_anim == 6) {
        // Rain (chuva) — gotas azuis caindo na diagonal
        for (int i = 0; i < 60; i++) {
          float speed = 1.0f + (i % 5) * 0.3f;
          float base_x = (i * 31 + 7) % SCREEN_WIDTH;
          float base_y = (i * 59 + 23) % SCREEN_HEIGHT;
          float drift = sinf(photo_wave_time * 0.3f + i * 1.1f) * 30.0f;
          float x = base_x + drift + (photo_wave_time * 15.0f * speed);
          float y = base_y + (photo_wave_time * 80.0f * speed);
          while (x > SCREEN_WIDTH + 20) x -= SCREEN_WIDTH + 40;
          while (y > SCREEN_HEIGHT + 20) y -= SCREEN_HEIGHT + 40;
          int alpha = 20 + (int)((sinf(photo_wave_time * 2.5f + i * 2.3f) + 1.0f) * 40.0f);
          float len = 10.0f + (i % 6) * 3.0f;
          vita2d_draw_line(x, y, x - 4, y + len, RGBA8(100, 200, 255, alpha));
          vita2d_draw_fill_circle(x, y, 2.5f, RGBA8(180, 230, 255, alpha + 30));
        }
      }
    }

    // Top bar: filename, size
    SceIoStat fstat;
    char size_str[32] = "";
    if (sceIoGetstat(file, &fstat) >= 0) {
      if (fstat.st_size < 1024)
        snprintf(size_str, 32, "%lld B", (long long)fstat.st_size);
      else if (fstat.st_size < 1024 * 1024)
        snprintf(size_str, 32, "%.1f KB", (double)fstat.st_size / 1024.0);
      else
        snprintf(size_str, 32, "%.1f MB", (double)fstat.st_size / (1024.0 * 1024.0));
    }
    vita2d_draw_rectangle(0, 0, SCREEN_WIDTH, 44, themeTopbarBg(vitashell_config.theme_preset));
    vita2d_draw_rectangle(0, 44, SCREEN_WIDTH, 1, COLOR_ALPHA(themeTopbarText(vitashell_config.theme_preset), 18));
    pgf_draw_textf(SHELL_MARGIN_X, 10, themeTopbarText(vitashell_config.theme_preset), "%s  |  %s  |  Zoom: %.0f%%", fname, size_str, zoom * 100.0f);

    // Toolbar buttons (bottom) — estilo do menu superior
    int bottom_bar_y = SCREEN_HEIGHT - 50;
    vita2d_draw_rectangle(0, bottom_bar_y - 1, SCREEN_WIDTH, 1, COLOR_ALPHA(themeTopbarText(vitashell_config.theme_preset), 18));
    vita2d_draw_rectangle(0, bottom_bar_y, SCREEN_WIDTH, 50, themeTopbarBg(vitashell_config.theme_preset));
    int btn_y = bottom_bar_y + 4, btn_h = 42;
    int btn_w = 110, btn_gap = 8;
    int total_btn_w = 3 * btn_w + 2 * btn_gap;
    int btn_start_x = (SCREEN_WIDTH - total_btn_w) / 2;
    unsigned int p_btn_colors[3] = {
      themeButtonAccent(vitashell_config.theme_preset),
      themeButtonDanger(vitashell_config.theme_preset),
      themeButtonSuccess(vitashell_config.theme_preset),
    };
    const char *btn_labels[3] = {
      language_container[COPY_BTN],
      language_container[DELETE_BTN],
      language_container[MIRROR_BTN],
    };
    for (int b = 0; b < 3; b++) {
      int bx = btn_start_x + b * (btn_w + btn_gap);
      unsigned int t_card = COLOR_ALPHA(themeCardBg(vitashell_config.theme_preset), 160);
      vita2d_draw_rectangle(bx, btn_y, btn_w, btn_h, t_card);
      vita2d_draw_rectangle(bx, btn_y, btn_w, 2, p_btn_colors[b]);
      vita2d_draw_rectangle(bx, btn_y+btn_h-1, btn_w, 1, COLOR_ALPHA(themeTopbarText(vitashell_config.theme_preset), 8));
      photo_center_text(bx + btn_w / 2.0f, btn_y + 12, themeTopbarText(vitashell_config.theme_preset), btn_labels[b]);
    }

    // Photo
    float flip_x = mirrored ? -zoom : zoom;
    vita2d_draw_texture_scale_rotate_hotspot(tex, SCREEN_HALF_WIDTH, SCREEN_HALF_HEIGHT, flip_x, zoom, rad, x, y);

    // Mirror indicator
    if (mirrored) {
      pgf_draw_text(SHELL_MARGIN_X, SCREEN_HEIGHT - 3.0f * SHELL_MARGIN_Y, COLOR_ALPHA(themeAccentColor(vitashell_config.theme_preset), 200), language_container[MIRRORED_LABEL]);
    }



    // End drawing
    endDrawing();
  }

  vita2d_wait_rendering_done();
  vita2d_free_texture(tex);

  free(buffer);

  return 0;
}
