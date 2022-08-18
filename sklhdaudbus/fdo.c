#include "driver.h"

EVT_WDF_DEVICE_PREPARE_HARDWARE Fdo_EvtDevicePrepareHardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE Fdo_EvtDeviceReleaseHardware;
EVT_WDF_DEVICE_D0_ENTRY Fdo_EvtDeviceD0Entry;
EVT_WDF_DEVICE_D0_EXIT Fdo_EvtDeviceD0Exit;
EVT_WDF_DEVICE_SELF_MANAGED_IO_INIT Fdo_EvtDeviceSelfManagedIoInit;
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
    pnpPowerCallbacks.EvtDeviceSelfManagedIoInit = Fdo_EvtDeviceSelfManagedIoInit;
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
    WDF_INTERRUPT_CONFIG interruptConfig;

    device = FdoCtx->WdfDevice;

    SklHdAudBusPrint(DEBUG_LEVEL_INFO, DBG_INIT,
        "%s\n", __func__);

    //
    // Create an interrupt object for hardware notifications
    //
    WDF_INTERRUPT_CONFIG_INIT(
        &interruptConfig,
        hda_interrupt,
        NULL);

    status = WdfInterruptCreate(
        device,
        &interruptConfig,
        WDF_NO_OBJECT_ATTRIBUTES,
        &FdoCtx->Interrupt);

    if (!NT_SUCCESS(status))
    {
        SklHdAudBusPrint(DEBUG_LEVEL_ERROR, DBG_PNP,
            "Error creating WDF interrupt object - %!STATUS!",
            status);

        return status;
    }

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

    status = WdfFdoQueryForInterface(Device, &GUID_BUS_INTERFACE_STANDARD, (PINTERFACE)&fdoCtx->BusInterface, sizeof(BUS_INTERFACE_STANDARD), 1, NULL);
    if (!NT_SUCCESS(status)) {
        return status;
    }

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

    fdoCtx->BusInterface.GetBusData(fdoCtx->BusInterface.Context, PCI_WHICHSPACE_CONFIG, &fdoCtx->venId, 0, sizeof(UINT16));
    fdoCtx->BusInterface.GetBusData(fdoCtx->BusInterface.Context, PCI_WHICHSPACE_CONFIG, &fdoCtx->devId, 2, sizeof(UINT16));

    UINT32 gcap = hda_read32(fdoCtx, GCAP);
    SklHdAudBusPrint(DEBUG_LEVEL_INFO, DBG_INIT,
        "chipset global capabilities = 0x%x\n", gcap);

    fdoCtx->captureStreams = (gcap >> 8) & 0x0f;
    fdoCtx->playbackStreams = (gcap >> 12) & 0x0f;

    SklHdAudBusPrint(DEBUG_LEVEL_INFO, DBG_INIT,
        "streams (cap %d, playback %d)\n", fdoCtx->captureStreams, fdoCtx->playbackStreams);

    fdoCtx->captureIndexOff = 0;
    fdoCtx->playbackIndexOff = fdoCtx->captureStreams;
    fdoCtx->numStreams = fdoCtx->captureStreams + fdoCtx->playbackStreams;

    fdoCtx->streams = ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(HDAC_STREAM) * fdoCtx->numStreams, SKLHDAUDBUS_POOL_TAG);
    if (!fdoCtx->streams) {
        return STATUS_NO_MEMORY;
    }
    RtlZeroMemory(fdoCtx->streams, sizeof(HDAC_STREAM) * fdoCtx->numStreams);

    //Init Streams
    {
        UINT32 i;
        int streamTags[2] = { 0, 0 };

        for (i = 0; i < fdoCtx->numStreams; i++) {
            int dir = (i >= fdoCtx->captureIndexOff &&
                i < fdoCtx->captureIndexOff + fdoCtx->captureStreams);
            /* stream tag must be unique throughout
             * the stream direction group,
             * valid values 1...15
             * use separate stream tag
             */
            int tag = ++streamTags[dir];

            {
                PHDAC_STREAM stream = &fdoCtx->streams[i];
                stream->FdoContext = fdoCtx;
                /* offset: SDI0=0x80, SDI1=0xa0, ... SDO3=0x160 */
                stream->sdAddr = fdoCtx->m_BAR0.Base.baseptr + (0x20 * i + 0x80);
                /* int mask: SDI0=0x01, SDI1=0x02, ... SDO3=0x80 */
                stream->int_sta_mask = 1 << i;
                stream->idx = i;
                stream->direction = dir;
                stream->streamTag = tag;
            }

            SklHdAudBusPrint(DEBUG_LEVEL_INFO, DBG_INIT,
                "Stream tag (idx %d): %d\n", i, tag);
        }
    }

    PHYSICAL_ADDRESS maxAddr;
    maxAddr.QuadPart = MAXULONG64;

    fdoCtx->posbuf = MmAllocateContiguousMemory(PAGE_SIZE, maxAddr);
    RtlZeroMemory(fdoCtx->posbuf, PAGE_SIZE);
    if (!fdoCtx->posbuf) {
        return STATUS_NO_MEMORY;
    }

    fdoCtx->rb = MmAllocateContiguousMemory(PAGE_SIZE, maxAddr);
    RtlZeroMemory(fdoCtx->rb, PAGE_SIZE);
    
    if (!fdoCtx->rb) {
        return STATUS_NO_MEMORY;
    }

    WdfWaitLockCreate(WDF_NO_OBJECT_ATTRIBUTES, &fdoCtx->cmdLock);

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

    if (fdoCtx->posbuf)
        MmFreeContiguousMemory(fdoCtx->posbuf);
    if (fdoCtx->rb)
        MmFreeContiguousMemory(fdoCtx->rb);

    if (fdoCtx->streams)
        ExFreePoolWithTag(fdoCtx->streams, SKLHDAUDBUS_POOL_TAG);

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

    SklHdAudBusPrint(DEBUG_LEVEL_INFO, DBG_INIT,
        "Clearing TCSEL\n");
    update_pci_byte(&fdoCtx->BusInterface, HDA_PCIREG_TCSEL, 0x07, 0);

    if (fdoCtx->venId == VEN_INTEL) {
        UINT32 val;
        pci_read_cfg_dword(&fdoCtx->BusInterface, INTEL_HDA_CGCTL, &val);
        val = val & ~INTEL_HDA_CGCTL_MISCBDCGE;
        pci_write_cfg_dword(&fdoCtx->BusInterface, INTEL_HDA_CGCTL, val);
    }

    hdac_bus_init(fdoCtx);

    if (fdoCtx->venId == VEN_INTEL) {
        UINT32 val;
        pci_read_cfg_dword(&fdoCtx->BusInterface, INTEL_HDA_CGCTL, &val);
        val = val & ~INTEL_HDA_CGCTL_MISCBDCGE;
        pci_write_cfg_dword(&fdoCtx->BusInterface, INTEL_HDA_CGCTL, val);
    }

    //Reduce dma latency to avoid noise
    if (IS_BXT(fdoCtx->venId, fdoCtx->devId)) {
        /*
         * In BXT-P A0, HD-Audio DMA requests is later than expected,
         * and makes an audio stream sensitive to system latencies when
         * 24/32 bits are playing.
         * Adjusting threshold of DMA fifo to force the DMA request
         * sooner to improve latency tolerance at the expense of power.
         */
        UINT32 val = hda_read32(fdoCtx, VS_EM4L);
        val &= (0x3 << 20);
        hda_write32(fdoCtx, VS_EM4L, val);
    }
    //TODO: mlcap & lctl (hda_intel_init_chip)

    SklHdAudBusPrint(DEBUG_LEVEL_INFO, DBG_INIT,
        "hda bus initialized\n");
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

