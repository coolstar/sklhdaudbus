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

	return busInterface;
}