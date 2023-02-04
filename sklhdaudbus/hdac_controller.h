#if !defined(_HDA_CONTROLLER_H_)
#define _HDA_CONTROLLER_H_

//New
NTSTATUS GetHDACapabilities(PFDO_CONTEXT fdoCtx);
NTSTATUS StartHDAController(PFDO_CONTEXT fdoCtx);
NTSTATUS StopHDAController(PFDO_CONTEXT fdoCtx);

//Old
BOOLEAN hda_interrupt(WDFINTERRUPT Interrupt, ULONG MessageID);
void hda_dpc(WDFINTERRUPT Interrupt, WDFOBJECT AssociatedObject);
NTSTATUS hdac_bus_send_cmd(PFDO_CONTEXT fdoCtx, unsigned int val);
NTSTATUS hdac_bus_get_response(PFDO_CONTEXT fdoCtx, UINT16 addr, UINT32* res);

NTSTATUS hdac_bus_exec_verb(PFDO_CONTEXT fdoCtx, UINT16 addr, UINT32 cmd, UINT32* res);

#endif