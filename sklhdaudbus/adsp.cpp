#include "driver.h"
#define ADSP_DECL 1
#include "adsp.h"

NTSTATUS ADSPGetResources(_In_ PVOID _context, _PCI_BAR* hdaBar, _PCI_BAR* adspBar, PVOID *ppcap, BUS_INTERFACE_STANDARD* pciConfig) {
	if (!_context)
		return STATUS_NO_SUCH_DEVICE;

	NTSTATUS status = STATUS_SUCCESS;

	PPDO_DEVICE_DATA devData = (PPDO_DEVICE_DATA)_context;
	if (!devData->FdoContext) {
		return STATUS_NO_SUCH_DEVICE;
	}

	if (hdaBar) {
		*hdaBar = devData->FdoContext->m_BAR0;
	}

	if (adspBar) {
		*adspBar = devData->FdoContext->m_BAR4;
	}

	if (adspBar) {
		*adspBar = devData->FdoContext->m_BAR4;
	}

	if (ppcap) {
		*ppcap = devData->FdoContext->ppcap;
	}

	if (pciConfig) {
		*pciConfig = devData->FdoContext->BusInterface;
	}

	return STATUS_SUCCESS;
}


NTSTATUS ADSPRegisterInterrupt(_In_ PVOID _context, _In_ PADSP_INTERRUPT_CALLBACK callback, _In_ PVOID callbackContext) {
	if (!_context)
		return STATUS_NO_SUCH_DEVICE;

	NTSTATUS status = STATUS_SUCCESS;

	PPDO_DEVICE_DATA devData = (PPDO_DEVICE_DATA)_context;
	if (!devData->FdoContext) {
		return STATUS_NO_SUCH_DEVICE;
	}

	devData->FdoContext->dspInterruptCallback = callback;
	devData->FdoContext->dspInterruptContext = callbackContext;
	return STATUS_SUCCESS;
}

NTSTATUS ADSPUnregisterInterrupt(_In_ PVOID _context) {
	if (!_context)
		return STATUS_NO_SUCH_DEVICE;

	NTSTATUS status = STATUS_SUCCESS;

	PPDO_DEVICE_DATA devData = (PPDO_DEVICE_DATA)_context;
	if (!devData->FdoContext) {
		return STATUS_NO_SUCH_DEVICE;
	}

	devData->FdoContext->dspInterruptCallback = NULL;
	devData->FdoContext->dspInterruptContext = NULL;
	return STATUS_SUCCESS;
}

NTSTATUS ADSPGetRenderStream(_In_ PVOID _context, HDAUDIO_STREAM_FORMAT StreamFormat, PHANDLE Handle, _Out_ UINT8* streamTag) {
	if (!_context)
		return STATUS_NO_SUCH_DEVICE;

	PPDO_DEVICE_DATA devData = (PPDO_DEVICE_DATA)_context;
	if (!devData->FdoContext) {
		return STATUS_NO_SUCH_DEVICE;
	}

	PFDO_CONTEXT fdoContext = devData->FdoContext;

	WdfInterruptAcquireLock(devData->FdoContext->Interrupt);
	for (int i = 0; i < fdoContext->playbackStreams; i++) {
		int tag = fdoContext->playbackIndexOff + i;
		PHDAC_STREAM stream = &fdoContext->streams[tag];
		if (stream->PdoContext != NULL) {
			continue;
		}

		stream->stripe = FALSE;
		stream->PdoContext = devData;
		stream->prepared = FALSE;
		stream->running = FALSE;
		stream->streamFormat = StreamFormat;

		int mask = HDA_PPCTL_PROCEN(stream->idx);
		UINT32 val = 0;
		val = read16(fdoContext->ppcap + HDA_REG_PP_PPCTL) & mask;

		if (!val) {
			DbgPrint("Decoupled stream\n");
			hdac_update32(fdoContext->ppcap, HDA_REG_PP_PPCTL, mask, mask);
		}
		DbgPrint("Stream index: %d, tag: %d\n", stream->idx, stream->streamTag);

		if (fdoContext->spbcap) {
			stream->spib_addr = fdoContext->spbcap + HDA_SPB_BASE + (HDA_SPB_INTERVAL * stream->idx) + HDA_SPB_SPIB;
			DbgPrint("SPIB offset: 0x%x\n", HDA_SPB_BASE + (HDA_SPB_INTERVAL * stream->idx) + HDA_SPB_SPIB);
		}

		if (Handle)
			*Handle = (HANDLE)stream;
		if (streamTag)
			*streamTag = stream->streamTag;

		WdfInterruptReleaseLock(devData->FdoContext->Interrupt);
		return STATUS_SUCCESS;
	}

	WdfInterruptReleaseLock(devData->FdoContext->Interrupt);
	return STATUS_INSUFFICIENT_RESOURCES;
}

