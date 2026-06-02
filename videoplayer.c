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

#include <psp2/avplayer.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/io/fcntl.h>
#include <psp2/display.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "main.h"
#include "videoplayer.h"
#include "file.h"
#include "theme.h"
#include "language.h"
#include "utils.h"

static SceAvPlayerHandle g_player = -1;
static SceUID g_video_fd = -1;
static SceOff g_video_file_size = 0;
static int g_is_paused = 0;
static uint64_t g_total_time_ms = 0;

// Track CDRAM frame allocations for freeing
#define MAX_FRAME_ALLOCS 16
static struct { void *ptr; SceUID uid; } g_frame_allocs[MAX_FRAME_ALLOCS];
static int g_frame_alloc_count = 0;

static SceUID findFrameBlock(void *ptr) {
  for (int i = 0; i < g_frame_alloc_count; i++) {
    if (g_frame_allocs[i].ptr == ptr)
      return g_frame_allocs[i].uid;
  }
  return -1;
}

// Memory callbacks
static void *videoAlloc(void *arg, uint32_t alignment, uint32_t size) {
  return memalign(alignment, size);
}

static void videoFree(void *arg, void *ptr) {
  free(ptr);
}

static void *videoAllocFrame(void *arg, uint32_t alignment, uint32_t size) {
  SceUID block = sceKernelAllocMemBlock("avp_frame", SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW, size, NULL);
  if (block < 0)
    return memalign(alignment, size);
  void *base = NULL;
  if (sceKernelGetMemBlockBase(block, &base) < 0 || !base) {
    sceKernelFreeMemBlock(block);
    return memalign(alignment, size);
  }
  if (g_frame_alloc_count < MAX_FRAME_ALLOCS) {
    g_frame_allocs[g_frame_alloc_count].ptr = base;
    g_frame_allocs[g_frame_alloc_count].uid = block;
    g_frame_alloc_count++;
  }
  return base;
}

static void videoFreeFrame(void *arg, void *ptr) {
  if (!ptr) return;
  SceUID block = findFrameBlock(ptr);
  if (block >= 0) {
    sceKernelFreeMemBlock(block);
    for (int i = 0; i < g_frame_alloc_count; i++) {
      if (g_frame_allocs[i].ptr == ptr) {
        g_frame_allocs[i] = g_frame_allocs[--g_frame_alloc_count];
        break;
      }
    }
  } else {
    free(ptr);
  }
}

// File callbacks
static int videoOpenFile(void *p, const char *filename) {
  g_video_fd = sceIoOpen(filename, SCE_O_RDONLY, 0);
  if (g_video_fd < 0) return -1;
  g_video_file_size = sceIoLseek(g_video_fd, 0, SCE_SEEK_END);
  sceIoLseek(g_video_fd, 0, SCE_SEEK_SET);
  return 0;
}

static int videoCloseFile(void *p) {
  if (g_video_fd >= 0) {
    sceIoClose(g_video_fd);
    g_video_fd = -1;
  }
  return 0;
}

static int videoReadOffsetFile(void *p, uint8_t *buffer, uint64_t position, uint32_t length) {
  if (g_video_fd < 0) return -1;
  sceIoLseek(g_video_fd, position, SCE_SEEK_SET);
  return sceIoRead(g_video_fd, buffer, length);
}

static uint64_t videoSizeFile(void *p) {
  return g_video_file_size;
}

