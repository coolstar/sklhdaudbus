#include "driver.h"

EVT_WDF_DEVICE_PREPARE_HARDWARE Fdo_EvtDevicePrepareHardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE Fdo_EvtDeviceReleaseHardware;
EVT_WDF_DEVICE_D0_ENTRY Fdo_EvtDeviceD0Entry;
EVT_WDF_DEVICE_D0_EXIT Fdo_EvtDeviceD0Exit;
NTSTATUS
Fdo_Initialize(
    _In_ PFDO_CONTEXT FdoCtx
);

NTSTATUS
Fdo_Create(
	_Inout_ PWDFDEVICE_INIT DeviceInit
)
{
	WDF_OBJECT_ATTRIBUTES attributes;
	WDF_PNPPOWER_EVENT_CALLBACKS pnpPowerCallbacks;
    PFDO_CONTEXT fdoCtx;
    WDFDEVICE wdfDevice;
    NTSTATUS status;

    SklHdAudBusPrint(DEBUG_LEVEL_INFO, DBG_INIT,
        "%s\n", __func__);

    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);
    pnpPowerCallbacks.EvtDevicePrepareHardware = Fdo_EvtDevicePrepareHardware;
    pnpPowerCallbacks.EvtDeviceReleaseHardware = Fdo_EvtDeviceReleaseHardware;
    pnpPowerCallbacks.EvtDeviceD0Entry = Fdo_EvtDeviceD0Entry;
    pnpPowerCallbacks.EvtDeviceD0Exit = Fdo_EvtDeviceD0Exit;
    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpPowerCallbacks);

    WdfDeviceInitSetPowerPageable(DeviceInit);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, FDO_CONTEXT);
    status = WdfDeviceCreate(&DeviceInit, &attributes, &wdfDevice);
    if (!NT_SUCCESS(status)) {
        SklHdAudBusPrint(DEBUG_LEVEL_ERROR, DBG_INIT,
            "WdfDriverCreate failed %x\n", status);
        goto Exit;
    }

    fdoCtx = Fdo_GetContext(wdfDevice);
    fdoCtx->WdfDevice = wdfDevice;

    status = Fdo_Initialize(fdoCtx);
    if (!NT_SUCCESS(status))
    {
        goto Exit;
    }

Exit:
    return status;
}

NTSTATUS
Fdo_Initialize(
    _In_ PFDO_CONTEXT FdoCtx
)
{
    NTSTATUS status;
    WDFDEVICE device;

    device = FdoCtx->WdfDevice;

    SklHdAudBusPrint(DEBUG_LEVEL_INFO, DBG_INIT,
        "%s\n", __func__);

    return STATUS_SUCCESS;
}

NTSTATUS
Fdo_EvtDevicePrepareHardware(
    _In_ WDFDEVICE Device,
    _In_ WDFCMRESLIST ResourcesRaw,
    _In_ WDFCMRESLIST ResourcesTranslated
)
{
    NTSTATUS status;
    PFDO_CONTEXT fdoCtx;
    ULONG resourceCount;

    fdoCtx = Fdo_GetContext(Device);
    resourceCount = WdfCmResourceListGetCount(ResourcesTranslated);

    SklHdAudBusPrint(DEBUG_LEVEL_INFO, DBG_INIT,
        "%s\n", __func__);

    //TODO: MAP PCI Bars + IRQ
    status = STATUS_SUCCESS;

    return status;
}

NTSTATUS
Fdo_EvtDeviceReleaseHardware(
    _In_ WDFDEVICE Device,
    _In_ WDFCMRESLIST ResourcesTranslated
)
{
    PFDO_CONTEXT fdoCtx;

    UNREFERENCED_PARAMETER(ResourcesTranslated);

    fdoCtx = Fdo_GetContext(Device);

    SklHdAudBusPrint(DEBUG_LEVEL_INFO, DBG_INIT,
        "%s\n", __func__);

    return STATUS_SUCCESS;
}

NTSTATUS
Fdo_EvtDeviceD0Entry(
    _In_ WDFDEVICE Device,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
)
{
    NTSTATUS status;
    PFDO_CONTEXT fdoCtx;

    fdoCtx = Fdo_GetContext(Device);

    SklHdAudBusPrint(DEBUG_LEVEL_INFO, DBG_INIT,
        "%s\n", __func__);

    status = STATUS_SUCCESS;

    return status;
}

NTSTATUS
Fdo_EvtDeviceD0Exit(
    _In_ WDFDEVICE Device,
    _In_ WDF_POWER_DEVICE_STATE TargetState
)
{
    NTSTATUS status;
    PFDO_CONTEXT fdoCtx;

    fdoCtx = Fdo_GetContext(Device);

    SklHdAudBusPrint(DEBUG_LEVEL_INFO, DBG_INIT,
        "%s\n", __func__);

    status = STATUS_SUCCESS;

    return status;
}