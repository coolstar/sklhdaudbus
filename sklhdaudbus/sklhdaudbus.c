#include "driver.h"

NTSTATUS
SklHdAudBusEvtDeviceAdd(
	_In_ WDFDRIVER Driver,
	_Inout_ PWDFDEVICE_INIT DeviceInit
)
{
	NTSTATUS status;
	status = Fdo_Create(DeviceInit);
	return status;
}

NTSTATUS
DriverEntry(
__in PDRIVER_OBJECT  DriverObject,
__in PUNICODE_STRING RegistryPath
)
{
	WDF_DRIVER_CONFIG config;
	NTSTATUS status;
	WDFDRIVER wdfDriver;

	SklHdAudBusPrint(DEBUG_LEVEL_INFO, DBG_INIT,
		"Driver Entry\n");

	WDF_DRIVER_CONFIG_INIT(&config, SklHdAudBusEvtDeviceAdd);

	status = WdfDriverCreate(DriverObject,
		RegistryPath,
		WDF_NO_OBJECT_ATTRIBUTES,
		&config,
		&wdfDriver
	);
	if (!NT_SUCCESS(status))
	{
		SklHdAudBusPrint(DEBUG_LEVEL_ERROR, DBG_INIT,
			"WdfDriverCreate failed %x\n", status);
	}

	return status;
}