NTSTATUS ADSPFreeRenderStream(
	_In_ PVOID _context,
	_In_ HANDLE Handle
) {
	SklHdAudBusPrint(DEBUG_LEVEL_VERBOSE, DBG_IOCTL, "%s called!\n", __func__);

	PPDO_DEVICE_DATA devData = (PPDO_DEVICE_DATA)_context;
	if (!devData->FdoContext) {
		return STATUS_NO_SUCH_DEVICE;
	}

	PHDAC_STREAM stream = (PHDAC_STREAM)Handle;
	if (stream->PdoContext != devData) {
		return STATUS_INVALID_HANDLE;
	}

	WdfInterruptAcquireLock(devData->FdoContext->Interrupt);

	if (stream->prepared || stream->running) {
		WdfInterruptReleaseLock(devData->FdoContext->Interrupt);
		return STATUS_INVALID_DEVICE_REQUEST;
	}

	PFDO_CONTEXT fdoContext = devData->FdoContext;
	int mask = HDA_PPCTL_PROCEN(stream->idx);
	UINT32 val = 0;
	val = read16(fdoContext->ppcap + HDA_REG_PP_PPCTL) & mask;

	if (val) {
		DbgPrint("Re-coupled stream\n");
		hdac_update32(fdoContext->ppcap, HDA_REG_PP_PPCTL, mask, 0);
	}

	stream->PdoContext = NULL;
	WdfInterruptReleaseLock(devData->FdoContext->Interrupt);

	return STATUS_SUCCESS;
}

NTSTATUS ADSPPrepareDSP(
	_In_ PVOID _context,
	_In_ HANDLE Handle,
	_In_ unsigned int ByteSize,
	_Out_ PVOID* mdlBuf
) {
	PPDO_DEVICE_DATA devData = (PPDO_DEVICE_DATA)_context;
	if (!devData->FdoContext) {
		return STATUS_NO_SUCH_DEVICE;
	}

	PHDAC_STREAM stream = (PHDAC_STREAM)Handle;
	if (stream->PdoContext != devData) {
		return STATUS_INVALID_HANDLE;
	}

	WdfInterruptAcquireLock(devData->FdoContext->Interrupt);

	if (stream->prepared || stream->running) {
		WdfInterruptReleaseLock(devData->FdoContext->Interrupt);
		return STATUS_DEVICE_BUSY;
	}

	WdfInterruptReleaseLock(devData->FdoContext->Interrupt);

	PHYSICAL_ADDRESS zeroAddr;
	zeroAddr.QuadPart = 0;
	PHYSICAL_ADDRESS maxAddr;
	maxAddr.QuadPart = MAXUINT64;
	PMDL mdl = MmAllocatePagesForMdl(zeroAddr, maxAddr, zeroAddr, ByteSize);
	if (!mdl) {
		return STATUS_NO_MEMORY;
	}

	stream->mdlBuf = mdl;
	stream->bufSz = min(ByteSize, MmGetMdlByteCount(mdl));
	stream->virtAddr = (UINT8*)MmMapLockedPagesSpecifyCache(mdl, KernelMode, MmWriteCombined, NULL, FALSE, MdlMappingNoExecute | NormalPagePriority);

	DbgPrint("Requested %d bytes, got %d. Using %d\n", ByteSize, MmGetMdlByteCount(mdl), stream->bufSz);

	hdac_stream_reset(stream);

	/* reset BDL address */
	stream_write32(stream, SD_BDLPL, 0);
	stream_write32(stream, SD_BDLPU, 0);

	hdac_stream_setup(stream);

	if (mdlBuf)
		*mdlBuf = stream->virtAddr;
	return STATUS_SUCCESS;
}

