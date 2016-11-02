/**
 *********************************************************************
 *********************************************************************
 */
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "sglib.h"
#include "bus.h"
#include "sgstring.h"
#include "uart_at89c51.h"
#include "cpu_mcs51.h"
#include "serial.h"
#include "signode.h"

#define REG_SCON	(0x98)
#define		SCON_RI		(1 << 0)
#define		SCON_TI		(1 << 1)
#define 	SCON_RB8	(1 << 2)
#define		SCON_TB8	(1 << 3)
#define		SCON_REN	(1 << 4)
#define		SCON_SM2	(1 << 5)
#define		SCON_SM1	(1 << 6)
#define		SCON_FESM0	(1 << 7)

#define	REG_SADEN	(0xb9)
#define REG_SADDR	(0xa9)
#define	REG_SBUF	(0x99)
#define	REG_PCON	(0x87)
#define 	PCON_IDL	(1 << 0)
#define 	PCON_PD		(1 << 1)
#define 	PCON_GF0	(1 << 2)
#define		PCON_GF1	(1 << 3)
#define		PCON_POF	(1 << 4)
#define		PCON_SMOD0	(1 << 6)
#define		PCON_SMOD1	(1 << 7)

typedef struct AT89C51_Uart {
	bool nineBit;
	uint8_t regSCON;
	uint8_t SM;
	uint8_t regSADEN;
	uint16_t regTXBUF;
	uint16_t regRXBUF;
	uint8_t regPCON;
	UartPort *backend;
	SigNode *sigIrq;
} AT89C51Uart;

static void
update_interrupt(AT89C51Uart * ua)
{
	if (ua->regSCON & (SCON_RI | SCON_TI)) {
		SigNode_Set(ua->sigIrq, SIG_LOW);
	} else {
		SigNode_Set(ua->sigIrq, SIG_OPEN);
	}
}

static void
serial_input(void *eventData, UartChar c)
{
	AT89C51Uart *ua = eventData;
#if 0
	static CycleCounter_t last;
	fprintf(stderr,"Input 0x%x at %ld\n",c,CyclesToMicroseconds(CycleCounter_Get() - last));
	last = CycleCounter_Get();
#endif
	ua->regRXBUF = c;
	if (c & 0x100) {
		ua->regSCON = ua->regSCON | SCON_RB8;
	} else {
		ua->regSCON = ua->regSCON & ~SCON_RB8;
	}
	if (ua->regSCON & SCON_RI) {
		/* Is there any loss detection ? */
	}
	ua->regSCON |= SCON_RI;
	update_interrupt(ua);
}

static bool
serial_output(void *eventData, UartChar * c)
{
	AT89C51Uart *ua = eventData;
	*c = ua->regTXBUF;
	if (ua->nineBit) {
		if (ua->regSCON & SCON_TB8) {
			*c |= 0x100;
		}
	}
	SerialDevice_StopTx(ua->backend);
	ua->regSCON |= SCON_TI;
	update_interrupt(ua);
	return true;
}

/**
 ***********************************************************
 * Register SCON
 *	Bit 0: RI    Receive Interrupt (Is this setable ?)
 *	Bit 1: TI	 Transmit Interrupt (Is this setable ?)
 *	Bit 2: RB8   Bit 8 of receive Buffer (9 Bit mode)	
 *	Bit 3: TB8	 Bit 8 of transmit Buffer (9 Bit mode)
 *	Bit 4: REN   Reception enable 
 * 	Bit 5: SM2   Serial Port mode
 * 	Bit 6: SM1
 *	Bit 7: FESM0 Framing error
 ***********************************************************
 */
static uint8_t
scon_read(void *eventData, uint8_t addr)
{
	AT89C51Uart *ua = eventData;
	return ua->regSCON;
}

