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
#include "archive.h"
#include "audioplayer.h"
#include "file.h"
#include "theme.h"
#include "language.h"
#include "utils.h"
#include "audio/lrcparse.h"
#include "audio/vita_audio.h"

#include "audio/player.h"

static struct fileInfo *fileinfo = NULL;
static vita2d_texture *tex = NULL;


/**
* Calculate the x-axis position if draw text in center
* @param[in] sx start x-axis
* @param[in] ex end x-axis
* @param[in] text
* @return x-axis position
*/
float getCenteroffset(float sx,float ex,char* string){
  if(!string||(string[0] == '\0'))
    return sx;

  float drawWidthSpace = ex - sx;
  uint16_t stringWidth = pgf_text_width(string);
  return stringWidth > drawWidthSpace ? sx : sx + (drawWidthSpace - stringWidth) / 2 ;
}

/**
* Try to load lrc file from audio path
* @param[in] path audio path
* @param[out] totalms set 0
* @param[out] lyricsIndex set 0
* @return Lyrics pointer , NULL is fail
*/
Lyrics* loadLyricsFile(const char *path, uint64_t *totalms, uint32_t *lyricsIndex){
  size_t pathlength = strlen(path);
  *totalms = *lyricsIndex = 0;

  while(pathlength > 0){
    if(path[pathlength] == '.'){
      break;
    }
    pathlength--;
  }

  if(pathlength < 0)
    return NULL;

  char lrcPath[pathlength + 5 * sizeof(char)];
  memccpy(lrcPath,path,sizeof(char),pathlength);//copy path string except filename extension
  strcpy(lrcPath+pathlength,".lrc");

  return lrcParseLoadWithFile(lrcPath);
}

/**
* Draw the lyrics from the designated area
* @param[in] lyrics Lyrics pointer
* @param[in] cur_time_string Playing time string
* @param[out] totalms Playing time (millisecond)
* @param[out] lyricsIndex Index of lyrics
* @param[in] lrcSpaceX Designated area starting point x
* @param[in] lrcSpaceX Designated area starting point y
*/
void drawLyrics(Lyrics* lyrics, const char *cur_time_string, uint64_t* totalms, uint32_t* lyricsIndex, float lrcSpaceX, float lrcSpaceY){
  if(!lyrics)
    return;

  char hourString[3];
  char minuteString[3];
  char secondString[3];

  strncpy(hourString,cur_time_string,sizeof(hourString)-1);
  strncpy(minuteString,cur_time_string + 3,sizeof(minuteString)-1);
  strncpy(secondString,cur_time_string + 6,sizeof(secondString)-1);

  *totalms = (((atoi(hourString) * 60) + atoi(minuteString)) * 60 + atoi(secondString)) * 1000;

  uint32_t m_index = *lyricsIndex >= 1 ? (*lyricsIndex - 1) : *lyricsIndex;
  float right_max_x = SCREEN_WIDTH - SHELL_MARGIN_X;
  //draw current lyrics
  pgf_draw_textf(getCenteroffset(lrcSpaceX,right_max_x,lyrics->lrclines[m_index].word),lrcSpaceY, AUDIO_INFO_ASSIGN, "%s",lyrics->lrclines[m_index].word);

  int i;
  for (i = 1;i < 7; i++){//draw 6 line lyrics for preview
    int n_index = m_index + i;
    if(n_index + 1 > lyrics->lyricscount)
      break;
    pgf_draw_textf(getCenteroffset(lrcSpaceX,right_max_x,lyrics->lrclines[n_index].word),lrcSpaceY + FONT_Y_SPACE * i, AUDIO_INFO, "%s",lyrics->lrclines[n_index].word);
  }

  if (*totalms >= (lyrics->lrclines[*lyricsIndex].totalms) ){
    *lyricsIndex = *lyricsIndex + 1;
  } else if (( *lyricsIndex >= 1 ) & ( *totalms < (lyrics->lrclines[*lyricsIndex - 1].totalms ))) {
    *lyricsIndex = *lyricsIndex - 1;
  }
}

void shortenString(char *out, const char *in, int width) {
  strcpy(out, in);

  int i;
  for (i = strlen(out)-1; i > 0; i--) {
    if (pgf_text_width(out) < width)
      break;

    out[i] = '\0';
  }
}

