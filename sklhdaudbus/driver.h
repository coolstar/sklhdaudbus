#if !defined(_SKLHDAUDBUS_H_)
#define _SKLHDAUDBUS_H_

#pragma warning(disable:4200)  // suppress nameless struct/union warning
#pragma warning(disable:4201)  // suppress nameless struct/union warning
#pragma warning(disable:4214)  // suppress bit field types other than int warning
#include <ntddk.h>
#include <initguid.h>
#include <wdm.h>
#include <wdmguid.h>
#include <wdf.h>
#include <ntintsafe.h>
#include <ntstrsafe.h>

#include "fdo.h"
#include "buspdo.h"
#include "hda_registers.h"
#include "hdac_controller.h"
#include "hda_verbs.h"

#define DRIVERNAME "sklhdaudbus.sys: "
#define SKLHDAUDBUS_POOL_TAG 'SADH'

#define VEN_INTEL 0x8086

static UINT8 read8(PVOID addr) {
	UINT8 ret = *(UINT8*)addr;
	return ret;
}

static void write8(PVOID addr, UINT8 data) {
	*(UINT8*)addr = data;
}

static UINT16 read16(PVOID addr) {
	UINT16 ret = *(UINT16*)addr;
	return ret;
}

static void write16(PVOID addr, UINT16 data) {
	*(UINT16*)addr = data;
}

static UINT32 read32(PVOID addr) {
	UINT32 ret = *(UINT32 *)addr;
	return ret;
}

static void write32(PVOID addr, UINT32 data) {
	*(UINT32*)addr = data;
}

static void pci_read_cfg_byte(PBUS_INTERFACE_STANDARD pciInterface, UINT reg, BYTE *data) {
	if (!data) {
		return;
	}
	pciInterface->GetBusData(pciInterface->Context, PCI_WHICHSPACE_CONFIG, data, reg, sizeof(BYTE));
}

static void pci_read_cfg_dword(PBUS_INTERFACE_STANDARD pciInterface, UINT reg, UINT32* data) {
	if (!data) {
		return;
	}
	pciInterface->GetBusData(pciInterface->Context, PCI_WHICHSPACE_CONFIG, data, reg, sizeof(UINT32));
}

static void pci_write_cfg_byte(PBUS_INTERFACE_STANDARD pciInterface, UINT reg, BYTE data) {
	pciInterface->SetBusData(pciInterface->Context, PCI_WHICHSPACE_CONFIG, &data, reg, sizeof(BYTE));
}

static void pci_write_cfg_dword(PBUS_INTERFACE_STANDARD pciInterface, UINT reg, UINT32 data) {
	pciInterface->GetBusData(pciInterface->Context, PCI_WHICHSPACE_CONFIG, &data, reg, sizeof(UINT32));
}

static void update_pci_byte(PBUS_INTERFACE_STANDARD pciInterface, UINT reg, BYTE mask, BYTE val) {
	BYTE data;
	pci_read_cfg_byte(pciInterface, reg, &data);
	data &= ~mask;
	data |= (val & mask);
	pci_write_cfg_byte(pciInterface, reg, data);
}

#define hda_read8(ctx, reg) read16(ctx->m_BAR0.Base.baseptr + HDA_REG_##reg)
#define hda_write8(ctx, reg, data) write16(ctx->m_BAR0.Base.baseptr + HDA_REG_##reg, data)
#define hda_update8(ctx, reg, mask, val) hda_write8(ctx, reg, (hda_read8(ctx, reg) & ~(mask)) | (val))
#define hda_read16(ctx, reg) read16(ctx->m_BAR0.Base.baseptr + HDA_REG_##reg)
#define hda_write16(ctx, reg, data) write16(ctx->m_BAR0.Base.baseptr + HDA_REG_##reg, data)
#define hda_update16(ctx, reg, mask, val) hda_write16(ctx, reg, (hda_read16(ctx, reg) & ~(mask)) | (val))
#define hda_read32(ctx, reg) read32(ctx->m_BAR0.Base.baseptr + HDA_REG_##reg)
#define hda_write32(ctx, reg, data) write32(ctx->m_BAR0.Base.baseptr + HDA_REG_##reg, data)
#define hda_update32(ctx, reg, mask, val) hda_write32(ctx, reg, (hda_read32(ctx, reg) & ~(mask)) | (val))

#define IS_BXT(ven, dev) (ven == VEN_INTEL && dev == 0x5a98)

static void udelay(LONG usec) {
	LARGE_INTEGER Interval;
	Interval.QuadPart = -10 * usec;
	KeDelayExecutionThread(KernelMode, FALSE, &Interval);
}

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