static void
scon_write(void *eventData, uint8_t addr, uint8_t value)
{
	AT89C51Uart *ua = eventData;
	UartCmd cmd;
	uint8_t diff;
	uint8_t bits;
	uint8_t msk = SCON_RI | SCON_TI;
	uint8_t clrmsk = ~((ua->regSCON & msk) ^ msk);
	value = value & clrmsk;
	diff = ua->regSCON ^ value;
	ua->regSCON = value;
	//fprintf(stderr,"now %02x\n",ua->regSCON);
	update_interrupt(ua);
	if ((diff & (SCON_FESM0 | SCON_SM1)) || (ua->SM == 0xff)) {
		if (ua->regPCON & PCON_SMOD0) {
			ua->SM = (ua->regSCON & SCON_FESM0) | (value & SCON_SM1);
		} else {
			ua->SM = value & (SCON_FESM0 | SCON_SM1);
		}
		switch (ua->SM) {
		    default:
		    case 0:
			    bits = 8;
				ua->nineBit = false;
			    break;
		    case SCON_SM1:
			    bits = 8;
				ua->nineBit = false;
			    break;
		    case SCON_FESM0:
			    bits = 9;
				ua->nineBit = true;
			    break;
		    case SCON_FESM0 | SCON_SM1:
			    bits = 9;
				ua->nineBit = true;
			    break;

		}
		//fprintf(stderr,"Bits :%u\n",bits);
		//sleep(1);
		cmd.opcode = UART_OPC_SET_CSIZE;
		cmd.arg = bits;
		SerialDevice_Cmd(ua->backend, &cmd);
		cmd.opcode = UART_OPC_SET_BAUDRATE;
		cmd.arg = 9600;
		SerialDevice_Cmd(ua->backend, &cmd);
	}
	if (value & SCON_REN) {
		SerialDevice_StartRx(ua->backend);
	} else {
		SerialDevice_StopRx(ua->backend);
	}
}

static uint8_t
saden_read(void *eventData, uint8_t addr)
{
	return 0;
}

static void
saden_write(void *eventData, uint8_t addr, uint8_t value)
{
}

static uint8_t
saddr_read(void *eventData, uint8_t addr)
{
	return 0;
}

static void
saddr_write(void *eventData, uint8_t addr, uint8_t value)
{
}

static uint8_t
sbuf_read(void *eventData, uint8_t addr)
{
	AT89C51Uart *ua = eventData;
	//fprintf(stderr, "Read RXBUF %02x\n", ua->regRXBUF);
	return ua->regRXBUF;
}

static void
sbuf_write(void *eventData, uint8_t addr, uint8_t value)
{
	AT89C51Uart *ua = eventData;
	ua->regTXBUF = value;
	fprintf(stderr, "Write sbuf: 0x%02x\n",value);
	SerialDevice_StartTx(ua->backend);
}

static uint8_t
pcon_read(void *eventData, uint8_t addr)
{
	AT89C51Uart *ua = eventData;
	return ua->regPCON;
}

static void
pcon_write(void *eventData, uint8_t addr, uint8_t value)
{
	AT89C51Uart *ua = eventData;
	ua->regPCON = value;
	fprintf(stderr, "Warning regPCON not implemented\n");
}

void
AT89C51Uart_New(const char *name)
{
	AT89C51Uart *ua;
	ua = sg_new(AT89C51Uart);
	ua->backend = Uart_New(name, serial_input, serial_output, NULL, ua);
	ua->sigIrq = SigNode_New("%s.irq", name);
	ua->SM = 0xff;
	MCS51_RegisterSFR(REG_SCON, scon_read, NULL, scon_write, ua);
	MCS51_RegisterSFR(REG_SADEN, saden_read, NULL, saden_write, ua);
	MCS51_RegisterSFR(REG_SADDR, saddr_read, NULL, saddr_write, ua);
	MCS51_RegisterSFR(REG_SBUF, sbuf_read, NULL, sbuf_write, ua);
	MCS51_RegisterSFR(REG_PCON, pcon_read, NULL, pcon_write, ua);
}
