#include "driver.h"

static void hda_clear_corbrp(PFDO_CONTEXT fdoCtx) {
	//Clear the CORB read pointer
	int timeout;

	//TODO: Re-enable
	/*for (timeout = 1000; timeout > 0; timeout--) {
		if (hda_read16(fdoCtx, CORBRP) & HDA_CORBRP_RST)
			break;
		udelay(1);
	}
	if (timeout <= 0) {
		SklHdAudBusPrint(DEBUG_LEVEL_ERROR, DBG_INIT,
			"CORB reset timeout#1, CORBRP = %d\n", hda_read16(fdoCtx, CORBRP));
	}*/

	hda_write16(fdoCtx, CORBRP, 0);
	for (timeout = 1000; timeout > 0; timeout--) {
		if (hda_read16(fdoCtx, CORBRP) == 0)
			break;
		udelay(1);
	}
	if (timeout <= 0) {
		SklHdAudBusPrint(DEBUG_LEVEL_ERROR, DBG_INIT,
			"CORB reset timeout#2, CORBRP = %d\n", hda_read16(fdoCtx, CORBRP));
	}
}

void hdac_bus_init_cmd_io(PFDO_CONTEXT fdoCtx) {
	//CORB Set up
	WdfInterruptAcquireLock(fdoCtx->Interrupt);
	fdoCtx->corb.buf = fdoCtx->rb;
	fdoCtx->corb.addr = MmGetPhysicalAddress(fdoCtx->corb.buf);
	hda_write32(fdoCtx, CORBLBASE, fdoCtx->corb.addr.LowPart);
	hda_write32(fdoCtx, CORBUBASE, fdoCtx->corb.addr.HighPart);

	//Set the corb size to 256 entries
	hda_write8(fdoCtx, CORBSIZE, 0x02);
	//Set the corb write pointer to 0
	hda_write16(fdoCtx, CORBWP, 0);

	//Reset the corb hw read pointer
	hda_write16(fdoCtx, CORBRP, HDA_CORBRP_RST);

	hda_clear_corbrp(fdoCtx);

	//Enable corb dma
	hda_write8(fdoCtx, CORBCTL, HDA_CORBCTL_RUN);

	fdoCtx->rirb.buf = (UINT32 *)((UINT8*)fdoCtx->rb + 0x800);
	fdoCtx->rirb.addr = MmGetPhysicalAddress(fdoCtx->rirb.buf);
	hda_write32(fdoCtx, RIRBLBASE, fdoCtx->rirb.addr.LowPart);
	hda_write32(fdoCtx, RIRBUBASE, fdoCtx->rirb.addr.HighPart);

	//Set the rirb size to 256 entries
	hda_write8(fdoCtx, RIRBSIZE, 0x02);
	//Reset the rirb hw write pointer
	hda_write16(fdoCtx, RIRBWP, HDA_RIRBWP_RST);
	//set N=1, get RIRB response interrupt for new entry
	hda_write16(fdoCtx, RINTCNT, 1);
	//enable rirb dma and response irq
	hda_write8(fdoCtx, RIRBCTL, HDA_RBCTL_DMA_EN | HDA_RBCTL_IRQ_EN);
	//Accept unsolicited responses
	hda_update32(fdoCtx, GCTL, HDA_GCTL_UNSOL, HDA_GCTL_UNSOL);
	WdfInterruptReleaseLock(fdoCtx->Interrupt);
}

static void hdac_wait_for_cmd_dmas(PFDO_CONTEXT fdoCtx) {
	LARGE_INTEGER StartTime;
	KeQuerySystemTimePrecise(&StartTime);

	int timeout_ms = 100;

	while (hda_read8(fdoCtx, RIRBCTL) & HDA_RBCTL_DMA_EN) {
		LARGE_INTEGER CurrentTime;
		KeQuerySystemTimePrecise(&CurrentTime);

		if (((CurrentTime.QuadPart - StartTime.QuadPart) / (10 * 1000)) >= timeout_ms) {
			break;
		}
		udelay(500);
	}

	KeQuerySystemTimePrecise(&StartTime);
	while (hda_read8(fdoCtx, CORBCTL) & HDA_CORBCTL_RUN) {
		LARGE_INTEGER CurrentTime;
		KeQuerySystemTimePrecise(&CurrentTime);

		if (((CurrentTime.QuadPart - StartTime.QuadPart) / (10 * 1000)) >= timeout_ms) {
			break;
		}
		udelay(500);
	}
}

