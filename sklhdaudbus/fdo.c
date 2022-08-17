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
    WDF_CHILD_LIST_CONFIG      config;
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

    //
    // WDF_ DEVICE_LIST_CONFIG describes how the framework should handle
    // dynamic child enumeration on behalf of the driver writer.
    // Since we are a bus driver, we need to specify identification description
    // for our child devices. This description will serve as the identity of our
    // child device. Since the description is opaque to the framework, we
    // have to provide bunch of callbacks to compare, copy, or free
    // any other resources associated with the description.
    //
    WDF_CHILD_LIST_CONFIG_INIT(&config,
        sizeof(PDO_IDENTIFICATION_DESCRIPTION),
        Bus_EvtDeviceListCreatePdo // callback to create a child device.
    );

    //
    // This function pointer will be called when the framework needs to copy a
    // identification description from one location to another.  An implementation
    // of this function is only necessary if the description contains description
    // relative pointer values (like  LIST_ENTRY for instance) .
    // If set to NULL, the framework will use RtlCopyMemory to copy an identification .
    // description. In this sample, it's not required to provide these callbacks.
    // they are added just for illustration.
    //
    config.EvtChildListIdentificationDescriptionDuplicate =
        Bus_EvtChildListIdentificationDescriptionDuplicate;

    //
    // This function pointer will be called when the framework needs to compare
    // two identificaiton descriptions.  If left NULL a call to RtlCompareMemory
    // will be used to compare two identificaiton descriptions.
    //
    config.EvtChildListIdentificationDescriptionCompare =
        Bus_EvtChildListIdentificationDescriptionCompare;
    //
    // This function pointer will be called when the framework needs to free a
    // identification description.  An implementation of this function is only
    // necessary if the description contains dynamically allocated memory
    // (by the driver writer) that needs to be freed. The actual identification
    // description pointer itself will be freed by the framework.
    //
    config.EvtChildListIdentificationDescriptionCleanup =
        Bus_EvtChildListIdentificationDescriptionCleanup;

    //
    // Tell the framework to use the built-in childlist to track the state
    // of the device based on the configuration we just created.
    //
    WdfFdoInitSetDefaultChildListConfig(DeviceInit,
        &config,
        WDF_NO_OBJECT_ATTRIBUTES);

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
    BOOLEAN fBar0Found = FALSE;
    BOOLEAN fBar4Found = FALSE;
    NTSTATUS status;
    PFDO_CONTEXT fdoCtx;
    ULONG resourceCount;

    fdoCtx = Fdo_GetContext(Device);
    resourceCount = WdfCmResourceListGetCount(ResourcesTranslated);

    SklHdAudBusPrint(DEBUG_LEVEL_INFO, DBG_INIT,
        "%s\n", __func__);

    for (ULONG i = 0; i < resourceCount; i++)
    {
        PCM_PARTIAL_RESOURCE_DESCRIPTOR pDescriptor;
        UCHAR Class;
        UCHAR Type;

        pDescriptor = WdfCmResourceListGetDescriptor(
            ResourcesTranslated, i);

        switch (pDescriptor->Type)
        {
        case CmResourceTypeMemory:
            //Look for BAR0 and BAR4
            if (fBar0Found == FALSE) {
                SklHdAudBusPrint(DEBUG_LEVEL_INFO, DBG_INIT,
                    "Found BAR0: 0x%llx (size 0x%lx)\n", pDescriptor->u.Memory.Start.QuadPart, pDescriptor->u.Memory.Length);

                fdoCtx->m_BAR0.Base.Base = MmMapIoSpace(pDescriptor->u.Memory.Start, pDescriptor->u.Memory.Length, MmNonCached);
                fdoCtx->m_BAR0.Len = pDescriptor->u.Memory.Length;

                SklHdAudBusPrint(DEBUG_LEVEL_INFO, DBG_INIT,
                    "Mapped to %p\n", fdoCtx->m_BAR0.Base.baseptr);
                fBar0Found = TRUE;
            }
            else if (fBar4Found == FALSE) {
                SklHdAudBusPrint(DEBUG_LEVEL_INFO, DBG_INIT,
                    "Found BAR4: 0x%llx (size 0x%lx)\n", pDescriptor->u.Memory.Start.QuadPart, pDescriptor->u.Memory.Length);

                fdoCtx->m_BAR0.Base.Base = MmMapIoSpace(pDescriptor->u.Memory.Start, pDescriptor->u.Memory.Length, MmNonCached);
                fdoCtx->m_BAR0.Len = pDescriptor->u.Memory.Length;

                SklHdAudBusPrint(DEBUG_LEVEL_INFO, DBG_INIT,
                    "Mapped to %p\n", fdoCtx->m_BAR0.Base.baseptr);
                fBar4Found = TRUE;
            }
            break;
        }
    }

    if (fdoCtx->m_BAR0.Base.Base == NULL) {
        status = STATUS_NOT_FOUND; //BAR0 is required
        return status;
    }

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

    if (fdoCtx->m_BAR0.Base.Base)
        MmUnmapIoSpace(fdoCtx->m_BAR0.Base.Base, fdoCtx->m_BAR0.Len);
    if (fdoCtx->m_BAR4.Base.Base)
        MmUnmapIoSpace(fdoCtx->m_BAR4.Base.Base, fdoCtx->m_BAR4.Len);

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

    WdfChildListBeginScan(WdfFdoGetDefaultChildList(Device));

    {
        PDO_IDENTIFICATION_DESCRIPTION description;
        //
        // Initialize the description with the information about the newly
        // plugged in device.
        //
        WDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER_INIT(
            &description.Header,
            sizeof(description)
        );

        description.FuncId = 1;
        description.VenId = 0x15AD;
        description.DevId = 0x1975;
        description.SubsysId = 0x15AD1975;
        description.RevId = 0x1001;

        //
        // Call the framework to add this child to the childlist. This call
        // will internaly call our DescriptionCompare callback to check
        // whether this device is a new device or existing device. If
        // it's a new device, the framework will call DescriptionDuplicate to create
        // a copy of this description in nonpaged pool.
        // The actual creation of the child device will happen when the framework
        // receives QUERY_DEVICE_RELATION request from the PNP manager in
        // response to InvalidateDeviceRelations call made as part of adding
        // a new child.
        //
        status = WdfChildListAddOrUpdateChildDescriptionAsPresent(
            WdfFdoGetDefaultChildList(Device), &description.Header,
            NULL); // AddressDescription
    }

    WdfChildListEndScan(WdfFdoGetDefaultChildList(Device));

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