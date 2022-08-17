#if !defined(_SKLHDAUDBUS_BUSPDO_H_)
#define _SKLHDAUDBUS_BUSPDO_H_

#define MAX_INSTANCE_ID_LEN 80

typedef struct _PDO_IDENTIFICATION_DESCRIPTION
{
    WDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER Header; // should contain this header

    UINT8 Idx;

    UINT8 FuncId;
    UINT16 VenId;
    UINT16 DevId;
    UINT32 SubsysId;
    UINT16 RevId;

} PDO_IDENTIFICATION_DESCRIPTION, * PPDO_IDENTIFICATION_DESCRIPTION;

//
// This is PDO device-extension.
//
typedef struct _PDO_DEVICE_DATA
{
    UINT8 Idx;

    UINT8 FuncId;
    UINT16 VenId;
    UINT16 DevId;
    UINT32 SubsysId;
    UINT16 RevId;

} PDO_DEVICE_DATA, * PPDO_DEVICE_DATA;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(PDO_DEVICE_DATA, PdoGetData)

NTSTATUS
Bus_EvtChildListIdentificationDescriptionDuplicate(
    WDFCHILDLIST DeviceList,
    PWDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER SourceIdentificationDescription,
    PWDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER DestinationIdentificationDescription
);

BOOLEAN
Bus_EvtChildListIdentificationDescriptionCompare(
    WDFCHILDLIST DeviceList,
    PWDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER FirstIdentificationDescription,
    PWDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER SecondIdentificationDescription
);

VOID
Bus_EvtChildListIdentificationDescriptionCleanup(
    _In_ WDFCHILDLIST DeviceList,
    _Out_ PWDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER IdentificationDescription
);

NTSTATUS
Bus_EvtDeviceListCreatePdo(
    WDFCHILDLIST DeviceList,
    PWDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER IdentificationDescription,
    PWDFDEVICE_INIT ChildInit
);

#endif