vita2d_texture *getAlternativeCoverImage(const char *file) {
  char path[MAX_PATH_LENGTH];

  char *p = strrchr(file, '/');
  if (p) {
    *p = '\0';

    snprintf(path, MAX_PATH_LENGTH, "%s/cover.jpg", file);
    if (checkFileExist(path)) {
      *p = '/';
      return vita2d_load_JPEG_file(path);
    }

    snprintf(path, MAX_PATH_LENGTH, "%s/folder.jpg", file);
    if (checkFileExist(path)) {
      *p = '/';
      return vita2d_load_JPEG_file(path);
    }

    *p = '/';
  }

  return NULL;
}

void getAudioInfo(const char *file) {
  char *buffer = NULL;

  fileinfo = getInfoFunct();

  if (tex) {
    vita2d_wait_rendering_done();
    vita2d_free_texture(tex);
    tex = NULL;
  }

  switch (fileinfo->encapsulatedPictureType) {
    case JPEG_IMAGE:
    case PNG_IMAGE:
    {
      SceUID fd = sceIoOpen(file, SCE_O_RDONLY, 0);
      if (fd >= 0) {
        char *buffer = malloc(fileinfo->encapsulatedPictureLength);
        if (buffer) {
          sceIoLseek32(fd, fileinfo->encapsulatedPictureOffset, SCE_SEEK_SET);
          sceIoRead(fd, buffer, fileinfo->encapsulatedPictureLength);
          sceIoClose(fd);

          if (fileinfo->encapsulatedPictureType == JPEG_IMAGE)
            tex = vita2d_load_JPEG_buffer(buffer, fileinfo->encapsulatedPictureLength);

          if (fileinfo->encapsulatedPictureType == PNG_IMAGE)
            tex = vita2d_load_PNG_buffer(buffer);

          if (tex)
            vita2d_texture_set_filters(tex, SCE_GXM_TEXTURE_FILTER_LINEAR, SCE_GXM_TEXTURE_FILTER_LINEAR);

          free(buffer);
        }

        break;
      }
    }
  }

  if (!tex)
    tex = getAlternativeCoverImage(file);
}

// ---- Visualizer ----
#define VIZ_BARS 20
#define VIZ_FFT_SIZE 512
static float viz_bar_heights[VIZ_BARS];
static float viz_bar_targets[VIZ_BARS];
static float viz_falloff[VIZ_BARS];

static void computeVisualizer() {
  short *pcm = NULL;
  int count = 0;
  getVizPcmData(&pcm, &count);
  if (!pcm || count < 64) return;

  // Simple FFT-based magnitude computation
  // Compute magnitudes for VIZ_BARS frequency bins using DFT-like approach
  for (int bar = 0; bar < VIZ_BARS; bar++) {
    float mag = 0.0f;
    // Frequency range: each bar covers ~43 Hz
    int bin_start = (bar * count) / (VIZ_BARS * 2);
    int bin_end = ((bar + 1) * count) / (VIZ_BARS * 2);
    if (bin_end > count / 2) bin_end = count / 2;
    if (bin_start >= bin_end) bin_start = bin_end - 1;
    if (bin_start < 0) bin_start = 0;

    for (int b = bin_start; b < bin_end; b++) {
      // Approximate magnitude using sum of absolute values in this band
      int idx = b * 2;
      float l = pcm[idx] / 32768.0f;
      float r = pcm[idx + 1] / 32768.0f;
      mag += sqrtf(l * l + r * r);
    }
    mag /= (bin_end - bin_start + 1);

    // Scale and smooth
    float target = mag * 120.0f;
    if (target > 120.0f) target = 120.0f;
    viz_bar_targets[bar] = target;

    // Smooth rise, slow fall
    if (target > viz_bar_heights[bar]) {
      viz_bar_heights[bar] += (target - viz_bar_heights[bar]) * 0.4f;
    } else {
      viz_bar_heights[bar] += (target - viz_bar_heights[bar]) * 0.08f;
    }
    if (viz_bar_heights[bar] < 0) viz_bar_heights[bar] = 0;
  }
}

static void drawVisualizer(float x, float y, float w, float h) {
  float bar_w = w / (VIZ_BARS * 2 + 1);
  int gap = bar_w;
  float bw = bar_w;
  if (bw < 2) bw = 2;

  for (int i = 0; i < VIZ_BARS; i++) {
    float bar_h = viz_bar_heights[i] * h / 120.0f;
    if (bar_h < 1) bar_h = 1;
    float bx = x + i * (bw + gap);

    unsigned int color;
    float pct = bar_h / h;
    if (pct < 0.33f)
      color = RGBA8(60, 200, 60, 200);
    else if (pct < 0.66f)
      color = RGBA8(200, 200, 50, 200);
    else
      color = RGBA8(220, 60, 60, 200);

    vita2d_draw_rectangle(bx, y + h - bar_h, bw, bar_h, color);
  }
}

