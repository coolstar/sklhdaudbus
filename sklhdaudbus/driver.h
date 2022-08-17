#if !defined(_SKLHDAUDBUS_H_)
#define _SKLHDAUDBUS_H_

#pragma warning(disable:4200)  // suppress nameless struct/union warning
#pragma warning(disable:4201)  // suppress nameless struct/union warning
#pragma warning(disable:4214)  // suppress bit field types other than int warning
#include <initguid.h>
#include <wdm.h>
#include <wdf.h>
#include <ntintsafe.h>
#include <ntstrsafe.h>

#include "fdo.h"
#include "buspdo.h"

#define DRIVERNAME "sklhdaudbus.sys: "
#define SKLHDAUDBUS_POOL_TAG 'SADH'

//
// Helper macros
//

#define DEBUG_LEVEL_ERROR   1
#define DEBUG_LEVEL_INFO    2
#define DEBUG_LEVEL_VERBOSE 3

#define DBG_INIT  1
#define DBG_PNP   2
#define DBG_IOCTL 4

static ULONG SklHdAudBusDebugLevel = 100;
static ULONG SklHdAudBusDebugCatagories = DBG_INIT || DBG_PNP || DBG_IOCTL;

#if 1
#define SklHdAudBusPrint(dbglevel, dbgcatagory, fmt, ...) {          \
    if (SklHdAudBusDebugLevel >= dbglevel &&                         \
        (SklHdAudBusDebugCatagories && dbgcatagory))                 \
		    {                                                           \
        DbgPrint(DRIVERNAME);                                   \
        DbgPrint(fmt, __VA_ARGS__);                             \
		    }                                                           \
}
#else
#define SklHdAudBusPrint(dbglevel, fmt, ...) {                       \
}
#endif
#endif