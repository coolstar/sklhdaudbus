#include "driver.h"

NTSTATUS HDAGraphics_EvtIoTargetQueryRemove(
    WDFIOTARGET ioTarget
) {
    DbgPrint("Device Removal Notification\n");

    WdfIoTargetCloseForQueryRemove(ioTarget);
    return STATUS_SUCCESS;
}

void HDAGraphics_EvtIoTargetRemoveCanceled(
    WDFIOTARGET ioTarget
) {
    WDF_IO_TARGET_OPEN_PARAMS openParams;
    NTSTATUS status;
    DbgPrint("Device Removal Cancel\n");

    WDF_IO_TARGET_OPEN_PARAMS_INIT_REOPEN(&openParams);

    status = WdfIoTargetOpen(ioTarget, &openParams);

    if (!NT_SUCCESS(status)) {
        KdPrint(("WdfIoTargetOpen failed 0x%x\n", status));
        WdfObjectDelete(ioTarget);
        return;
    }
}

void HDAGraphics_EvtIoTargetRemoveComplete(
    WDFIOTARGET ioTarget
) {
    PFDO_CONTEXT fdoCtx;
    DbgPrint("Device Removal Complete\n");

    PGRAPHICSIOTARGET_CONTEXT ioTargetContext;
    ioTargetContext = GraphicsIoTarget_GetContext(ioTarget);

    fdoCtx = ioTargetContext->FdoContext;

    WdfWaitLockAcquire(fdoCtx->GraphicsDevicesCollectionWaitLock, NULL);
    WdfCollectionRemove(fdoCtx->GraphicsDevicesCollection, ioTarget);
    WdfWaitLockRelease(fdoCtx->GraphicsDevicesCollectionWaitLock);

    WdfObjectDelete(ioTarget);
}

void HDAGraphicsPowerNotificationCallback(
    PVOID GraphicsDeviceHandle,
    DEVICE_POWER_STATE NewGrfxPowerState,
    BOOLEAN PreNotification,
    PVOID PrivateHandle
) {
    PFDO_CONTEXT fdoCtx = (PFDO_CONTEXT)PrivateHandle;
    UNREFERENCED_PARAMETER(GraphicsDeviceHandle);
    UNREFERENCED_PARAMETER(NewGrfxPowerState);
    UNREFERENCED_PARAMETER(PreNotification);
    UNREFERENCED_PARAMETER(fdoCtx);
    //No-Op
}

void HDAGraphicsPowerRemovalNotificationCallback(
    PVOID GraphicsDeviceHandle,
    PVOID PrivateHandle
) {
    PFDO_CONTEXT fdoCtx = (PFDO_CONTEXT)PrivateHandle;
    UNREFERENCED_PARAMETER(GraphicsDeviceHandle);
    UNREFERENCED_PARAMETER(fdoCtx);
    //No-Op
}

void HDAGraphicsPowerFStateNotificationCallback(
    PVOID GraphicsDeviceHandle,
    ULONG ComponentIndex,
    UINT NewFState,
    BOOLEAN PreNotification,
    PVOID PrivateHandle
) {
    PFDO_CONTEXT fdoCtx = (PFDO_CONTEXT)PrivateHandle;
    DbgPrint("%s 0x%x\n", __func__, ComponentIndex);

    if (NewFState) {
        DbgPrint("Fx State\n");
        if (PreNotification) {

        }
        else {

        }
    }
    else {
        DbgPrint("F0 State\n");
        if (PreNotification) {

        }
        else {

        }
    }
}

