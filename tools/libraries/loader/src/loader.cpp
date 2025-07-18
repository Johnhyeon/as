/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 */
/* ================================ [ INCLUDES  ] ============================================== */
#include "loader.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <string>
#include "Std_Timer.h"
#include <mutex>
#include <thread>
#include <chrono>
#include "PAL.h"

using namespace std::literals::chrono_literals;
/* ================================ [ MACROS    ] ============================================== */
#define LOADER_LOG_SIZE (1024 * 32)

#define LOADER_STS_CREATED 0
#define LOADER_STS_STARTED 1
#define LOADER_STS_EXITED 2

#ifndef LOADER_KEEP_TESTER_ONLINE
#define LOADER_KEEP_TESTER_ONLINE 2000000
#endif

#ifdef _WIN32
#define DLL ".dll"
#else
#define DLL ".so"
#endif

#ifndef LOADER_BUILT_IN_APP_MAX
#define LOADER_BUILT_IN_APP_MAX 32
#endif
/* ================================ [ TYPES     ] ============================================== */
typedef struct loader_s {
  srec_t *appSRec;
  srec_t *flsSRec;
  size_t totalSize;
  size_t lsz; /* log size */
  srec_sign_type_t signType;
  uint32_t funcAddr; /* functional address for CAN/FD only*/
  const loader_app_t *app;
  int progress;  /* resolution in 0.01% */
  int progRatio; /* for data transfer */
  std::thread thread;
  std::mutex logMutex;
  std::mutex tpMutex;
  int status;
  Std_TimerType testerTimer;
  uint8_t request[LOADER_MSG_SIZE];
  uint8_t response[LOADER_MSG_SIZE];
  char logs[LOADER_LOG_SIZE];
  isotp_t *isotp;
  boolean stop;
  int logLevel;
  void *dll;
  int result;
} loader_t;
/* ================================ [ DECLARES  ] ============================================== */
/* ================================ [ DATAS     ] ============================================== */
static const loader_app_t *lLoaderApps[LOADER_BUILT_IN_APP_MAX];
static uint32_t lLoaderAppsNum = 0;
/* ================================ [ LOCALS    ] ============================================== */
static void uds_keep_tester_online(loader_t *loader) {
  static const uint8_t testerKeep[2] = {0x3E, 0x00 | SUPPRESS_POS_RESP_BIT};
  uint32_t funcAddr = loader->funcAddr;

  if (Std_GetTimerElapsedTime(&loader->testerTimer) > LOADER_KEEP_TESTER_ONLINE) {
    if (funcAddr != 0) {
      isotp_ioctl(loader->isotp, ISOTP_IOCTL_SET_TX_ID, &funcAddr, sizeof(funcAddr));
    }

    isotp_transmit(loader->isotp, testerKeep, 2, NULL, 0);

    if (funcAddr != 0) {
      isotp_ioctl(loader->isotp, ISOTP_IOCTL_SET_TX_ID, &funcAddr, sizeof(funcAddr));
    }

    Std_TimerStart(&loader->testerTimer);
    std::this_thread::sleep_for(10ms);
  }
}

