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

#include "qr.h"

#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h> 
#include <psp2/camera.h>

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <quirc.h>
#include <vita2d.h>

#include "main.h"
#include "io_process.h"
#include "network_download.h"
#include "package_installer.h"
#include "archive.h"
#include "file.h"
#include "message_dialog.h"
#include "language.h"
#include "utils.h"

static int qr_enabled;

static struct quirc *qr;
static uint32_t* qr_data;
static char *data;
static int qr_next;

static vita2d_texture *camera_tex;

static SceCameraInfo cam_info;
static SceCameraRead cam_info_read;

static char last_qr[MAX_QR_LENGTH];
static char last_download[MAX_QR_LENGTH];
static int last_qr_len;
static int qr_scanned = 0;

static SceUID thid;

int qr_thread(SceSize args, void *argp) {
  qr = quirc_new();
  quirc_resize(qr, CAM_WIDTH, CAM_HEIGHT);
  qr_next = 1;
  while (1) {
    sceKernelDelayThread(10);
    if (qr_next == 0 && qr_scanned == 0) {
      uint8_t *image;
      int w, h;
      image = quirc_begin(qr, &w, &h);
      uint32_t colourRGBA;
      int i;
      for (i = 0; i < w*h; i++) {
        colourRGBA = qr_data[i];
        image[i] = ((colourRGBA & 0x000000FF) + ((colourRGBA & 0x0000FF00) >> 8) + ((colourRGBA & 0x00FF0000) >> 16)) / 3;
      }
      quirc_end(qr);
      int num_codes = quirc_count(qr);
      if (num_codes > 0) {
        struct quirc_code code;
        struct quirc_data data;
        quirc_decode_error_t err;
        
        quirc_extract(qr, 0, &code);
        err = quirc_decode(&code, &data);
        if (err) {
        } else {
          memcpy(last_qr, data.payload, data.payload_len);
          last_qr_len = data.payload_len;
          qr_scanned = 1;
        }
      } else {
        memset(last_qr, 0, MAX_QR_LENGTH);
      }
      qr_next = 1;
      sceKernelDelayThread(250000);
    }
  }
}

static void url_extract_filename(char *url, char *out, int out_size) {
  char *p = url;
  char *last = url;
  while ((p = strpbrk(p, "\\/"))) {
    last = p;
    p++;
  }
  if (last != url) last++;
  strncpy(out, last, out_size - 1);
  out[out_size - 1] = '\0';
}