NTSTATUS
Fdo_EvtDeviceSelfManagedIoInit(
    _In_ WDFDEVICE Device
)
{
    NTSTATUS status;
    PFDO_CONTEXT fdoCtx;

    fdoCtx = Fdo_GetContext(Device);

    WdfChildListBeginScan(WdfFdoGetDefaultChildList(Device));

    for (int addr = 0; addr < HDA_MAX_CODECS; addr++) {
        if (((fdoCtx->codecMask >> addr) & 0x1) == 0)
            continue;

        DbgPrint("Scan index %d\n", addr);

        UINT32 cmdTmpl = (addr << 28) | (AC_NODE_ROOT << 20) |
            (AC_VERB_PARAMETERS << 8);
        UINT32 funcType = 0, vendorDevice, subsysId, revId, nodeCount;
        if (!NT_SUCCESS(hdac_bus_exec_verb(fdoCtx, addr, cmdTmpl | AC_PAR_VENDOR_ID, &vendorDevice))) {
            continue;
        }
        if (!NT_SUCCESS(hdac_bus_exec_verb(fdoCtx, addr, cmdTmpl | AC_PAR_SUBSYSTEM_ID, &subsysId))) {
            continue;
        }
        if (!NT_SUCCESS(hdac_bus_exec_verb(fdoCtx, addr, cmdTmpl | AC_PAR_REV_ID, &revId))) {
            continue;
        }
        if (!NT_SUCCESS(hdac_bus_exec_verb(fdoCtx, addr, cmdTmpl | AC_PAR_NODE_COUNT, &nodeCount))) {
            continue;
        }

        UINT16 startID = (nodeCount >> 16) & 0x7FFF;
        nodeCount = (nodeCount & 0x7FFF);

        UINT16 mainFuncGrp = 0;
        {
            UINT16 nid = startID;
            for (int i = 0; i < nodeCount; i++, nid++) {
                UINT32 cmd = (addr << 28) | (nid << 20) |
                    (AC_VERB_PARAMETERS << 8) | AC_PAR_FUNCTION_TYPE;
                if (!NT_SUCCESS(hdac_bus_exec_verb(fdoCtx, addr, cmd, &funcType))) {
                    continue;
                }
                switch (funcType & 0xFF) {
                case AC_GRP_AUDIO_FUNCTION:
                case AC_GRP_MODEM_FUNCTION:
                    mainFuncGrp = nid;
                    break;
                }
            }
        }

        if (subsysId == -1 || subsysId == 0) {
            UINT32 cmd = (addr << 28) | (mainFuncGrp << 20) |
                (AC_VERB_GET_SUBSYSTEM_ID << 8);
            DbgPrint("Try getting subsystem ID with other method\n");
            hdac_bus_exec_verb(fdoCtx, addr, cmd, &subsysId);
        }

        DbgPrint("Func 0x%x, vendor: 0x%x, subsys: 0x%x, rev: 0x%x\n", funcType, vendorDevice, subsysId, revId);

        PDO_IDENTIFICATION_DESCRIPTION description;
        //
        // Initialize the description with the information about the detected codec.
        //
        WDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER_INIT(
            &description.Header,
            sizeof(description)
        );

        description.CodecIds.CtlrDevId = fdoCtx->devId;
        description.CodecIds.CtlrVenId = fdoCtx->venId;

        description.CodecIds.Idx = addr;
        description.CodecIds.FuncId = funcType & 0xFF;
        description.CodecIds.VenId = (vendorDevice >> 16) & 0xFFFF;
        description.CodecIds.DevId = vendorDevice & 0xFFFF;
        description.CodecIds.SubsysId = subsysId;
        description.CodecIds.RevId = (revId >> 8) & 0xFFFF;

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

    SklHdAudBusPrint(DEBUG_LEVEL_INFO, DBG_INIT,
        "hda scan complete\n");
    return status;
}