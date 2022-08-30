#include "driver.h"
#include "adsp.h"

NTSTATUS ADSPGetResources(_In_ PVOID _context, _PCI_BAR* hdaBar, _PCI_BAR* adspBar, BUS_INTERFACE_STANDARD* pciConfig) {
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

	if (pciConfig) {
		*pciConfig = devData->FdoContext->BusInterface;
	}

	return STATUS_SUCCESS;
}

ADSP_BUS_INTERFACE ADSP_BusInterface(PVOID Context) {
	ADSP_BUS_INTERFACE busInterface;
	RtlZeroMemory(&busInterface, sizeof(ADSP_BUS_INTERFACE));

	busInterface.Size = sizeof(ADSP_BUS_INTERFACE);
	busInterface.Version = 1;
	busInterface.Context = Context;
	busInterface.InterfaceReference = WdfDeviceInterfaceReferenceNoOp;
	busInterface.InterfaceDereference = WdfDeviceInterfaceDereferenceNoOp;
	busInterface.GetResources = ADSPGetResources;

	return busInterface;
}