void hdac_bus_stop_cmd_io(PFDO_CONTEXT fdoCtx) {
	WdfInterruptAcquireLock(fdoCtx->Interrupt);
	//disable ringbuffer DMAs
	hda_write8(fdoCtx, RIRBCTL, 0);
	hda_write8(fdoCtx, CORBCTL, 0);

	WdfInterruptReleaseLock(fdoCtx->Interrupt);

	hdac_wait_for_cmd_dmas(fdoCtx);

	WdfInterruptAcquireLock(fdoCtx->Interrupt);
	//disable unsolicited responses
	hda_update32(fdoCtx, GCTL, HDA_GCTL_UNSOL, 0);
	WdfInterruptReleaseLock(fdoCtx->Interrupt);
}

static UINT16 hda_command_addr(UINT32 cmd) {
	UINT16 addr = cmd >> 28;
	if (addr >= HDA_MAX_CODECS) {
		SklHdAudBusPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL,
			"invalid command addr\n");
		addr = 0;
	}
	return addr;
}

NTSTATUS hdac_bus_send_cmd(PFDO_CONTEXT fdoCtx, unsigned int val) {
	UINT16 addr = hda_command_addr(val);

	WdfInterruptAcquireLock(fdoCtx->Interrupt);

	fdoCtx->last_cmd[addr] = val;

	//Add command to corb
	UINT16 wp = hda_read16(fdoCtx, CORBWP);
	if (wp == 0xffff) {
		//Something wrong, controller likely went to sleep
		SklHdAudBusPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL,
			"%s: device not found\n", __func__);
		WdfInterruptReleaseLock(fdoCtx->Interrupt);
		return STATUS_DEVICE_DOES_NOT_EXIST;
	}
	wp++;
	wp %= HDA_MAX_CORB_ENTRIES;

	UINT16 rp = hda_read16(fdoCtx, CORBRP);
	if (wp == rp) {
		//Oops it's full
		SklHdAudBusPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL,
			"%s: device busy\n", __func__);
		WdfInterruptReleaseLock(fdoCtx->Interrupt);
		return STATUS_RETRY;
	}

	fdoCtx->rirb.cmds[addr]++;
	fdoCtx->corb.buf[wp] = val;

	hda_write16(fdoCtx, CORBWP, wp);

	WdfInterruptReleaseLock(fdoCtx->Interrupt);
	return STATUS_SUCCESS;
}

#define HDA_RIRB_EX_UNSOL_EV	(1<<4)

void hdac_bus_update_rirb(PFDO_CONTEXT fdoCtx) {
	UINT rp, wp;
	UINT32 res, res_ex;
	UINT16 addr;

	wp = hda_read16(fdoCtx, RIRBWP);
	if (wp == 0xffff) {
		//Something wrong, controller likely went to sleep
		SklHdAudBusPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL,
			"%s: device not found\n", __func__);
		return;
	}

	if (wp == fdoCtx->rirb.wp)
		return;
	fdoCtx->rirb.wp = wp;

	while (fdoCtx->rirb.rp != wp) {
		fdoCtx->rirb.rp++;
		fdoCtx->rirb.rp %= HDA_MAX_RIRB_ENTRIES;

		rp = fdoCtx->rirb.rp << 1; /* an RIRB entry is 8-bytes */
		res_ex = fdoCtx->rirb.buf[rp + 1];
		res = fdoCtx->rirb.buf[rp];
		addr = res_ex & 0xf;
		if (addr >= HDA_MAX_CODECS) {
			SklHdAudBusPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL,
				"spurious response % #x: % #x, rp = % d, wp = % d\n",
				res, res_ex, fdoCtx->rirb.rp, wp);
		}
		else if (res_ex & HDA_RIRB_EX_UNSOL_EV) {
			DbgPrint("Unsolicited Event received\n");
		}
		else if (fdoCtx->rirb.cmds[addr]) {
			fdoCtx->rirb.res[addr] = res;
			fdoCtx->rirb.cmds[addr]--;
			if (!fdoCtx->rirb.cmds[addr]) {
				DbgPrint("TODO: Notify for RIRB processed\n");
			}
		}
		else {
			SklHdAudBusPrint(DEBUG_LEVEL_ERROR, DBG_IOCTL,
				"spurious response %#x:%#x, last cmd=%#08x\n",
				res, res_ex, fdoCtx->last_cmd[addr])
		}
	}
}