static int uds_request_service_impl(loader_t *loader, const uint8_t *data, size_t length,
                                    const int *expected, size_t eLen, int functional) {
  int r = L_R_OK;
  int rlen = 0;
  size_t i;
  uint32_t funcAddr = loader->funcAddr;
  static const uint8_t servicesWithSubFunc[] = {0x10, 0x28, 0x85, 0x3E};

  LDLOG(DEBUG, "\n request service %02X:\n", data[0]);
  LDHEX(DEBUG, "  TX:", data, length);

  std::lock_guard<std::mutex> lg(loader->tpMutex);
  if ((TRUE == functional) && (funcAddr != 0)) {
    /* switch to functional addressing mode */
    isotp_ioctl(loader->isotp, ISOTP_IOCTL_SET_TX_ID, &funcAddr, sizeof(funcAddr));
  }

  uds_keep_tester_online(loader);

  r = isotp_transmit(loader->isotp, data, length, NULL, 0);
  if (0 != r) {
    LDLOG(ERROR, "  isotp transmit error: %d\n", r);
  } else {
    for (i = 0; i < sizeof(servicesWithSubFunc); i++) {
      if ((length >= 2) && (servicesWithSubFunc[i] == data[0]) &&
          (0 != (data[1] & SUPPRESS_POS_RESP_BIT))) {
        r = L_R_OK_FOR_NOW;
        eLen = 0; /* as no reponse, no need to check exptected */
        std::this_thread::sleep_for(50ms);
        /* sleep 50 ms to wait the request done by all the addressed ECUs. */
        break;
      }
    }
  }
  while (L_R_OK == r) {
    rlen = isotp_receive(loader->isotp, loader->response, sizeof(loader->response));
    if (rlen > 0) {
      LDHEX(DEBUG, "  RX:", loader->response, (size_t)rlen);
    }
    if ((3 == rlen)) {
      if ((0x7F == loader->response[0]) && (loader->response[1] == data[0])) {
        if (0x78 == loader->response[2]) {
          /* pending response */
          uds_keep_tester_online(loader);
        } else {
          /* negative response */
          LDLOG(INFO, "  negative response %02X\n", loader->response[2]);
          r = L_R_NOK;
        }
      } else if ((data[0] | 0x40) == loader->response[0]) {
        r = L_R_OK_FOR_NOW;
      } else {
        r = L_R_NOK;
      }
    } else if (rlen > 0) {
      r = L_R_OK_FOR_NOW;
    } else {
      LDLOG(ERROR, "  isotp receive error: %d\n", rlen);
      r = L_R_ERROR;
    }
  }

  if (L_R_OK_FOR_NOW == r) {
    if ((size_t)rlen < eLen) {
      LDLOG(INFO, "  response too short\n");
      r = L_R_NOK;
    }
  }

  if (L_R_OK_FOR_NOW == r) {
    r = L_R_OK;
    for (i = 0; i < eLen; i++) {
      if ((-1 != expected[i]) && (loader->response[i] != (uint8_t)expected[i])) {
        LDLOG(INFO, "  response not as expected at %d R %02X != E %02X\n", (int)i,
              loader->response[i], (uint8_t)expected[i]);
        r = L_R_NOK;
      }
    }
  }

  if (L_R_OK == r) {
    loader->progress += 1; /* step 0.01% */
    LDLOG(DEBUG, "  PASS\n");
  } else {
    LDLOG(DEBUG, "  FAIL: %d\n", r);
  }

  if ((TRUE == functional) && (funcAddr != 0)) {
    /* switch back to physical addressing mode */
    isotp_ioctl(loader->isotp, ISOTP_IOCTL_SET_TX_ID, &funcAddr, sizeof(funcAddr));
  }

  return r;
}