void HDAGraphicsPowerInitialComponentStateCallback( //https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmthk/nc-d3dkmthk-pdxgk_initial_component_state
    PVOID GraphicsDeviceHandle,
    PVOID PrivateHandle,
    ULONG ComponentIndex,
    BOOLEAN IsBlockingType,
    UINT InitialFState,
    GUID ComponentGuid,
    UINT PowerComponentMappingFlag
) {
    PFDO_CONTEXT fdoCtx = (PFDO_CONTEXT)PrivateHandle;
    DbgPrint("%s 0x%x\n", __func__, ComponentIndex);
    if (PowerComponentMappingFlag) {
    }
    else {
        if (InitialFState) {
            DbgPrint("Fx State\n");
        } else {
            DbgPrint("F0 State\n");
        }
    }
}

void HDAGraphicsPowerInterfaceAdd(WDFWORKITEM WorkItem) {
    PGRAPHICSWORKITEM_CONTEXT workItemContext = GraphicsWorkitem_GetContext(WorkItem);
    PFDO_CONTEXT fdoCtx = workItemContext->FdoContext;
    PUNICODE_STRING graphicsDeviceSymlink = &workItemContext->GPUDeviceSymlink;

    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, GRAPHICSIOTARGET_CONTEXT);

    WDFIOTARGET ioTarget;
    WDF_IO_TARGET_OPEN_PARAMS openParams;
    PGRAPHICSIOTARGET_CONTEXT ioTargetContext;

    NTSTATUS status = WdfIoTargetCreate(fdoCtx->WdfDevice, &attributes, &ioTarget);
    if (!NT_SUCCESS(status)) {
        DbgPrint("WdfIoTargetCreate failed 0x%x\n", status);
        goto exit;
    }

    ioTargetContext = GraphicsIoTarget_GetContext(ioTarget);
    ioTargetContext->FdoContext = fdoCtx;

    WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(
        &openParams,
        graphicsDeviceSymlink,
        STANDARD_RIGHTS_ALL);
    openParams.ShareAccess = FILE_SHARE_WRITE | FILE_SHARE_READ;
    openParams.EvtIoTargetQueryRemove = HDAGraphics_EvtIoTargetQueryRemove;
    openParams.EvtIoTargetRemoveCanceled = HDAGraphics_EvtIoTargetRemoveCanceled;
    openParams.EvtIoTargetRemoveComplete = HDAGraphics_EvtIoTargetRemoveComplete;

    status = WdfIoTargetOpen(ioTarget, &openParams);

    if (!NT_SUCCESS(status)) {
        DbgPrint("WdfIoTargetOpen failed with status 0x%x\n", status);
        WdfObjectDelete(ioTarget);
        goto exit;
    }

    WdfWaitLockAcquire(fdoCtx->GraphicsDevicesCollectionWaitLock, NULL);
    WdfCollectionAdd(fdoCtx->GraphicsDevicesCollection, ioTarget);
    WdfWaitLockRelease(fdoCtx->GraphicsDevicesCollectionWaitLock);

    DXGK_GRAPHICSPOWER_REGISTER_INPUT graphicsPowerRegisterInput = { 0 };
    graphicsPowerRegisterInput.Version = DXGK_GRAPHICSPOWER_VERSION;
    graphicsPowerRegisterInput.PrivateHandle = fdoCtx;
    graphicsPowerRegisterInput.PowerNotificationCb = HDAGraphicsPowerNotificationCallback;
    graphicsPowerRegisterInput.RemovalNotificationCb = HDAGraphicsPowerRemovalNotificationCallback;
    graphicsPowerRegisterInput.FStateNotificationCb = HDAGraphicsPowerFStateNotificationCallback;
    graphicsPowerRegisterInput.InitialComponentStateCb = HDAGraphicsPowerInitialComponentStateCallback;

    WDF_MEMORY_DESCRIPTOR inputDescriptor;
    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&inputDescriptor, &graphicsPowerRegisterInput, sizeof(graphicsPowerRegisterInput));

    WDF_MEMORY_DESCRIPTOR outputDescriptor;
    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&outputDescriptor, &ioTargetContext->graphicsPowerRegisterOutput, sizeof(ioTargetContext->graphicsPowerRegisterOutput));

    status = WdfIoTargetSendInternalIoctlSynchronously(ioTarget,
        WDF_NO_HANDLE,
        IOCTL_INTERNAL_GRAPHICSPOWER_REGISTER,
        &inputDescriptor, &outputDescriptor, NULL, NULL);
    if (!NT_SUCCESS(status)) {
        DbgPrint("IOCTL_INTERNAL_GRAPHICSPOWER_REGISTER failed with status 0x%x\n", status);
        goto exit;
    }