NTSTATUS hdac_bus_get_response(PFDO_CONTEXT fdoCtx, UINT16 addr, UINT32* res) {
	LARGE_INTEGER StartTime;
	KeQuerySystemTimePrecise(&StartTime);

	int timeout_ms = 1000;
	for (ULONG loopcounter = 0; ; loopcounter++) {
		if (!fdoCtx->rirb.cmds[addr]) {
			if (res) {
				*res = fdoCtx->rirb.res[addr]; //the last value
				DbgPrint("Read Response 0x%x\n", *res);
			}
			return STATUS_SUCCESS;
		}

		LARGE_INTEGER CurrentTime;
		KeQuerySystemTimePrecise(&CurrentTime);

		if (((CurrentTime.QuadPart - StartTime.QuadPart) / (10 * 1000)) >= timeout_ms) {
			return STATUS_IO_TIMEOUT;
		}

		udelay(100);
	}
}

void hdac_bus_enter_link_reset(PFDO_CONTEXT fdoCtx) {
    //Reset controller
    hda_update32(fdoCtx, GCTL, HDA_GCTL_RESET, 0);

	LARGE_INTEGER StartTime;
	KeQuerySystemTimePrecise(&StartTime);

	int timeout_ms = 100;

	while (hda_read32(fdoCtx, GCTL) & HDA_GCTL_RESET) {
		LARGE_INTEGER CurrentTime;
		KeQuerySystemTimePrecise(&CurrentTime);

		if (((CurrentTime.QuadPart - StartTime.QuadPart) / (10 * 1000)) >= timeout_ms) {
			return;
		}
		udelay(500);
	}
}

void hdac_bus_exit_link_reset(PFDO_CONTEXT fdoCtx) {
	hda_update32(fdoCtx, GCTL, HDA_GCTL_RESET, HDA_GCTL_RESET);

	LARGE_INTEGER StartTime;
	KeQuerySystemTimePrecise(&StartTime);

	int timeout_ms = 100;

	while (!hda_read32(fdoCtx, GCTL)) {
		LARGE_INTEGER CurrentTime;
		KeQuerySystemTimePrecise(&CurrentTime);

		if (((CurrentTime.QuadPart - StartTime.QuadPart) / (10 * 1000)) >= timeout_ms) {
			return;
		}
		udelay(500);
	}
}

NTSTATUS hdac_bus_reset_link(PFDO_CONTEXT fdoCtx) {
    /* clear STATESTS if not in reset */

    if (hda_read32(fdoCtx, GCTL) & HDA_GCTL_RESET)
        hda_write32(fdoCtx, STATESTS, STATESTS_INT_MASK);

	// Reset Controller
	hdac_bus_enter_link_reset(fdoCtx);

	/* delay for >= 100us for codec PLL to settle per spec
	 * Rev 0.9 section 5.5.1
	 */
	udelay(500);

	// Bring Controller out of reset

	hdac_bus_exit_link_reset(fdoCtx);

	// Wait >= 540us for codecs to initialize

	udelay(1000);

	// Check if controller is ready
	if (!hda_read32(fdoCtx, GCTL)) {
		SklHdAudBusPrint(DEBUG_LEVEL_ERROR, DBG_INIT,
			"Controller not ready!\n");
		return STATUS_DEVICE_POWER_FAILURE;
	}

	if (!fdoCtx->codecMask) {
		fdoCtx->codecMask = hda_read16(fdoCtx, STATESTS);
		SklHdAudBusPrint(DEBUG_LEVEL_INFO, DBG_INIT,
			"codec mask = 0x%lx\n", fdoCtx->codecMask);
	}
	return STATUS_SUCCESS;
}