int audioPlayer(const char *file, int type, FileList *list, FileListEntry *entry, int *base_pos, int *rel_pos) {
  static int speed_list[] = { -7, -3, -1, 0, 1, 3, 7 };
  #define N_SPEED (sizeof(speed_list) / sizeof(int))

  sceAppMgrAcquireBgmPort();

  powerLock();

  setAudioFunctions(type);

  // Enable PCM capture for visualizer
  setVizPcmCapture(1);

  initFunct(0);
  loadFunct((char *)file);
  playFunct();

  getAudioInfo(file);

  uint64_t totalms = 0;
  uint32_t lyricsIndex = 0;
  Lyrics* lyrics = loadLyricsFile(file,&totalms,&lyricsIndex);

  int scroll_count = 0;
  float scroll_x = 0.0f;
  int is_touching = 0;
  int touch_x_start = 0, touch_y_start = 0;
  int touch_x_last = 0, touch_y_last = 0;
  float cover_size = MAX_ENTRIES * FONT_Y_SPACE;

  while (1) {
    char cur_time_string[12];
    getTimeStringFunct(cur_time_string);

    readPad();

    int tx = 0, ty = 0;
    if (touch.reportNum > 0) {
      tx = (touch.report[0].x * 960) / 1920;
      ty = (touch.report[0].y * 544) / 1088;
      if (!is_touching) {
        is_touching = 1;
        touch_x_start = tx; touch_y_start = ty;
      }
      touch_x_last = tx; touch_y_last = ty;
    } else {
      if (is_touching) {
        if (abs(touch_x_last - touch_x_start) < 15 && abs(touch_y_last - touch_y_start) < 15) {
          int aud_close_w = 80, aud_close_h = 34;
          int aud_close_x = SCREEN_WIDTH - aud_close_w - 10;
          int aud_close_y = 10;
          if (touch_x_last >= aud_close_x && touch_x_last <= aud_close_x + aud_close_w && touch_y_last >= aud_close_y && touch_y_last <= aud_close_y + aud_close_h) {
              pressed_pad[PAD_CANCEL] = 1;
          }
          
          int ab_bgy = SCREEN_HEIGHT - 50;
          int ab_btn_w = 100, ab_btn_gap = 10;
          int ab_total = 3 * ab_btn_w + 2 * ab_btn_gap;
          int ab_start = (SCREEN_WIDTH - ab_total) / 2;
          
          if (touch_y_last >= ab_bgy) {
            int prev_bx = ab_start;
            int play_bx = ab_start + 1 * (ab_btn_w + ab_btn_gap);
            int next_bx = ab_start + 2 * (ab_btn_w + ab_btn_gap);
            
            if (touch_x_last >= prev_bx && touch_x_last <= prev_bx + ab_btn_w) {
              pressed_pad[PAD_LTRIGGER] = 1; // Prev
            } else if (touch_x_last >= play_bx && touch_x_last <= play_bx + ab_btn_w) {
              pressed_pad[PAD_ENTER] = 1; // Play/Pause
            } else if (touch_x_last >= next_bx && touch_x_last <= next_bx + ab_btn_w) {
              pressed_pad[PAD_RTRIGGER] = 1; // Next
            }
          } else {
            // Touch seek on progress bar
            float bar_x = 2.0f * SHELL_MARGIN_X + cover_size;
            float bar_y = ab_bgy - 4;
            float bar_w = SCREEN_WIDTH - 3.0f * SHELL_MARGIN_X - cover_size;
            if (touch_y_last >= bar_y && touch_y_last <= bar_y + 12 &&
                touch_x_last >= bar_x && touch_x_last <= bar_x + bar_w) {
              double pct = (touch_x_last - bar_x) / bar_w;
              if (totalms > 0 && setFilePositionFunct) {
                setFilePositionFunct(pct * (totalms / 1000.0));
                setPlayingSpeedFunct(0);
                playFunct();
              }
            }
          }
        }
        is_touching = 0;
      }
    }

    // Cancel
    if (pressed_pad[PAD_CANCEL]) {
      break;
    }

    // Display off
    if (pressed_pad[PAD_TRIANGLE]) {
      scePowerRequestDisplayOff();
    }

    // Toggle play/pause
    if (pressed_pad[PAD_ENTER]) {
      if (isPlayingFunct() && getPlayingSpeedFunct() == 0) {
        pauseFunct();
      } else {
        setPlayingSpeedFunct(0);
        playFunct();
      }
    }

    if (pressed_pad[PAD_LEFT] || pressed_pad[PAD_RIGHT]) {
      int speed = getPlayingSpeedFunct();

      if (pressed_pad[PAD_LEFT]) {
        int i;
        for (i = 0; i < N_SPEED; i++) {
          if (speed_list[i] == speed) {
            if (i > 0)
              speed = speed_list[i-1];
            break;
          }
        }
      }

      if (pressed_pad[PAD_RIGHT]) {
        int i;
        for (i = 0; i < N_SPEED; i++) {
          if (speed_list[i] == speed) {
            if (i < N_SPEED - 1)
              speed = speed_list[i + 1];
            break;
          }
        }
      }

      setPlayingSpeedFunct(speed);

      playFunct();
    }

    // Previous/next song.
    if (getPercentageFunct() == 100.0f || endOfStreamFunct() ||
      pressed_pad[PAD_LTRIGGER] || pressed_pad[PAD_RTRIGGER]) {
      int previous = pressed_pad[PAD_LTRIGGER];
      if (previous && strcmp(cur_time_string, "00:00:00") != 0) {
        lrcParseClose(lyrics);
        endFunct();
        initFunct(0);
        loadFunct((char *)file);
        playFunct();

        getAudioInfo(file);

        lyrics = loadLyricsFile(file,&totalms,&lyricsIndex);

      } else {
        int available = 0;

        int old_base_pos = *base_pos;
        int old_rel_pos = *rel_pos;
        FileListEntry *old_entry = entry;

        if (getPercentageFunct() == 100.0f && !endOfStreamFunct())
          previous = 1;

        if (endOfStreamFunct())
          previous = 0;

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
              } else if ((*base_pos+*rel_pos + 1) < list->length) {
                (*base_pos)++;
              }
            }
          }

          if (!entry->is_folder) {
            char path[MAX_PATH_LENGTH];
            snprintf(path, MAX_PATH_LENGTH, "%s%s", list->path, entry->name);
            int type = getFileType(path);
            if (type == FILE_TYPE_MP3 || type == FILE_TYPE_OGG) {
              file = path;

              lrcParseClose(lyrics);
              endFunct();

              setAudioFunctions(type);

              initFunct(0);
              loadFunct((char *)file);
              playFunct();

              getAudioInfo(file);

              lyrics = loadLyricsFile(file,&totalms,&lyricsIndex);

              available = 1;
              break;
            }
          }
        }

        if (!available) {
          *base_pos = old_base_pos;
          *rel_pos = old_rel_pos;
          entry = old_entry;
          break;
        }
      }
    }

    // Start drawing with animated background
    startDrawing(NULL);
    drawGifBackground();

    // Top bar hidden for cleaner music experience

    unsigned int accent = themeAccentColor(vitashell_config.theme_preset);
    unsigned int card = COLOR_ALPHA(themeCardBg(vitashell_config.theme_preset), 160);
    unsigned int topbar_text = themeTopbarText(vitashell_config.theme_preset);

    // Cover with subtle border
    if (tex) {
      vita2d_draw_rectangle(SHELL_MARGIN_X - 2, START_Y - 2, cover_size + 4, cover_size + 4, COLOR_ALPHA(accent, 40));
      vita2d_draw_texture_scale(tex, SHELL_MARGIN_X, START_Y, cover_size / vita2d_texture_get_width(tex), cover_size / vita2d_texture_get_height(tex));
    } else {
      vita2d_draw_texture(cover_image, SHELL_MARGIN_X, START_Y);
    }

    // Info
    float x = 2.0f * SHELL_MARGIN_X + cover_size;

    pgf_draw_text(x, START_Y + (0 * FONT_Y_SPACE), AUDIO_INFO_ASSIGN, language_container[TITLE]);
    pgf_draw_text(x, START_Y + (1 * FONT_Y_SPACE), AUDIO_INFO_ASSIGN, language_container[ALBUM]);
    pgf_draw_text(x, START_Y + (2 * FONT_Y_SPACE), AUDIO_INFO_ASSIGN, language_container[ARTIST]);
    pgf_draw_text(x, START_Y + (3 * FONT_Y_SPACE), AUDIO_INFO_ASSIGN, language_container[GENRE]);
    pgf_draw_text(x, START_Y + (4 * FONT_Y_SPACE), AUDIO_INFO_ASSIGN, language_container[YEAR]);

    x += 120.0f;

    vita2d_enable_clipping();
    vita2d_set_clip_rectangle(x + 1.0f, START_Y, x + 1.0f + 390.0f, START_Y + (5 * FONT_Y_SPACE));

    float title_x = x;
    uint32_t color = AUDIO_INFO;

    int width = (int)pgf_text_width(fileinfo->title);
    if (width >= 390.0f) {
      if (scroll_count < 60) {
        scroll_x = title_x;
      } else if (scroll_count < width + 90) {
        scroll_x--;
      } else if (scroll_count < width + 120) {
        color = (color & 0x00FFFFFF) | ((((color >> 24) * (scroll_count - width - 90)) / 30) << 24);
        scroll_x = title_x;
      } else {
        scroll_count = 0;
      }
      
      scroll_count++;
      
      title_x = scroll_x;
    }

    pgf_draw_text(title_x, START_Y + (0 * FONT_Y_SPACE), color, fileinfo->title[0] == '\0' ? "-" : fileinfo->title);    
    pgf_draw_text(x, START_Y + (1 * FONT_Y_SPACE), AUDIO_INFO, fileinfo->album[0] == '\0' ? "-" : fileinfo->album);
    pgf_draw_text(x, START_Y + (2 * FONT_Y_SPACE), AUDIO_INFO, fileinfo->artist[0] == '\0' ? "-" : fileinfo->artist);
    pgf_draw_text(x, START_Y + (3 * FONT_Y_SPACE), AUDIO_INFO, fileinfo->genre[0] == '\0' ? "-" : fileinfo->genre);
    pgf_draw_text(x, START_Y + (4 * FONT_Y_SPACE), AUDIO_INFO, fileinfo->year[0] == '\0' ? "-" : fileinfo->year);

    vita2d_disable_clipping();

    x -= 120.0f;

    drawLyrics(lyrics, cur_time_string, &totalms, &lyricsIndex, x, START_Y + (6 * FONT_Y_SPACE));

    // Compute and draw visualizer
    computeVisualizer();
    float viz_x = 2.0f * SHELL_MARGIN_X + cover_size;
    float viz_y = START_Y + (5 * FONT_Y_SPACE) + 10;
    float viz_w = SCREEN_WIDTH - viz_x - SHELL_MARGIN_X;
    float viz_h = 70;
    drawVisualizer(viz_x, viz_y, viz_w, viz_h);

    // Bottom toolbar
    int ab_bgy = SCREEN_HEIGHT - 50;
    int ab_toolbar_h = 50;
    vita2d_draw_rectangle(0, ab_bgy, SCREEN_WIDTH, ab_toolbar_h, themeTopbarBg(vitashell_config.theme_preset));
    vita2d_draw_rectangle(0, ab_bgy, SCREEN_WIDTH, 1, COLOR_ALPHA(topbar_text, 18));

    int ab_btn_y = ab_bgy + 6, ab_btn_h = 38;
    int ab_btn_w = 100, ab_btn_gap = 10;
    int ab_total = 3 * ab_btn_w + 2 * ab_btn_gap;
    int ab_start = (SCREEN_WIDTH - ab_total) / 2;

    unsigned int ab_def = themeButtonDefault(vitashell_config.theme_preset);

    // Prev button
    int prev_bx = ab_start;
    vita2d_draw_rectangle(prev_bx, ab_btn_y, ab_btn_w, ab_btn_h, card);
    vita2d_draw_rectangle(prev_bx, ab_btn_y, ab_btn_w, 2, ab_def);
    vita2d_draw_rectangle(prev_bx, ab_btn_y+ab_btn_h-1, ab_btn_w, 1, COLOR_ALPHA(topbar_text, 8));
    pgf_draw_text(prev_bx + (ab_btn_w - pgf_text_width("<<")) / 2.0f, ab_btn_y + 8, topbar_text, "<<");

    // Play/Pause button
    int play_bx = ab_start + 1 * (ab_btn_w + ab_btn_gap);
    vita2d_texture *ab_icon = NULL;
    if (getPlayingSpeedFunct() != 0) {
      ab_icon = (getPlayingSpeedFunct() < 0) ? fastrewind_image : fastforward_image;
    } else {
      ab_icon = isPlayingFunct() ? pause_image : play_image;
    }
    vita2d_draw_rectangle(play_bx, ab_btn_y, ab_btn_w, ab_btn_h, card);
    vita2d_draw_rectangle(play_bx, ab_btn_y, ab_btn_w, 2, accent);
    vita2d_draw_rectangle(play_bx, ab_btn_y+ab_btn_h-1, ab_btn_w, 1, COLOR_ALPHA(topbar_text, 8));
    if (ab_icon) {
      float icon_x = play_bx + (ab_btn_w - vita2d_texture_get_width(ab_icon)) / 2.0f;
      float icon_y = ab_btn_y + (ab_btn_h - vita2d_texture_get_height(ab_icon)) / 2.0f;
      vita2d_draw_texture(ab_icon, icon_x, icon_y);
    }

    // Speed indicator
    if (getPlayingSpeedFunct() != 0) {
      pgf_draw_textf(play_bx + ab_btn_w + 4, ab_btn_y + 10, AUDIO_SPEED, "%dx", abs(getPlayingSpeedFunct() + (getPlayingSpeedFunct() < 0 ? -1 : 1)));
    }

    // Next button
    int next_bx = ab_start + 2 * (ab_btn_w + ab_btn_gap);
    vita2d_draw_rectangle(next_bx, ab_btn_y, ab_btn_w, ab_btn_h, card);
    vita2d_draw_rectangle(next_bx, ab_btn_y, ab_btn_w, 2, ab_def);
    vita2d_draw_rectangle(next_bx, ab_btn_y+ab_btn_h-1, ab_btn_w, 1, COLOR_ALPHA(topbar_text, 8));
    pgf_draw_text(next_bx + (ab_btn_w - pgf_text_width(">>")) / 2.0f, ab_btn_y + 8, topbar_text, ">>");

    // Close button (Top Right)
    int aud_close_w = 80, aud_close_h = 34;
    int aud_close_x = SCREEN_WIDTH - aud_close_w - 10;
    int aud_close_y = 10;
    unsigned int aud_close_col = themeButtonDanger(vitashell_config.theme_preset);
    vita2d_draw_rectangle(aud_close_x, aud_close_y, aud_close_w, aud_close_h, card);
    vita2d_draw_rectangle(aud_close_x, aud_close_y, aud_close_w, 2, aud_close_col);
    pgf_draw_text(aud_close_x + (aud_close_w - pgf_text_width(language_container[CLOSE_LABEL])) / 2.0f, aud_close_y + 8, topbar_text, language_container[CLOSE_LABEL]);

    // Progress section: thicker bar with time labels at both ends
    float prog_section_y = ab_bgy - FONT_Y_SPACE - 22;
    float prog_bar_y = prog_section_y + FONT_Y_SPACE + 4;
    float prog_bar_h = 12;
    float prog_bar_x = 2.0f * SHELL_MARGIN_X + cover_size;
    float prog_bar_w = SCREEN_WIDTH - 3.0f * SHELL_MARGIN_X - cover_size;

    // Current time at left end
    char cur_t_str[12];
    strcpy(cur_t_str, cur_time_string + 3); // skip HH:
    pgf_draw_text(prog_bar_x, prog_section_y, AUDIO_TIME_CURRENT, cur_t_str);

    // Total time at right end
    pgf_draw_text(ALIGN_RIGHT(prog_bar_x + prog_bar_w, pgf_text_width(fileinfo->strLength)), prog_section_y, AUDIO_TIME_TOTAL, fileinfo->strLength);

    // Progress bar
    vita2d_draw_rectangle(prog_bar_x, prog_bar_y, prog_bar_w, prog_bar_h, AUDIO_TIME_BAR_BG);
    float pct_f = (float)getPercentageFunct() / 100.0f;
    if (pct_f > 0) {
      float fill_w = pct_f * prog_bar_w;
      if (fill_w < 2) fill_w = 2;
      vita2d_draw_rectangle(prog_bar_x, prog_bar_y, fill_w, prog_bar_h, AUDIO_TIME_BAR);
      // Thumb indicator
      vita2d_draw_rectangle(prog_bar_x + fill_w - 4, prog_bar_y - 2, 8, prog_bar_h + 4, accent);
    }

    // End drawing
    endDrawing();
  }

  if (tex) {
    vita2d_wait_rendering_done();
    vita2d_free_texture(tex);
    tex = NULL;
  }

  setVizPcmCapture(0);

  lrcParseClose(lyrics);
  endFunct();

  powerUnlock();

  sceAppMgrReleaseBgmPort();

  return 0;
}