static void loader_main(void *args) {
  int r = L_R_OK;
  size_t i;
  Std_TimerType timer;
  float cost = 0;
  float speed = 0;
  loader_t *loader = (loader_t *)args;
  const loader_app_t *app = loader->app;

  loader->status = LOADER_STS_STARTED;
  LDLOG(INFO, "loader %s started:\n", app->name);
  LDLOG(INFO, "Total data size %llu bytes\n", loader->totalSize);
  Std_TimerStart(&timer);
  Std_TimerStart(&loader->testerTimer);

  loader->progRatio = 9900;

  for (i = 0; i < app->numOfServices; i++) {
    loader->progRatio -= app->services[i].progress;
  }

  for (i = 0; (L_R_OK == r) && (i < app->numOfServices && (FALSE == loader->stop)); i++) {
    if (NULL != app->services[i].preLog) {
      LDLOG(INFO, app->services[i].preLog);
    }
    r = app->services[i].handle(loader);
    if (0 == r) {
      if (NULL != app->services[i].postLog) {
        LDLOG(INFO, app->services[i].postLog);
      }
      loader->progress += app->services[i].progress;
    }
  }

  if (L_R_OK == r) {
    cost = Std_GetTimerElapsedTime(&timer) / 1000000.0;
    speed = loader->totalSize / 1024.0 / cost;
    LDLOG(INFO, "loader average speed %.2f kbps, cost %.2f seconds\n", speed, cost);
  } else {
    LDLOG(INFO, "loader failed\n");
  }

  loader->status = LOADER_STS_EXITED;
  loader->result = r;
}
/* ================================ [ FUNCTIONS ] ============================================== */
loader_t *loader_create(loader_args_t *args) {
  int r = 0;
  loader_t *loader;
  loader = new loader_t;
  uint32_t i;
  char *path;
  loader_get_app_func_t get_app_func;

  if (NULL != loader) {
    loader->isotp = args->isotp;
    loader->appSRec = args->appSRec;
    loader->flsSRec = args->flsSRec;
    loader->totalSize = args->appSRec->totalSize;
    if (loader->flsSRec != NULL) {
      loader->totalSize += args->flsSRec->totalSize;
    }
    loader->signType = args->signType;
    loader->funcAddr = args->funcAddr;
    loader->logLevel = L_LOG_INFO;
    loader->status = LOADER_STS_CREATED;
    loader->stop = FALSE;
    loader->app = NULL;
    loader->dll = NULL;

    for (i = 0; i < lLoaderAppsNum; i++) {
      if (0 == strcmp(args->choice, lLoaderApps[i]->name)) {
        loader->app = lLoaderApps[i];
        break;
      }
    }

    if (NULL == loader->app) {
      path = (char *)loader->request;
      snprintf(path, sizeof(loader->request), "Loader%s" DLL, args->choice);
      loader->dll = PAL_DlOpen(path);
      if (NULL != loader->dll) {
        std::string interface = std::string(args->choice) + "Get";
        get_app_func = (loader_get_app_func_t)PAL_DlSym(loader->dll, interface.c_str());
        if (NULL != get_app_func) {
          loader->app = get_app_func();
        }
        if (NULL == loader->app) {
          PAL_DlClose(loader->dll);
          printf("loader application %s is invalid\n", path);
          r = -EINVAL;
        }
      } else {
        printf("loader application %s doesn't exist\n", path);
        r = -EEXIST;
      }
    }

    if (NULL == loader->app) {
      printf("invalid app %s, choose from:", args->choice);
      for (i = 0; i < lLoaderAppsNum; i++) {
        printf(" %s", lLoaderApps[i]->name);
      }
      printf("\n");
      r = -EEXIST;
    }

    if (0 == r) {
      loader->thread = std::thread(loader_main, (void *)loader);
    }

    if (0 != r) {
      delete loader;
      loader = NULL;
      printf("loader start failed with error = %d\n", r);
    }
  }

  return loader;
}

void loader_set_log_level(loader_t *loader, int level) {
  loader->logLevel = level;
}

void loader_log(loader_t *loader, int level, const char *fmt, ...) {
  va_list args;
  if ((level) >= loader->logLevel) {
    std::lock_guard<std::mutex> lg(loader->logMutex);
    va_start(args, fmt);
    loader->lsz +=
      vsnprintf(&loader->logs[loader->lsz], sizeof(loader->logs) - loader->lsz, fmt, args);
    va_end(args);
  }
}

void loader_log_hex(loader_t *loader, int level, const char *prefix, const uint8_t *data,
                    size_t len) {
  size_t i;
  if (level >= loader->logLevel) {
    std::lock_guard<std::mutex> lg(loader->logMutex);
    loader->lsz += snprintf(&loader->logs[loader->lsz], sizeof(loader->logs) - loader->lsz, prefix);
    for (i = 0; (i < len) && (i < (size_t)16); i++) {
      loader->lsz +=
        snprintf(&loader->logs[loader->lsz], sizeof(loader->logs) - loader->lsz, " %02X", data[i]);
    }
    loader->lsz += snprintf(&loader->logs[loader->lsz], sizeof(loader->logs) - loader->lsz, "\n");
  }
}