NTSTATUS hdac_bus_init(PFDO_CONTEXT fdoCtx) {
	NTSTATUS status;
	
	status = hdac_bus_reset_link(fdoCtx);
	if (!NT_SUCCESS(status)) {
		return status;
	}

	{
		//Clear Interrupts

		//Clear stream status
		for (int i = 0; i < fdoCtx->numStreams; i++) {
			stream_write8(&fdoCtx->streams[i], SD_STS, SD_INT_MASK);
		}

		//Clear STATESTS
		hda_write16(fdoCtx, STATESTS, STATESTS_INT_MASK);

		//Clear rirb status
		hda_write16(fdoCtx, RIRBSTS, RIRB_INT_MASK);

		//Clear int status
		hda_write32(fdoCtx, INTSTS, HDA_INT_CTRL_EN | HDA_INT_ALL_STREAM);
	}

	hdac_bus_init_cmd_io(fdoCtx);

	//Enable interrupts
	hda_update32(fdoCtx, INTCTL, HDA_INT_CTRL_EN | HDA_INT_GLOBAL_EN,
		HDA_INT_CTRL_EN | HDA_INT_GLOBAL_EN);

	//Program the position buffer
	PHYSICAL_ADDRESS posbufAddr = MmGetPhysicalAddress(fdoCtx->posbuf);
	hda_write32(fdoCtx, DPLBASE, posbufAddr.LowPart);
	hda_write32(fdoCtx, DPUBASE, posbufAddr.HighPart);

	return STATUS_SUCCESS;
}

BOOLEAN hda_interrupt(
	WDFINTERRUPT Interrupt,
	ULONG MessageID) {
	UNREFERENCED_PARAMETER(MessageID);

	WDFDEVICE Device = WdfInterruptGetDevice(Interrupt);
	PFDO_CONTEXT fdoCtx = Fdo_GetContext(Device);

	BOOLEAN active, handled = FALSE;
	int repeat = 0; //Avoid endless loop
	do {
		UINT32 status = hda_read32(fdoCtx, INTSTS);
		if (status == 0 || status == 0xffffffff)
			break;

		handled = TRUE;
		active = FALSE;

		//handle stream IRQ

		status = hda_read16(fdoCtx, RIRBSTS);
		if (status & RIRB_INT_MASK) {
			/*
			 * Clearing the interrupt status here ensures that no
			 * interrupt gets masked after the RIRB wp is read in
			 * snd_hdac_bus_update_rirb. This avoids a possible
			 * race condition where codec response in RIRB may
			 * remain unserviced by IRQ, eventually falling back
			 * to polling mode in azx_rirb_get_response.
			 */
			hda_write16(fdoCtx, RIRBSTS, RIRB_INT_MASK);
			active = TRUE;
			if (status & RIRB_INT_RESPONSE) {
				DbgPrint("Got RIRB\n");
				hdac_bus_update_rirb(fdoCtx);
			}
		}
	} while (active && ++repeat < 10);

	return handled;
}

NTSTATUS hdac_bus_exec_verb_unlocked(PFDO_CONTEXT fdoCtx, UINT16 addr, UINT32 cmd, UINT32* res) {
	NTSTATUS status;
	UINT32 tmp;

	for (;;) {
		status = hdac_bus_send_cmd(fdoCtx, cmd);
		if (status != STATUS_RETRY)
			break;

		//Process pending verbs
		status = hdac_bus_get_response(fdoCtx, addr, &tmp);
		if (!NT_SUCCESS(status))
			break;
	}
	if (NT_SUCCESS(status)) {
		status = hdac_bus_get_response(fdoCtx, addr, res);
	}
	return status;
}

NTSTATUS hdac_bus_exec_verb(PFDO_CONTEXT fdoCtx, UINT16 addr, UINT32 cmd, UINT32* res) {
	WdfWaitLockAcquire(fdoCtx->cmdLock, 0);

	NTSTATUS status = hdac_bus_exec_verb_unlocked(fdoCtx, addr, cmd, res);

	WdfWaitLockRelease(fdoCtx->cmdLock);
	return status;
}