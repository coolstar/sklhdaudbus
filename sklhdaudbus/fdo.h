#if !defined(_SKLHDAUDBUS_FDO_H_)
#define _SKLHDAUDBUS_FDO_H_

union baseaddr {
    PVOID Base;
    UINT8* baseptr;
};

typedef struct _PCI_BAR {
    union baseaddr Base;
    ULONG Len;
} PCI_BAR, * PPCI_BAR;

#include "adsp.h"

#define HDA_UNSOL_QUEUE_SIZE	64
#define MAX_NOTIF_EVENTS 16
#define HDA_MAX_CODECS		8	/* limit by controller side */

struct _FDO_CONTEXT;
struct _PDO_DEVICE_DATA;

typedef struct _HDAC_STREAM_CALLBACK {
    BOOLEAN InUse;
    PDEVICE_OBJECT Fdo;
    PHDAUDIO_DMA_NOTIFICATION_CALLBACK NotificationCallback;
    PVOID CallbackContext;
} HDAC_STREAM_CALLBACK, *PHDAC_STREAM_CALLBACK;

typedef struct _HDAC_STREAM {
    struct _FDO_CONTEXT* FdoContext;
    struct _PDO_DEVICE_DATA* PdoContext;

    PMDL mdlBuf;
    UINT32* posbuf;

    UINT32* bdl;

    BOOLEAN stripe;
    int direction;

    UINT32 bufSz;
    UINT32 periodBytes;
    UINT32 fifoSize;
    int frags;

    UINT8* sdAddr;
    UINT32 int_sta_mask;

    UINT8* pphc_addr; //ppcap
    UINT8* pplc_addr; //ppcap

    UINT8* spib_addr; //spbcap

    UINT8 streamTag;
    UINT8 idx;

    HDAUDIO_STREAM_FORMAT streamFormat;

    PKEVENT registeredEvents[MAX_NOTIF_EVENTS];
    HDAC_STREAM_CALLBACK registeredCallbacks[MAX_NOTIF_EVENTS];

    BOOLEAN running;
    BOOLEAN irqReceived;
} HDAC_STREAM, *PHDAC_STREAM;

typedef struct _HDAC_RB {
    UINT32 *buf;
    PHYSICAL_ADDRESS addr;
    USHORT rp, wp;
    int cmds[HDA_MAX_CODECS];
    UINT32 res[HDA_MAX_CODECS];
} HDAC_RB, *PHDAC_RB;

typedef struct _FDO_CONTEXT
{
    WDFDEVICE WdfDevice;

    UINT16 venId;
    UINT16 devId;
    UINT8 revId;

    PCI_BAR m_BAR0; //required
    PCI_BAR m_BAR4; //Intel AudioDSP
    BUS_INTERFACE_STANDARD BusInterface; //PCI Bus Interface
    WDFINTERRUPT Interrupt;

    UINT8* ppcap;
    UINT8* spbcap;

    BOOLEAN is64BitOK;
    UINT32 hwVersion;

    UINT32 captureIndexOff;
    UINT32 playbackIndexOff;
    UINT32 captureStreams;
    UINT32 playbackStreams;
    UINT32 numStreams;

    PHDAC_STREAM streams;
    struct _PDO_DEVICE_DATA* codecs[HDA_MAX_CODECS];

    PADSP_INTERRUPT_CALLBACK dspInterruptCallback;
    PVOID dspInterruptContext;
    PVOID nhlt;
    UINT64 nhltSz;
    PVOID sofTplg;
    UINT64 sofTplgSz;

    //unsolicited events
    UINT32 unsol_queue[HDA_UNSOL_QUEUE_SIZE * 2];
    UINT unsol_rp, unsol_wp;
    BOOL processUnsol;

    //bit flags of detected codecs
    UINT16 codecMask;
    USHORT numCodecs;

    HDAC_RB corb;
    HDAC_RB rirb;
    unsigned int last_cmd[HDA_MAX_CODECS];
    WDFWAITLOCK cmdLock;

    PVOID rb; //CORB and RIRB buffers
    PVOID posbuf;
} FDO_CONTEXT, * PFDO_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(FDO_CONTEXT, Fdo_GetContext)

NTSTATUS
Fdo_Create(
	_Inout_ PWDFDEVICE_INIT DeviceInit
);

#endif