NTSTATUS ADSPCleanupDSP(_In_ PVOID _context, _In_ HANDLE Handle) {
	PPDO_DEVICE_DATA devData = (PPDO_DEVICE_DATA)_context;
	if (!devData->FdoContext) {
		return STATUS_NO_SUCH_DEVICE;
	}

	PHDAC_STREAM stream = (PHDAC_STREAM)Handle;
	if (stream->PdoContext != devData) {
		return STATUS_INVALID_HANDLE;
	}

	WdfInterruptAcquireLock(devData->FdoContext->Interrupt);

	stream_write32(stream, SD_BDLPL, 0);
	stream_write32(stream, SD_BDLPU, 0);
	stream_write32(stream, SD_CTL, 0);

	if (stream->virtAddr) {
		MmUnmapLockedPages(stream->virtAddr, stream->mdlBuf);
		stream->virtAddr = NULL;
	}
	MmFreePagesFromMdlEx(stream->mdlBuf, MM_DONT_ZERO_ALLOCATION);
	ExFreePool(stream->mdlBuf);
	stream->mdlBuf = NULL;

	WdfInterruptReleaseLock(devData->FdoContext->Interrupt);

	return STATUS_SUCCESS;
}

void ADSPStartStopDSP(_In_ PVOID _context, _In_ HANDLE Handle, BOOL startStop) {
	PPDO_DEVICE_DATA devData = (PPDO_DEVICE_DATA)_context;
	if (!devData->FdoContext) {
		return;
	}

	PHDAC_STREAM stream = (PHDAC_STREAM)Handle;
	if (stream->PdoContext != devData) {
		return;
	}

	DbgPrint("Toggling DSP load stream: %d\n", startStop);

	if (startStop)
		hdac_stream_start(stream);
	else
		hdac_stream_stop(stream);
}

void ADSPEnableSPIB(_In_ PVOID _context, _In_ HANDLE Handle, UINT32 value) {
	PPDO_DEVICE_DATA devData = (PPDO_DEVICE_DATA)_context;
	if (!devData->FdoContext) {
		return;
	}

	PHDAC_STREAM stream = (PHDAC_STREAM)Handle;
	if (stream->PdoContext != devData) {
		return;
	}

	if (!devData->FdoContext->spbcap) {
		return;
	}

	UINT32 mask = (1 << stream->idx);
	hdac_update32(devData->FdoContext->spbcap, HDA_REG_SPB_SPBFCCTL, mask, mask);

	write32(stream->spib_addr, value);

	DbgPrint("Enabled SPIB: %d\n", value);
}

void ADSPDisableSPIB(_In_ PVOID _context, _In_ HANDLE Handle) {
	PPDO_DEVICE_DATA devData = (PPDO_DEVICE_DATA)_context;
	if (!devData->FdoContext) {
		return;
	}

	PHDAC_STREAM stream = (PHDAC_STREAM)Handle;
	if (stream->PdoContext != devData) {
		return;
	}

	if (!devData->FdoContext->spbcap) {
		return;
	}

	UINT32 mask = (1 << stream->idx);
	hdac_update32(devData->FdoContext->spbcap, HDA_REG_SPB_SPBFCCTL, mask, 0);

	write32(stream->spib_addr, 0);

	DbgPrint("Disabled SPIB\n");
}

ADSP_BUS_INTERFACE ADSP_BusInterface(PVOID Context) {
	ADSP_BUS_INTERFACE busInterface;
	RtlZeroMemory(&busInterface, sizeof(ADSP_BUS_INTERFACE));

	PPDO_DEVICE_DATA devData = (PPDO_DEVICE_DATA)Context;

	busInterface.Size = sizeof(ADSP_BUS_INTERFACE);
	busInterface.Version = 1;
	busInterface.Context = Context;
	busInterface.InterfaceReference = WdfDeviceInterfaceReferenceNoOp;
	busInterface.InterfaceDereference = WdfDeviceInterfaceDereferenceNoOp;
	busInterface.CtlrDevId = devData->CodecIds.CtlrDevId;
	busInterface.GetResources = ADSPGetResources;
	busInterface.RegisterInterrupt = ADSPRegisterInterrupt;
	busInterface.UnregisterInterrupt = ADSPUnregisterInterrupt;

	busInterface.GetRenderStream = ADSPGetRenderStream;
	busInterface.FreeRenderStream = ADSPFreeRenderStream;
	busInterface.PrepareDSP = ADSPPrepareDSP;
	busInterface.CleanupDSP = ADSPCleanupDSP;
	busInterface.TriggerDSP = ADSPStartStopDSP;
	busInterface.DSPEnableSPIB = ADSPEnableSPIB;
	busInterface.DSPDisableSPIB = ADSPDisableSPIB;

	return busInterface;
}