exit:
    WdfObjectDelete(WorkItem);
}

NTSTATUS
HDAGraphicsPowerInterfaceCallback(
    PVOID NotificationStruct,
    PVOID Context
) {
    NTSTATUS status = STATUS_SUCCESS;
    PFDO_CONTEXT fdoCtx = (PFDO_CONTEXT)Context;
    PDEVICE_INTERFACE_CHANGE_NOTIFICATION devNotificationStruct = (PDEVICE_INTERFACE_CHANGE_NOTIFICATION)NotificationStruct;

    if (!IsEqualGUID(devNotificationStruct->InterfaceClassGuid, GUID_DEVINTERFACE_GRAPHICSPOWER)) {
        return STATUS_NOT_SUPPORTED;
    }

    if (IsEqualGUID(devNotificationStruct->Event, GUID_DEVICE_INTERFACE_ARRIVAL)) {
        DbgPrint("Graphics Arrival Notification!\n");

        status = RtlUnicodeStringValidate(devNotificationStruct->SymbolicLinkName);
        if (!NT_SUCCESS(status)) {
            return status;
        }

        WDF_WORKITEM_CONFIG workItemConfig;
        WDF_WORKITEM_CONFIG_INIT(&workItemConfig, HDAGraphicsPowerInterfaceAdd);

        WDF_OBJECT_ATTRIBUTES  attributes;
        WDFWORKITEM workItem;

        WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
        WDF_OBJECT_ATTRIBUTES_SET_CONTEXT_TYPE(
            &attributes,
            GRAPHICSWORKITEM_CONTEXT
        );
        attributes.ParentObject = fdoCtx->WdfDevice;

        WdfWorkItemCreate(&workItemConfig, &attributes, &workItem);

        PGRAPHICSWORKITEM_CONTEXT workItemContext = GraphicsWorkitem_GetContext(workItem);
        workItemContext->FdoContext = fdoCtx;
        workItemContext->GPUDeviceSymlink = *devNotificationStruct->SymbolicLinkName;

        WdfWorkItemEnqueue(workItem);
    }

    return status;
}

void CheckHDAGraphicsRegistryKeys(PFDO_CONTEXT fdoCtx) {
    NTSTATUS status;
    WDFKEY driverKey;
    status = WdfDeviceOpenRegistryKey(fdoCtx->WdfDevice, PLUGPLAY_REGKEY_DRIVER, READ_CONTROL, NULL, &driverKey);
    if (!NT_SUCCESS(status)) {
        return;
    }

    WDFKEY settingsKey;
    DECLARE_CONST_UNICODE_STRING(DriverSettings, L"Settings");
    DECLARE_CONST_UNICODE_STRING(GfxSharedCodecAddress, L"GfxSharedCodecAddress");
    status = WdfRegistryOpenKey(driverKey, &DriverSettings, READ_CONTROL, NULL, &settingsKey);
    if (!NT_SUCCESS(status)) {
        goto closeDriverKey;
    }

    ULONG GfxCodecAddr;
    status = WdfRegistryQueryULong(settingsKey, &GfxSharedCodecAddress, &GfxCodecAddr);
    if (NT_SUCCESS(status)) {
        fdoCtx->UseSGPCCodec = TRUE;
        fdoCtx->GraphicsCodecAddress = GfxCodecAddr;
    }

    WdfRegistryClose(settingsKey);

closeDriverKey:
    WdfRegistryClose(driverKey);
}