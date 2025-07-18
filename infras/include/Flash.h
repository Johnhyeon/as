/**
 * SSAS - Simple Smart Automotive Software
 * Copyright (C) 2021 Parai Wang <parai@foxmail.com>
 *
 * ref: http://portal.automotive-his.de/images/pdf/FlashProgramming/his%20flash%20driver%20v130.pdf
 */
#ifndef FLASH_H
#define FLASH_H
/* ================================ [ INCLUDES  ] ============================================== */
#ifdef __cplusplus
extern "C" {
#endif
/* ================================ [ MACROS    ] ============================================== */
/* software version information - only an example!!! */
#define FLASH_DRIVER_VERSION_MAJOR 0x01 /* major version number / interface */
#define FLASH_DRIVER_VERSION_MINOR 0x01 /* minor version number / internal */
#define FLASH_DRIVER_VERSION_PATCH 0x00 /* bugfix / patchlevel */

#define FLASH_DRIVER_INIT_OFFSET sizeof(unsigned long)
#define FLASH_DRIVER_DEINIT_OFFSET (sizeof(unsigned long) + sizeof(tFlashFct) * 1)
#define FLASH_DRIVER_ERASE_OFFSET (sizeof(unsigned long) + sizeof(tFlashFct) * 2)
#define FLASH_DRIVER_WRITE_OFFSET (sizeof(unsigned long) + sizeof(tFlashFct) * 3)
#define FLASH_DRIVER_READ_OFFSET (sizeof(unsigned long) + sizeof(tFlashFct) * 4)

/* sa holds flash driver start address,
 * pp contains pointer the pointer to the parameter structure */
#if defined(_WIN32) || defined(__linux__)
#define FLASH_DRIVER_INIT(sa, pp) FlashInit(pp)
#define FLASH_DRIVER_DEINIT(sa, pp) FlashDeinit(pp)
#define FLASH_DRIVER_ERASE(sa, pp) FlashErase(pp)
#define FLASH_DRIVER_WRITE(sa, pp) FlashWrite(pp)
#define FLASH_DRIVER_READ(sa, pp) FlashRead(pp)
#else
#define FLASH_DRIVER_INIT(sa, pp) (*(tFlashFct *)((sa) + FLASH_DRIVER_INIT_OFFSET))((pp))
#define FLASH_DRIVER_DEINIT(sa, pp) (*(tFlashFct *)((sa) + FLASH_DRIVER_DEINIT_OFFSET))((pp))
#define FLASH_DRIVER_ERASE(sa, pp) (*(tFlashFct *)((sa) + FLASH_DRIVER_ERASE_OFFSET))((pp))
#define FLASH_DRIVER_WRITE(sa, pp) (*(tFlashFct *)((sa) + FLASH_DRIVER_WRITE_OFFSET))((pp))
#define FLASH_DRIVER_READ(sa, pp) (*(tFlashFct *)((sa) + FLASH_DRIVER_READ_OFFSET))((pp))
#endif

/* standard error codes */
#define kFlashOk 0x00     /* called function succeeded */
#define kFlashFailed 0x01 /* called function failed */
/* controller specific error codes - only an example!!! */
#define kFlashInvalidParam 0x02
#define kFlashInvalidAddress 0x03
#define kFlashInvalidSize 0x04
#define kFlashInvalidData 0x05
#define kFlashNoProgClock 0x06
#define kFlashNoProgVoltage 0x07
#define kFlashPViol 0x08
#define kFlashAccerr 0x09

#define FLASH_DRIVER_RELOCATABLE
#ifndef FLASH_DRIVER_START_ADDRESS
#define FLASH_DRIVER_START_ADDRESS ((unsigned long)(&FlashHeader))
#endif
#define FLASH_DRIVER_NUMBER_OF_MODULES 1

/* configuration according to the mcu information */
#ifndef FLASH_ERASE_SIZE
#define FLASH_ERASE_SIZE 512
#endif
#ifndef FLASH_IS_ERASE_ADDRESS_ALIGNED
#define FLASH_IS_ERASE_ADDRESS_ALIGNED(a) (0 == ((FLASH_ERASE_SIZE - 1) & (a)))
#endif

/* must be n times of FLASH_WRITE_SIZE */
#ifndef FLASH_WRITE_SIZE
#define FLASH_WRITE_SIZE 8
#endif
#ifndef FLASH_IS_WRITE_ADDRESS_ALIGNED
#define FLASH_IS_WRITE_ADDRESS_ALIGNED(a) (0 == ((FLASH_WRITE_SIZE - 1) & (a)))
#endif

/* must be 1 or 2 */
#ifndef FLASH_READ_SIZE
#define FLASH_READ_SIZE 2
#endif
#ifndef FLASH_IS_READ_ADDRESS_ALIGNED
#define FLASH_IS_READ_ADDRESS_ALIGNED(a) (0 == ((FLASH_READ_SIZE - 1) & (a)))
#endif

#define FLASH_ALIGNED_ERASE_SIZE(s) (((s) + FLASH_ERASE_SIZE - 1) & (~(FLASH_ERASE_SIZE - 1)))
#define FLASH_ALIGNED_WRITE_SIZE(s) (((s) + FLASH_WRITE_SIZE - 1) & (~(FLASH_WRITE_SIZE - 1)))
#define FLASH_ALIGNED_READ_SIZE(s) (((s) + FLASH_READ_SIZE - 1) & (~(FLASH_READ_SIZE - 1)))

#ifndef NULL
#define NULL 0
#endif
/* ================================ [ TYPES     ] ============================================== */
typedef unsigned long tFlashAddress;
typedef unsigned long tFlashLength;

typedef unsigned char tFlashData;

typedef unsigned char tMajorVersion;  /* flash driver major version number */
typedef unsigned char tMinorVersion;  /* flash driver minor version number */
typedef unsigned char tBugfixVersion; /* bugfix / patchlevel number */
typedef unsigned short tFlashResult;  /* result of flash driver routines */
typedef unsigned long tAddress;       /* logical address */
typedef unsigned long tLength;        /* length (in bytes) */
typedef unsigned char tData;          /* ambiguous data */
typedef void (*tWDTriggerFct)(void);  /* watchdog trigger routine */

typedef struct {
  /* initialization: inout parameters */
  tBugfixVersion patchlevel; /* flash driver patch level version */
  tMajorVersion minornumber; /* flash driver minor version number */
  tMinorVersion majornumber; /* flash driver major version number */
  unsigned char reserved1;   /* reserved for future use, set to 0x00 for now */

  /* retrun value / error code: output parameters */
  tFlashResult errorcode;   /* return value / error code */
  unsigned short reserved2; /* reserved for future use, set to 0x0000 for now */

  /* erase / write: input parameters */
  tAddress address; /* logical target address */
  tLength length;   /* lenght information (in bytes) */
  tData *data;      /* pointer to data buffer */
  /* additional input parameters */
  tWDTriggerFct wdTriggerFct; /* pointer to watchdog trigger routine */
  /* erase / write: optional output parameters: debugging information */
  tData intendedData[2]; /* intented data at error address */
  tData actualData[2];   /* actual data at error address */
  tAddress errorAddress; /* address of error */
                         /* additonal controller / hardware specific parameters */
                         /* --> to be specified by flash driver vendor */
} tFlashParam;
typedef void (*tFlashFct)(tFlashParam *); /* prototype of flash driver routine */

typedef struct {
  union {
    unsigned long w;
    struct {
      unsigned MCU : 8; // LSB
      unsigned mask : 8;
      unsigned reserved : 8;
      unsigned version : 8; // MSB
    } W;
  } Info;
  tFlashFct Init;
  tFlashFct Deinit;
  tFlashFct Erase;
  tFlashFct Write;
  tFlashFct Read; /* add by parai, extended API */
#ifdef FLASH_HEADER_EXTENED_TYPE
  FLASH_HEADER_EXTENED_TYPE extend;
#endif
} tFlashHeader;
/* ================================ [ DECLARES  ] ============================================== */
extern const tFlashHeader FlashHeader;
/* ================================ [ DATAS     ] ============================================== */
/* ================================ [ LOCALS    ] ============================================== */
/* ================================ [ FUNCTIONS ] ============================================== */
extern void FlashInit(tFlashParam *FlashParam);
extern void FlashDeinit(tFlashParam *FlashParam);
extern void FlashErase(tFlashParam *FlashParam);
extern void FlashWrite(tFlashParam *FlashParam);
extern void FlashRead(tFlashParam *FlashParam);
#ifdef __cplusplus
}
#endif
#endif /* FLASH_H */