int qr_scan_thread(SceSize args, void *argp) {
  data = last_qr;
  // Case-insensitive http check
  if (last_qr_len > 4) {
    char http_check[5];
    int i;
    for (i = 0; i < 4; i++)
      http_check[i] = (data[i] >= 'A' && data[i] <= 'Z') ? data[i] + 0x20 : data[i];
    http_check[4] = '\0';
    if (strcmp(http_check, "http") != 0) {
      initMessageDialog(SCE_MSG_DIALOG_BUTTON_TYPE_OK, language_container[QR_SHOW_CONTENTS], data);
      setDialogStep(DIALOG_STEP_QR_SHOW_CONTENTS);
      return sceKernelExitDeleteThread(0);
    }
  } else {
    initMessageDialog(SCE_MSG_DIALOG_BUTTON_TYPE_OK, language_container[QR_SHOW_CONTENTS], data);
    setDialogStep(DIALOG_STEP_QR_SHOW_CONTENTS);
    return sceKernelExitDeleteThread(0);
  }
  
  initMessageDialog(SCE_MSG_DIALOG_BUTTON_TYPE_NONE, language_container[PLEASE_WAIT]);
  
  const char *headerData;
  unsigned int headerLen;
  int vpk = 0;
  char fileName[MAX_URL_LENGTH];
  fileName[0] = '\0';
  uint64_t fileSize = 0;
  char sizeString[16] = "";
  int ret;
  int has_file_info = 0;

  ret = getDownloadFileSize(data, &fileSize);
  if (ret >= 0) {
    getSizeString(sizeString, fileSize);
    has_file_info = 1;
  }

  ret = getFieldFromHeader(data, "Content-Disposition", &headerData, &headerLen);
  if (ret >= 0 && headerLen > 0) {
    // Has Content-Disposition
    if (strstr(headerData, "inline") != NULL) {
      // inline → treat as website (but store URL for touch dialog)
      sceMsgDialogClose();
      while (isMessageDialogRunning())
        sceKernelDelayThread(10 * 1000);

      strncpy(qr_dialog_url, data, QR_DIALOG_URL_SIZE - 1);
      qr_dialog_url[QR_DIALOG_URL_SIZE - 1] = '\0';
      setDialogStep(DIALOG_STEP_QR_WEBSITE_TOUCH);
      while (getDialogStep() == DIALOG_STEP_QR_WEBSITE_TOUCH)
        sceKernelDelayThread(10 * 1000);
      if (getDialogStep() != DIALOG_STEP_NONE)
        goto EXIT;
      return sceKernelExitDeleteThread(0);
    }

    char *p = strstr(headerData, "filename=");
    if (p) {
      char *fn = p + 9;
      p = strchr(fn, '\n');
      if (p) *p = '\0';
      while (*fn < 0x20 || *fn == ' ' || *fn == '\\' || *fn == '/' ||
             *fn == ':' || *fn == '*' || *fn == '?' || *fn == '"' ||
             *fn == '<' || *fn == '>' || *fn == '|')
        fn++;
      int i;
      for (i = strlen(fn) - 1; i >= 0; i--) {
        if (fn[i] < 0x20 || fn[i] == ' ' || fn[i] == '\\' || fn[i] == '/' ||
            fn[i] == ':' || fn[i] == '*' || fn[i] == '?' || fn[i] == '"' ||
            fn[i] == '<' || fn[i] == '>' || fn[i] == '|')
          fn[i] = 0;
        else
          break;
      }
      strncpy(fileName, fn, sizeof(fileName) - 1);
      fileName[sizeof(fileName) - 1] = '\0';
    }
  } else {
    // No Content-Disposition — try to extract filename from URL
    url_extract_filename(data, fileName, sizeof(fileName));
  }

  sceMsgDialogClose();
  while (isMessageDialogRunning())
    sceKernelDelayThread(10 * 1000);

  // If no filename has an extension, treat as website
  if (fileName[0] == '\0' || !strrchr(fileName, '.')) {
    strncpy(qr_dialog_url, data, QR_DIALOG_URL_SIZE - 1);
    qr_dialog_url[QR_DIALOG_URL_SIZE - 1] = '\0';
    setDialogStep(DIALOG_STEP_QR_WEBSITE_TOUCH);
    while (getDialogStep() == DIALOG_STEP_QR_WEBSITE_TOUCH)
      sceKernelDelayThread(10 * 1000);
    return sceKernelExitDeleteThread(0);
  }

  // Has a file extension — determine type
  vpk = getFileType(fileName) == FILE_TYPE_VPK;

  // Store info for touch dialog
  strncpy(qr_dialog_url, data, QR_DIALOG_URL_SIZE - 1);
  qr_dialog_url[QR_DIALOG_URL_SIZE - 1] = '\0';
  strncpy(qr_dialog_filename, fileName, QR_DIALOG_FNAME_SIZE - 1);
  qr_dialog_filename[QR_DIALOG_FNAME_SIZE - 1] = '\0';
  if (has_file_info)
    strncpy(qr_dialog_size, sizeString, sizeof(qr_dialog_size) - 1);
  else
    qr_dialog_size[0] = '\0';
  qr_dialog_is_vpk = vpk;

  setDialogStep(DIALOG_STEP_QR_CONFIRM_TOUCH);
  while (getDialogStep() == DIALOG_STEP_QR_CONFIRM_TOUCH) {
    sceKernelDelayThread(10 * 1000);
  }

  if (getDialogStep() == DIALOG_STEP_NONE)
    goto EXIT;

  // Yes — start download
  char download_path[MAX_URL_LENGTH];
  char short_name[MAX_URL_LENGTH];
  int count = 0;

  char *ext = strrchr(fileName, '.');
  if (ext) {
    int len = ext - fileName;
    if (len > (int)sizeof(short_name) - 1)
      len = sizeof(short_name) - 1;
    strncpy(short_name, fileName, len);
    short_name[len] = '\0';
  } else {
    strncpy(short_name, fileName, sizeof(short_name) - 1);
    ext = "";
  }

  while (1) {
    if (count == 0)
      snprintf(download_path, sizeof(download_path) - 1, "ux0:download/%s", fileName);
    else
      snprintf(download_path, sizeof(download_path) - 1, "ux0:download/%s (%d)%s", short_name, count, ext);

    SceIoStat stat;
    memset(&stat, 0, sizeof(SceIoStat));
    if (sceIoGetstat(download_path, &stat) < 0)
      break;
    count++;
  }

  sceIoMkdir("ux0:download", 0006);
  strcpy(last_download, download_path);

  if (vpk)
    return downloadFileProcess(data, download_path, DIALOG_STEP_QR_DOWNLOADED_VPK);
  else
    return downloadFileProcess(data, download_path, DIALOG_STEP_QR_DOWNLOADED);

EXIT:
  return sceKernelExitDeleteThread(0);
}

