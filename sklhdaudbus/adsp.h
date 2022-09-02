#ifndef __ADSP_INTERFACE
#define __ADSP_INTERFACE

//
// The ADSP_BUS_INTERFACE interface GUID
//
// {752A2CAE-3455-4D18-A184-8B34B22632CD}
DEFINE_GUID(GUID_ADSP_BUS_INTERFACE,
    0x752a2cae, 0x3455, 0x4d18, 0xa1, 0x84, 0x8b, 0x34, 0xb2, 0x26, 0x32, 0xcd);

typedef _Must_inspect_result_ NTSTATUS(*PGET_ADSP_RESOURCES) (_In_ PVOID _context, _Out_ _PCI_BAR* hdaBar, _Out_ _PCI_BAR* adspBar, PVOID *ppcap, _Out_ BUS_INTERFACE_STANDARD* pciConfig);
typedef _Must_inspect_result_ BOOL(*PADSP_INTERRUPT_CALLBACK)(PVOID context);
typedef _Must_inspect_result_ NTSTATUS(*PREGISTER_ADSP_INTERRUPT) (_In_ PVOID _context, _In_ PADSP_INTERRUPT_CALLBACK callback, _In_ PVOID callbackContext);
typedef _Must_inspect_result_ NTSTATUS(*PUNREGISTER_ADSP_INTERRUPT) (_In_ PVOID _context);

typedef struct _ADSP_BUS_INTERFACE
{
    //
    // First we define the standard INTERFACE structure ...
    //
    USHORT                    Size;
    USHORT                    Version;
    PVOID                     Context;
    PINTERFACE_REFERENCE      InterfaceReference;
    PINTERFACE_DEREFERENCE    InterfaceDereference;

    //
    // Then we expand the structure with the ADSP_BUS_INTERFACE stuff.

    UINT16 CtlrDevId;
    PGET_ADSP_RESOURCES           GetResources;
    PREGISTER_ADSP_INTERRUPT      RegisterInterrupt;
    PUNREGISTER_ADSP_INTERRUPT    UnregisterInterrupt;
} ADSP_BUS_INTERFACE, * PADSP_BUS_INTERFACE;

#ifndef ADSP_DECL
ADSP_BUS_INTERFACE ADSP_BusInterface(PVOID Context);
#endif
#endif