int loader_poll(loader_t *loader, int *progress, char **msg) {
  int r = 0;

  *msg = NULL;
  *progress = loader->progress;
  if (*progress >= 10000) {
    *progress = 9900;
  }

  if (LOADER_STS_EXITED == loader->status) {
    r = loader->result;
    if (0 == r) {
      *progress = 10000;
    }
  }

  std::lock_guard<std::mutex> lg(loader->logMutex);
  if (loader->lsz > 0) {
    *msg = strdup(loader->logs);
    loader->lsz = 0;
  }

  return r;
}

void loader_destory(loader_t *loader) {
  loader->stop = TRUE;
  if (loader->thread.joinable()) {
    loader->thread.join();
  }
  if (nullptr != loader->dll) {
    PAL_DlClose(loader->dll);
  }

  delete loader;
}

int uds_request_service(loader_t *loader, const uint8_t *data, size_t length, const int *expected,
                        size_t eLen) {
  return uds_request_service_impl(loader, data, length, expected, eLen, FALSE);
}

int uds_broadcast_service(loader_t *loader, const uint8_t *data, size_t length, const int *expected,
                          size_t eLen) {
  return uds_request_service_impl(loader, data, length, expected, eLen, TRUE);
}

uint8_t *loader_get_request(loader_t *loader) {
  return loader->request;
}

uint8_t *loader_get_response(loader_t *loader) {
  return loader->response;
}

loader_crc_t loader_crc_init(loader_t *loader) {
  loader_crc_t crc = 0;
  switch (loader->signType) {
  case SREC_SIGN_CRC16:
  case SREC_SIGN_CRC16_V2:
    crc = 0xFFFF;
    break;
  case SREC_SIGN_CRC32:
  case SREC_SIGN_CRC32_V2:
    crc = 0xFFFFFFFF;
    break;
  default:
    break;
  }
  return crc;
}

loader_crc_t loader_calulate_crc(loader_t *loader, const uint8_t *DataPtr, uint32_t Length,
                                 loader_crc_t StartValue, boolean IsFirstCall) {
  loader_crc_t crc;
  switch (loader->signType) {
  case SREC_SIGN_CRC16:
  case SREC_SIGN_CRC16_V2:
    crc = Crc_CalculateCRC16(DataPtr, Length, (uint16_t)StartValue, IsFirstCall);
    break;
  case SREC_SIGN_CRC32:
  case SREC_SIGN_CRC32_V2:
    crc = Crc_CalculateCRC32(DataPtr, Length, (uint32_t)StartValue, IsFirstCall);
    break;
  default:
    break;
  }
  return crc;
}

uint32_t loader_set_crc(loader_t *loader, uint8_t *DataPtr, loader_crc_t Crc) {
  uint32_t len = 0;
  switch (loader->signType) {
  case SREC_SIGN_CRC16:
  case SREC_SIGN_CRC16_V2:
    DataPtr[0] = (Crc >> 8) & 0xFF;
    DataPtr[1] = Crc & 0xFF;
    len = 2;
    break;
  case SREC_SIGN_CRC32:
  case SREC_SIGN_CRC32_V2:
    DataPtr[0] = (Crc >> 24) & 0xFF;
    DataPtr[1] = (Crc >> 16) & 0xFF;
    DataPtr[2] = (Crc >> 8) & 0xFF;
    DataPtr[3] = Crc & 0xFF;
    len = 4;
    break;
  default:
    break;
  }
  return len;
}

boolean loader_is_stopt(loader_t *loader) {
  return loader->stop;
}

void loader_add_progress(loader_t *loader, uint32_t doSize) {
  loader->progress += (uint32_t)(((uint64_t)doSize * loader->progRatio) / loader->totalSize);
}

srec_t *loader_get_app_srec(loader_t *loader) {
  return loader->appSRec;
}

srec_t *loader_get_flsdrv_srec(loader_t *loader) {
  return loader->flsSRec;
}

void loader_register_app(const loader_app_t *app) {
  if (lLoaderAppsNum < ARRAY_SIZE(lLoaderApps)) {
    lLoaderApps[lLoaderAppsNum] = app;
    lLoaderAppsNum++;
  }
}