// YUV420 planar to ABGR8888 conversion
static void yuv420_to_abgr(const uint8_t *yuv, uint32_t *rgba, int w, int h) {
  int frame_size = w * h;
  int chroma_size = (w / 2) * (h / 2);
  const uint8_t *y_plane = yuv;
  const uint8_t *u_plane = yuv + frame_size;
  const uint8_t *v_plane = yuv + frame_size + chroma_size;

  for (int j = 0; j < h; j++) {
    for (int i = 0; i < w; i++) {
      int yi = j * w + i;
      int ui = (j / 2) * (w / 2) + (i / 2);

      int Y = y_plane[yi];
      int U = u_plane[ui] - 128;
      int V = v_plane[ui] - 128;

      int R = (Y * 298 + V * 409 + 128) >> 8;
      int G = (Y * 298 - U * 100 - V * 208 + 128) >> 8;
      int B = (Y * 298 + U * 516 + 128) >> 8;

      if (R < 0) R = 0;
      if (R > 255) R = 255;
      if (G < 0) G = 0;
      if (G > 255) G = 255;
      if (B < 0) B = 0;
      if (B > 255) B = 255;

      rgba[yi] = (0xFF << 24) | (B << 16) | (G << 8) | R;
    }
  }
}

int videoPlayer(const char *file, FileList *list, FileListEntry *entry, int *base_pos, int *rel_pos) {
  powerLock();

  int result = 0;
  g_frame_alloc_count = 0;

  // Ensure sysmodule is loaded
  int module_res = sceSysmoduleLoadModule(SCE_SYSMODULE_AVPLAYER);
  if (module_res < 0) {
    powerUnlock();
    return module_res;
  }

  // Init player
  SceAvPlayerInitData initData;
  memset(&initData, 0, sizeof(initData));
  initData.memoryReplacement.allocate = videoAlloc;
  initData.memoryReplacement.deallocate = videoFree;
  initData.memoryReplacement.allocateTexture = videoAllocFrame;
  initData.memoryReplacement.deallocateTexture = videoFreeFrame;

  initData.fileReplacement.open = videoOpenFile;
  initData.fileReplacement.close = videoCloseFile;
  initData.fileReplacement.readOffset = videoReadOffsetFile;
  initData.fileReplacement.size = videoSizeFile;

  initData.basePriority = 0x10000100;
  initData.numOutputVideoFrameBuffers = 16;
  initData.autoStart = SCE_TRUE;

  g_player = sceAvPlayerInit(&initData);
  if (g_player < 0) {
    powerUnlock();
    return g_player;
  }

  // Add source
  int ret = sceAvPlayerAddSource(g_player, file);
  if (ret < 0) {
    sceAvPlayerClose(g_player);
    g_player = -1;
    powerUnlock();
    return ret;
  }

  // Get video stream info for dimensions
  int video_w = SCREEN_WIDTH, video_h = SCREEN_HEIGHT;
  g_total_time_ms = 0;
  for (int i = 0; i < 2; i++) {
    SceAvPlayerStreamInfo streamInfo;
    memset(&streamInfo, 0, sizeof(streamInfo));
    if (sceAvPlayerGetStreamInfo(g_player, i, &streamInfo) < 0) continue;
    if (streamInfo.type == SCE_AVPLAYER_VIDEO) {
      if (streamInfo.details.video.width > 0 && streamInfo.details.video.height > 0) {
        video_w = streamInfo.details.video.width;
        video_h = streamInfo.details.video.height;
      }
    }
    if (streamInfo.duration > g_total_time_ms)
      g_total_time_ms = streamInfo.duration;
  }

  g_is_paused = 0;

  int is_touching = 0;
  int touch_x_start = 0, touch_y_start = 0;
  int touch_x_last = 0, touch_y_last = 0;

  // Pre-allocate conversion buffer and texture
  uint32_t *rgb_buf = (uint32_t *)malloc(video_w * video_h * 4);
  vita2d_texture *video_tex = vita2d_create_empty_texture_format(video_w, video_h, SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR);
  if (video_tex) {
    vita2d_texture_set_filters(video_tex, SCE_GXM_TEXTURE_FILTER_LINEAR, SCE_GXM_TEXTURE_FILTER_LINEAR);
  }

  while (1) {
    readPad();

    // Touch handling
    if (touch.reportNum > 0) {
      int tx = (touch.report[0].x * 960) / 1920;
      int ty = (touch.report[0].y * 544) / 1088;
      if (!is_touching) {
        is_touching = 1;
        touch_x_start = tx;
        touch_y_start = ty;
      }
      touch_x_last = tx;
      touch_y_last = ty;
    } else if (is_touching) {
      is_touching = 0;
      if (abs(touch_x_last - touch_x_start) < 20 && abs(touch_y_last - touch_y_start) < 20) {
        // Close button
        int close_x = SCREEN_WIDTH - 90, close_y = 10, close_w = 80, close_h = 34;
        if (touch_x_last >= close_x && touch_x_last <= close_x + close_w &&
            touch_y_last >= close_y && touch_y_last <= close_y + close_h) {
          pressed_pad[PAD_CANCEL] = 1;
        } else {
          // Progress bar seek
          int bar_y = SCREEN_HEIGHT - 30;
          if (touch_y_last >= bar_y - 10 && touch_y_last <= bar_y + 10) {
            if (g_total_time_ms > 0) {
              float pct = (touch_x_last - 10.0f) / (SCREEN_WIDTH - 20.0f);
              if (pct < 0) pct = 0;
              if (pct > 1) pct = 1;
              sceAvPlayerJumpToTime(g_player, (uint64_t)(pct * g_total_time_ms));
            }
          } else {
            // Toggle play/pause
            if (g_is_paused) {
              sceAvPlayerResume(g_player);
              g_is_paused = 0;
            } else {
              sceAvPlayerPause(g_player);
              g_is_paused = 1;
            }
          }
        }
      }
    }

    // Cancel button
    if (pressed_pad[PAD_CANCEL]) {
      break;
    }

    // Enter to toggle play/pause
    if (pressed_pad[PAD_ENTER]) {
      if (g_is_paused) {
        sceAvPlayerResume(g_player);
        g_is_paused = 0;
      } else {
        sceAvPlayerPause(g_player);
        g_is_paused = 1;
      }
    }

    // Left/Right for seek (5 seconds)
    if (pressed_pad[PAD_LEFT] && g_total_time_ms > 0) {
      uint64_t cur = sceAvPlayerCurrentTime(g_player);
      if (cur >= 5000) sceAvPlayerJumpToTime(g_player, cur - 5000);
      else sceAvPlayerJumpToTime(g_player, 0);
    }
    if (pressed_pad[PAD_RIGHT] && g_total_time_ms > 0) {
      uint64_t cur = sceAvPlayerCurrentTime(g_player);
      uint64_t new_time = cur + 5000;
      if (new_time < g_total_time_ms) sceAvPlayerJumpToTime(g_player, new_time);
      else sceAvPlayerJumpToTime(g_player, g_total_time_ms - 1000);
    }

    // Check if video ended
    if (!sceAvPlayerIsActive(g_player)) {
      break;
    }

    // Get video frame
    SceAvPlayerFrameInfo videoFrame;
    memset(&videoFrame, 0, sizeof(videoFrame));
    SceBool got_frame = sceAvPlayerGetVideoData(g_player, &videoFrame);

    // Start drawing
    startDrawing(NULL);

    // Background
    vita2d_draw_rectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, RGBA8(0, 0, 0, 255));

    if (got_frame == SCE_TRUE && videoFrame.pData && video_tex && rgb_buf) {
      int fw = videoFrame.details.video.width;
      int fh = videoFrame.details.video.height;

      if (fw > 0 && fh > 0) {
        // Clamp to allocated buffer dimensions to prevent overflow
        if (fw > video_w) fw = video_w;
        if (fh > video_h) fh = video_h;

        // Convert YUV420 to ABGR
        yuv420_to_abgr((const uint8_t *)videoFrame.pData, rgb_buf, fw, fh);

        // Upload to texture
        vita2d_wait_rendering_done();
        void *tex_data = vita2d_texture_get_datap(video_tex);
        if (tex_data) {
          memcpy(tex_data, rgb_buf, fw * fh * 4);
        }

        // Calculate aspect ratio
        float scale_x = (float)SCREEN_WIDTH / fw;
        float scale_y = (float)SCREEN_HEIGHT / fh;
        float scale = (scale_x < scale_y) ? scale_x : scale_y;

        float draw_w = fw * scale;
        float draw_h = fh * scale;
        float draw_x = (SCREEN_WIDTH - draw_w) / 2.0f;
        float draw_y = (SCREEN_HEIGHT - draw_h) / 2.0f;

        vita2d_draw_texture_scale(video_tex, draw_x, draw_y, scale, scale);
      }
    } else {
      pgf_draw_text(SCREEN_HALF_WIDTH - 60, SCREEN_HALF_HEIGHT - 10, RGBA8(255, 255, 255, 200), "Buffering...");
    }

    // Controls overlay
    unsigned int overlay_bg = RGBA8(0, 0, 0, 140);

    // Top bar
    vita2d_draw_rectangle(0, 0, SCREEN_WIDTH, 44, overlay_bg);

    const char *short_name = strrchr(file, '/');
    if (short_name) short_name++; else short_name = file;
    char name_buf[64];
    strncpy(name_buf, short_name, 60);
    name_buf[60] = '\0';
    pgf_draw_text(12, 12, RGBA8(255, 255, 255, 220), name_buf);

    // Close button
    int close_w = 80, close_h = 34;
    int close_x = SCREEN_WIDTH - close_w - 10, close_y = 5;
    vita2d_draw_rectangle(close_x, close_y, close_w, close_h, RGBA8(180, 40, 40, 200));
    pgf_draw_text(close_x + (close_w - pgf_text_width(language_container[CLOSE_LABEL])) / 2.0f, close_y + 8, RGBA8(255, 255, 255, 220), language_container[CLOSE_LABEL]);

    // Bottom bar
    int bar_y = SCREEN_HEIGHT - 40;
    vita2d_draw_rectangle(0, bar_y, SCREEN_WIDTH, 40, overlay_bg);

    // Time
    char time_str[32];
    uint64_t cur_time = sceAvPlayerCurrentTime(g_player);
    unsigned int cur_sec = (unsigned int)(cur_time / 1000);
    unsigned int total_sec = (unsigned int)(g_total_time_ms / 1000);
    snprintf(time_str, sizeof(time_str), "%02d:%02d / %02d:%02d",
             cur_sec / 60, cur_sec % 60, total_sec / 60, total_sec % 60);
    pgf_draw_text(SCREEN_HALF_WIDTH - pgf_text_width(time_str) / 2.0f, bar_y + 10, RGBA8(200, 200, 200, 220), time_str);

    // Progress bar
    float prog_bar_x = 10, prog_bar_y = bar_y + 2;
    float prog_bar_w = SCREEN_WIDTH - 20, prog_bar_h = 4;
    vita2d_draw_rectangle(prog_bar_x, prog_bar_y, prog_bar_w, prog_bar_h, RGBA8(60, 60, 60, 200));
    if (g_total_time_ms > 0) {
      float pct = (float)cur_time / g_total_time_ms;
      if (pct > 1.0f) pct = 1.0f;
      vita2d_draw_rectangle(prog_bar_x, prog_bar_y, prog_bar_w * pct, prog_bar_h, themeAccentColor(vitashell_config.theme_preset));
    }

    // Pause indicator
    if (g_is_paused) {
      const char *pause_indicator = "II";
      float pw = pgf_text_width(pause_indicator);
      pgf_draw_text(SCREEN_HALF_WIDTH - pw / 2.0f, SCREEN_HALF_HEIGHT - 20, RGBA8(255, 255, 255, 180), pause_indicator);
    }

    endDrawing();
  }

  // Cleanup
  if (video_tex) {
    vita2d_wait_rendering_done();
    vita2d_free_texture(video_tex);
  }
  free(rgb_buf);

  if (g_player >= 0) {
    sceAvPlayerStop(g_player);
    sceAvPlayerClose(g_player);
    g_player = -1;
  }

  if (g_video_fd >= 0) {
    sceIoClose(g_video_fd);
    g_video_fd = -1;
  }

  powerUnlock();
  return result;
}