int initQR() {
  SceKernelMemBlockType orig = vita2d_texture_get_alloc_memblock_type();
  vita2d_texture_set_alloc_memblock_type(SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW);
  camera_tex = vita2d_create_empty_texture(CAM_WIDTH, CAM_HEIGHT);
  vita2d_texture_set_alloc_memblock_type(orig);
  
  cam_info.size = sizeof(SceCameraInfo);
  cam_info.format = SCE_CAMERA_FORMAT_ABGR;
  cam_info.resolution = SCE_CAMERA_RESOLUTION_640_360;
  cam_info.pitch = vita2d_texture_get_stride(camera_tex) - (CAM_WIDTH << 2);
  cam_info.sizeIBase = (CAM_WIDTH * CAM_HEIGHT) << 2;
  cam_info.pIBase = vita2d_texture_get_datap(camera_tex);
  cam_info.framerate = 30;
  
  cam_info_read.size = sizeof(SceCameraRead);
  cam_info_read.mode = 0;
  if (sceCameraOpen(1, &cam_info) < 0) {
    qr_enabled = 0;
    vita2d_free_texture(camera_tex);
    return -1;
  }
  
  thid = sceKernelCreateThread("qr_decode_thread", qr_thread, 0x40, 0x100000, 0, 0, NULL);
  if (thid >= 0) sceKernelStartThread(thid, 0, NULL);
  qr_enabled = 1;
  return 0;
}

int finishQR() {
  sceKernelDeleteThread(thid);
  vita2d_free_texture(camera_tex);
  sceCameraClose(1);
  quirc_destroy(qr);
  return 0;
}

int startQR() {
  return sceCameraStart(1);
}

int stopQR() {
  int res = sceCameraStop(1);

  int y;
  for (y = 0; y < CAM_HEIGHT; y++) {
    int x;
    for (x = 0; x < CAM_WIDTH; x++) {
      ((uint32_t *)qr_data)[x + CAM_WIDTH * y] = 0;
    }
  }

  return res;
}

int renderCameraQR(int x, int y) {  
  sceCameraRead(1, &cam_info_read);
  vita2d_draw_texture(camera_tex, x, y);
  if (qr_next) {
    qr_data = (uint32_t *)vita2d_texture_get_datap(camera_tex);
    qr_next = 0;
  }
  return 0;
} 

char *getLastQR() {
  return data;
}

char *getLastDownloadQR() {
  return last_download;
}

int scannedQR() {
  return qr_scanned;
}

void setScannedQR(int s) {
  qr_scanned = s;
}

int enabledQR() {
  return qr_enabled;
}
