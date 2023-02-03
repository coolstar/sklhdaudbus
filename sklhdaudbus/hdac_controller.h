#if !defined(_HDA_CONTROLLER_H_)
#define _HDA_CONTROLLER_H_

NTSTATUS hdac_bus_init(PFDO_CONTEXT fdoCtx);
void hdac_bus_stop(PFDO_CONTEXT fdoCtx);
BOOLEAN hda_interrupt(WDFINTERRUPT Interrupt, ULONG MessageID);
NTSTATUS hdac_bus_send_cmd(PFDO_CONTEXT fdoCtx, unsigned int val);
NTSTATUS hdac_bus_get_response(PFDO_CONTEXT fdoCtx, UINT16 addr, UINT32* res);

NTSTATUS hdac_bus_exec_verb(PFDO_CONTEXT fdoCtx, UINT16 addr, UINT32 cmd, UINT32* res);

#endif