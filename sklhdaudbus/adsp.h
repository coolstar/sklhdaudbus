//
// The ADSP_BUS_INTERFACE interface GUID
//
// {752A2CAE-3455-4D18-A184-8B34B22632CD}
DEFINE_GUID(GUID_ADSP_BUS_INTERFACE,
    0x752a2cae, 0x3455, 0x4d18, 0xa1, 0x84, 0x8b, 0x34, 0xb2, 0x26, 0x32, 0xcd);

typedef _Must_inspect_result_ NTSTATUS(*PGET_ADSP_RESOURCES) (_In_ PVOID _context, _Out_ _PCI_BAR* hdaBar, _Out_ _PCI_BAR* adspBar, _Out_ BUS_INTERFACE_STANDARD* pciConfig);

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
    // Then we expand the structure with the HDAUDIO_BUS_INTERFACE_PING_PONG stuff.
    // Many functions are identical (and derived) from the HDAUDIO_BUS_INTERFACE
    // interface. 

    PGET_ADSP_RESOURCES           GetResources;
} ADSP_BUS_INTERFACE, * PADSP_BUS_INTERFACE;

ADSP_BUS_INTERFACE ADSP_BusInterface(PVOID Context);