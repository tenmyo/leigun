/*
 **********************************************************************
 * CLI i2c
 * 	Interface of the command line interface interpreter
 *	to the I2C bus of a simulated board
 **********************************************************************
 */
#include "interpreter.h"
#include "signode.h"
#include "cycletimer.h"
#include "sgstring.h"
#include "cli_i2c.h"

#if 0
//#define dbgprintf(x...) { fprintf(stderr,x); }
#define dbgprintf(x...) { fprintf(stdout,x); }
#else
#define dbgprintf(x...)
#endif


#define CODE_MEM_SIZE (1024)

#define INSTR_SDA_L             (0x01000000)
#define INSTR_SDA_H             (0x02000000)
#define INSTR_SCL_L             (0x03000000)
#define INSTR_SCL_H             (0x04000000)
#define INSTR_CHECK_ARB         (0x05000000)
#define INSTR_NDELAY            (0x06000000)
#define INSTR_SYNC              (0x07000000)
#define INSTR_END               (0x08000000)
#define INSTR_INTERRUPT         (0x09000000)
#define INSTR_CHECK_ACK         (0x0a000000)
#define INSTR_READ_SDA          (0x0b000000)
#define INSTR_READ_ACK          (0x0c000000)
#define INSTR_RXDATA_AVAIL      (0x0d000000)
#define INSTR_WAIT_BUS_FREE     (0x0e000000)

#define ACK     (5)
#define NACK    (6)
#define RXBUF_SIZE 20

typedef struct I2C_Timing {
        uint32_t t_hdsta;
        uint32_t t_low;
        uint32_t t_high;
        uint32_t t_susta;
        uint32_t t_hddat_max;
        uint32_t t_sudat;
        uint32_t t_susto;
        uint32_t t_buf;
} I2C_Timing;

typedef struct CliI2c {
	I2C_Timing i2c_timing;
        int ack;
	uint8_t rx_shift;
        uint32_t ip;
        uint32_t icount;
        uint32_t code[CODE_MEM_SIZE];
	SigNode *sda;
	SigNode *scl;

	/* The interpreter of the current request */
	Interp *req_interp;

	uint8_t rxbuf[RXBUF_SIZE];
	unsigned int rxbuf_wp;
        SigTrace *sclStretchTrace;
        CycleTimer ndelayTimer;
} CliI2c;

#define MAX_BUSES	(5)

#define RET_DONE                (0)
#define RET_DO_NEXT             (1)
#define RET_EMU_ERROR           (-3)

#define T_HDSTA(i2c) ((i2c)->i2c_timing.t_hdsta)
#define T_LOW(i2c)  ((i2c)->i2c_timing.t_low)
#define T_HIGH(i2c) ((i2c)->i2c_timing.t_high)
#define T_SUSTA(i2c) ((i2c)->i2c_timing.t_susta)
#define T_HDDAT_MAX(i2c) ((i2c)->i2c_timing.t_hddat_max)
#define T_HDDAT(i2c) (T_HDDAT_MAX(i2c)>>1)
#define T_SUDAT(i2c) ((i2c)->i2c_timing.t_sudat)
#define T_SUSTO(i2c) ((i2c)->i2c_timing.t_susto)
#define T_BUF(i2c) ((i2c)->i2c_timing.t_buf)

#define ADD_CODE(i2c,x) ((i2c)->code[((i2c)->icount++) % CODE_MEM_SIZE] = (x))

static CliI2c *g_i2cbuses[MAX_BUSES] = {0,};

#define EV_SCRIPT_DONE 	(5)
#define EV_LOST_ARB	(6)
#define EV_TIMEOUT	(7)
#define EV_NACK		(8)

#define LVL_HIGH	(SIG_PULLUP)

/*
 ***************************************************************************
 * reset_interpreter
 *      reset the I2C-master micro instruction interpreter.
 *      called before assembling a script or on abort of script.
 ***************************************************************************
 */
static void
reset_interpreter(CliI2c *i2c) {
        CycleTimer_Remove(&i2c->ndelayTimer);
        i2c->code[0] = INSTR_END;
        i2c->ip = 0;
        i2c->icount=0;
        i2c->rx_shift=0;
        i2c->ack=0;
}


static void
handle_event(CliI2c *i2c,uint32_t event) 
{
	if(event == EV_SCRIPT_DONE) {
		int i;
		for(i=0;i<i2c->rxbuf_wp;i++) {
			Interp_AppendResult(i2c->req_interp,"0x%02x ",i2c->rxbuf[i]);
		}
		if(i2c->rxbuf_wp) {
			Interp_AppendResult(i2c->req_interp,"\r\n");
		}
		Interp_FinishDelayed(i2c->req_interp,CMD_RESULT_OK);
	} else if(event == EV_LOST_ARB) {
		reset_interpreter(i2c);
		Interp_FinishDelayed(i2c->req_interp,CMD_RESULT_ERROR);
	} else if(event == EV_TIMEOUT) {
		reset_interpreter(i2c);
		Interp_FinishDelayed(i2c->req_interp,CMD_RESULT_ERROR);
	} else if(event == EV_NACK) {
		Interp_AppendResult(i2c->req_interp,"Nack\r\n");
		Interp_FinishDelayed(i2c->req_interp,CMD_RESULT_ERROR);
	} else {
		Interp_FinishDelayed(i2c->req_interp,CMD_RESULT_ERROR);
	}
	i2c->req_interp = 0;
}

static void
scl_timeout(void *clientData)
{
        CliI2c *i2c = (CliI2c *)clientData;
        SigNode * dom = SigNode_FindDominant(i2c->scl);
        if(dom) {
                fprintf(stderr,"CLI i2c: I2C SCL seems to be blocked by %s\n",SigName(dom));
        } else {
                fprintf(stderr,"CLI i2c: I2C SCL seems to be blocked level %d\n",SigNode_Val(i2c->scl));
        }
        SigNode_Untrace(i2c->scl,i2c->sclStretchTrace);
	handle_event(i2c,EV_TIMEOUT);
}

static void run_interpreter(void *clientData);

/*
 * -------------------------------------------------------------------
 * scl_released
 *      The SCL trace proc for clock stretching
 *      If it is called before timeout it will
 *      remove the timer and continue running the interpreter
 * -------------------------------------------------------------------
 */
static void
scl_released(SigNode *node,int value,void *clientData)
{
        CliI2c *i2c = (CliI2c *)clientData;
        if((value == SIG_PULLUP) || (value == SIG_HIGH)) {
                SigNode_Untrace(i2c->scl,i2c->sclStretchTrace);
                CycleTimer_Remove(&i2c->ndelayTimer);
                run_interpreter(clientData);
        }
}

static int
execute_instruction(CliI2c *i2c) {
        uint32_t icode;
	uint32_t instr; 
	uint32_t arg;
        icode = i2c->code[i2c->ip];
	instr = icode & 0xff000000; 
	arg =   icode & 0x00ffffff;
        i2c->ip = (i2c->ip + 1) % CODE_MEM_SIZE;
        switch(instr) {
                case INSTR_SDA_H:
                        dbgprintf("SDA_H %08x\n",icode);
                        SigNode_Set(i2c->sda,LVL_HIGH);
                        break;
                case INSTR_SDA_L:
                        dbgprintf("SDA_L %08x\n",icode);
                        SigNode_Set(i2c->sda,SIG_LOW);
                        break;
                case INSTR_SCL_H:
                        dbgprintf("SCL_H %08x\n",icode);
                        SigNode_Set(i2c->scl,LVL_HIGH);
                        break;
                case INSTR_SCL_L:
                        dbgprintf("SCL_L %08x\n",icode);
                        SigNode_Set(i2c->scl,SIG_LOW);
                        break;

                case INSTR_CHECK_ARB:
                        break;

                case INSTR_NDELAY:
                        {
                                uint32_t nsecs = icode & 0xffffff;
                                int64_t cycles = NanosecondsToCycles(nsecs);
                                CycleTimer_Add(&i2c->ndelayTimer,cycles,run_interpreter,i2c);
                                dbgprintf("NDELAY %08x\n",icode);
                        }
                        return RET_DONE;
                        break;

                case INSTR_SYNC:
                        dbgprintf("SYNC %08x\n",icode);
                        if(SigNode_Val(i2c->scl)==SIG_LOW) {
                                uint32_t msecs = 200;
                                int64_t cycles =  MillisecondsToCycles(msecs);
                                i2c->sclStretchTrace = SigNode_Trace(i2c->scl,scl_released,i2c);
                                CycleTimer_Add(&i2c->ndelayTimer,cycles,scl_timeout,i2c);
                                return RET_DONE;
                        }
                        break;


                case INSTR_READ_SDA:
                        if(SigNode_Val(i2c->sda) == SIG_LOW) {
                                i2c->rx_shift = (i2c->rx_shift<<1);
                        } else {
                                i2c->rx_shift = (i2c->rx_shift<<1) | 1;
                        }
                        break;

                case INSTR_READ_ACK:
                        if(SigNode_Val(i2c->sda) == SIG_LOW) {
                                i2c->ack = ACK;
                        } else {
                                i2c->ack = NACK;
                        }
                        dbgprintf("READ_ACK %08x: %d\n",icode,i2c->ack);
                        break;

                case  INSTR_END:
                        dbgprintf("ENDSCRIPT %08x\n",icode);
                        reset_interpreter(i2c);
                        return RET_DONE;
                        break;

                case  INSTR_CHECK_ACK:
                        dbgprintf("CHECK_ACK %08x\n",icode);
                        if(i2c->ack == NACK) {
                                /* I should urgently check if TXRDY is really set on NACK */
                                //i2c->sr |= SR_NACK | SR_TXRDY | SR_TXCOMP;
                                //update_interrupt(i2c);
				handle_event(i2c,EV_NACK);
                                return RET_DONE;
                        }
                        break;

                case  INSTR_INTERRUPT:
			handle_event(i2c,arg) ;
                        dbgprintf("INTERRUPT %08x\n",icode);
                        //i2c->sr |= icode & 0xffff;
                        //update_interrupt(i2c);
                        break;

                case  INSTR_RXDATA_AVAIL:
			if(i2c->rxbuf_wp < RXBUF_SIZE) {
				i2c->rxbuf[i2c->rxbuf_wp++] = i2c->rx_shift;
			}
                        dbgprintf("RXDATA_AVAIL %02x %08x\n",i2c->rx_shift,icode);
                        //i2c->rhr = i2c->rx_shift;
                        break;

                case  INSTR_WAIT_BUS_FREE:
                        /* Wait bus free currently not implemented */
                        break;


                default:
                        return RET_EMU_ERROR;
                        break;

        }
        return RET_DO_NEXT;
}

/*
 ********************************************************************
 * mscript_check_ack
 *      Assemble a micro operation script which checks
 *      for acknowledge.
 *      has to be entered with SCL low for at least T_HDDAT
 ********************************************************************
 */
static void
mscript_check_ack(CliI2c *i2c) {
        /* check ack of previous */
        ADD_CODE(i2c,INSTR_SDA_H);
        ADD_CODE(i2c,INSTR_NDELAY | (T_LOW(i2c)-T_HDDAT(i2c)));
        ADD_CODE(i2c,INSTR_SCL_H);
        ADD_CODE(i2c,INSTR_SYNC);
        ADD_CODE(i2c,INSTR_NDELAY | T_HIGH(i2c));
        ADD_CODE(i2c,INSTR_READ_ACK);
        ADD_CODE(i2c,INSTR_CHECK_ACK);
        ADD_CODE(i2c,INSTR_SCL_L);
        ADD_CODE(i2c,INSTR_NDELAY | T_HDDAT(i2c));
}

/*
 * --------------------------------------------------------------
 * mscript_write_byte
 *      Assemble a micro operation script which writes a
 *      byte to the I2C-Bus
 *      Has to be entered with SCL low for at least T_HDDAT
 * --------------------------------------------------------------
 */
static void
mscript_write_byte(CliI2c * i2c,uint8_t data)
{
        int i;
        for(i=7;i>=0;i--) {
                int bit = (data>>i) & 1;
                if(bit) {
                        ADD_CODE(i2c,INSTR_SDA_H);
                } else {
                        ADD_CODE(i2c,INSTR_SDA_L);
                }
                ADD_CODE(i2c,INSTR_NDELAY | (T_LOW(i2c)-T_HDDAT(i2c)));
                ADD_CODE(i2c,INSTR_SCL_H);
                ADD_CODE(i2c,INSTR_SYNC);
                ADD_CODE(i2c,INSTR_NDELAY | T_HIGH(i2c));
                if(bit) {
                        ADD_CODE(i2c,INSTR_CHECK_ARB);
                }
                ADD_CODE(i2c,INSTR_SCL_L);
                ADD_CODE(i2c,INSTR_NDELAY | T_HDDAT(i2c));
        }
        mscript_check_ack(i2c);
        ADD_CODE(i2c,INSTR_NDELAY | (T_LOW(i2c)-T_HDDAT(i2c)));
}

/*
 **************************************************************
 * mscript_do_ack
 *      Assemble a micro operation script which sends
 *      an acknowledge on I2C bus
 *      Enter with SCL low for at least T_HDDAT
 **************************************************************
 */
static void
mscript_do_ack(CliI2c *i2c, int ack)
{
        if(ack == ACK) {
                ADD_CODE(i2c,INSTR_SDA_L);
        } else {
                /* should already be in this state because do ack is done after reading only */
                ADD_CODE(i2c,INSTR_SDA_H);
        }
        ADD_CODE(i2c,INSTR_NDELAY | (T_LOW(i2c)-T_HDDAT(i2c)));
        ADD_CODE(i2c,INSTR_SCL_H);
        ADD_CODE(i2c,INSTR_SYNC);
        ADD_CODE(i2c,INSTR_NDELAY | T_HIGH(i2c));
        if(ack == NACK) {
                ADD_CODE(i2c,INSTR_CHECK_ARB);
        }
        ADD_CODE(i2c,INSTR_SCL_L);
        ADD_CODE(i2c,INSTR_NDELAY | T_HDDAT(i2c));
}

/*
 ************************************************************************
 * mscript_read_byte
 *      Assemble a micro operation script which reads a byte from
 *      I2C bus
 *      Enter with SCL low and T_HDDAT waited
 ************************************************************************
 */

static void
mscript_read_byte(CliI2c *i2c)
{
        int i;
        ADD_CODE(i2c,INSTR_SDA_H);
        for(i=7;i>=0;i--) {
                ADD_CODE(i2c,INSTR_NDELAY | (T_LOW(i2c)-T_HDDAT(i2c)));
                ADD_CODE(i2c,INSTR_SCL_H);
                ADD_CODE(i2c,INSTR_SYNC);
                ADD_CODE(i2c,INSTR_NDELAY | T_HIGH(i2c));
                ADD_CODE(i2c,INSTR_READ_SDA);
                ADD_CODE(i2c,INSTR_SCL_L);
                ADD_CODE(i2c,INSTR_NDELAY | (T_HDDAT(i2c)));
        }
        ADD_CODE(i2c,INSTR_RXDATA_AVAIL);
}


/*
 **************************************************************************
 * Create a start condition
 **************************************************************************
 */
#define STARTMODE_REPSTART (1)
#define STARTMODE_START (2)
static void
mscript_start(CliI2c *i2c,int startmode) {
        /* For repeated start do not assume SDA and SCL state */
        if(startmode == STARTMODE_REPSTART) {
                ADD_CODE(i2c,INSTR_SDA_H);
                ADD_CODE(i2c,INSTR_NDELAY | (T_LOW(i2c)-T_HDDAT(i2c)));

                ADD_CODE(i2c,INSTR_SCL_H);
                ADD_CODE(i2c,INSTR_SYNC);
                ADD_CODE(i2c,INSTR_NDELAY | T_HIGH(i2c));
        } else {
                ADD_CODE(i2c,INSTR_WAIT_BUS_FREE);
        }

        /* Generate a start condition */
        ADD_CODE(i2c,INSTR_CHECK_ARB);
        ADD_CODE(i2c,INSTR_SDA_L);
        ADD_CODE(i2c,INSTR_NDELAY | T_HDSTA(i2c));
        ADD_CODE(i2c,INSTR_SCL_L);
        ADD_CODE(i2c,INSTR_NDELAY | T_HDDAT(i2c));
}


/*
 * ----------------------------------------------------------
 * mscript_stop
 *      Assemble a micro operation script which generates a
 *      stop condition on I2C bus.
 *      Enter with SCL low for at least T_HDDAT
 * ----------------------------------------------------------
 */
static void
mscript_stop(CliI2c *i2c)
{
        ADD_CODE(i2c,INSTR_SDA_L);
        ADD_CODE(i2c,INSTR_NDELAY | (T_LOW(i2c)-T_HDDAT(i2c)));
        ADD_CODE(i2c,INSTR_SCL_H);
        ADD_CODE(i2c,INSTR_SYNC);
        ADD_CODE(i2c,INSTR_NDELAY | T_SUSTO(i2c));
        ADD_CODE(i2c,INSTR_SDA_H);
        ADD_CODE(i2c,INSTR_NDELAY | T_BUF(i2c));
}



/*
 * ----------------------------------------------------------------
 * run_interpreter
 *      The I2C-master micro instruction interpreter main loop
 *      executes instructions until the script is done or
 *      waits for some event
 * ----------------------------------------------------------------
 */
static void
run_interpreter(void *clientData)
{
        CliI2c *i2c = (CliI2c *) clientData;
        int retval;
        do {
                retval = execute_instruction(i2c);
        } while(retval == RET_DO_NEXT);
}

static void
abort_command_i2crw(Interp *interp, void *clientData) {
	CliI2c *i2c = (CliI2c *) clientData;
	reset_interpreter(i2c);
	Interp_FinishDelayed(interp,CMD_RESULT_ABORT);
}
/*
 * -----------------------------------------------------------------------------
 * start interpreter
 *      Start execution of micro operation I2C scripts
 * -----------------------------------------------------------------------------
 */
static int 
start_interpreter(Interp *interp,CliI2c *i2c) {
        if(CycleTimer_IsActive(&i2c->ndelayTimer)) {
                dbgprintf("Cli-I2C: Starting already running interp.\n");
                return CMD_RESULT_ERROR;
        }
        CycleTimer_Add(&i2c->ndelayTimer,0,run_interpreter,i2c);
	Interp_SetAbortProc(interp,abort_command_i2crw,i2c);
	return CMD_RESULT_DELAYED;
}

int
cmd_i2cr(Interp *interp,void *clientData,int argc,char *argv[])
{
	unsigned int bus,i2caddr,mem_addr;
	unsigned int count = 1;	
	int i;
	CliI2c *i2c;
	if((argc < 4)) {
		return CMD_RESULT_BADARGS;
	}
	if(sscanf(argv[1],"%d",&bus) != 1) {
		return CMD_RESULT_BADARGS;
	}
	if(sscanf(argv[2],"0x%02x",&i2caddr) != 1) {
		return CMD_RESULT_BADARGS;
	}
	if(sscanf(argv[3],"0x%02x",&mem_addr) != 1) {
		return CMD_RESULT_BADARGS;
	}
	if(argc > 4) {
		if(sscanf(argv[4],"%d",&count) != 1) {
			return CMD_RESULT_BADARGS;
		}
		if(count < 1) {
			count = 1;
		}
		if(count > 4) {
			count = 4;
		}
	}
	if(bus >= MAX_BUSES) {
		return CMD_RESULT_BADARGS;
	}
	i2c = g_i2cbuses[bus];
	if(!i2c) {
		return CMD_RESULT_BADARGS;
	}
	dbgprintf("argv[1] %s, argv[2] %s, argv[3] %s\n",argv[1],argv[2],argv[3]);
	dbgprintf("read from bus %d,i2ca 0x%02x,mema 0x%02x\n",bus,i2caddr,mem_addr);
	reset_interpreter(i2c);
	i2c->rxbuf_wp = 0;
	mscript_start(i2c,STARTMODE_START);
	mscript_write_byte(i2c,i2caddr & 0xfe);
	mscript_write_byte(i2c,mem_addr);
	mscript_start(i2c,STARTMODE_REPSTART);
	mscript_write_byte(i2c,i2caddr | 1);
	for(i = 0; i < count; i++) {
		mscript_read_byte(i2c);
		if(i == (count - 1)) {
			mscript_do_ack(i2c,NACK);
		} else {
			mscript_do_ack(i2c,ACK);
		}
	}
	mscript_stop(i2c);
        ADD_CODE(i2c,INSTR_INTERRUPT | EV_SCRIPT_DONE);
	ADD_CODE(i2c,INSTR_END); 
//	fprintf(stderr,"The instr counter is %d\n",i2c->icount);
	i2c->req_interp = interp;
	return start_interpreter(interp,i2c);
}

int
cmd_i2cw(Interp *interp,void *clientData,int argc,char *argv[])
{
	unsigned int bus,i2caddr,mem_addr,value[4];
	CliI2c *i2c;
	int i;
	if((argc < 5)) {
		return CMD_RESULT_BADARGS;
	} else if (argc > 8) {
		return CMD_RESULT_BADARGS;
	}
	if(sscanf(argv[1],"%d",&bus) != 1) {
		return CMD_RESULT_BADARGS;
	}
	if(sscanf(argv[2],"0x%02x",&i2caddr) != 1) {
		return CMD_RESULT_BADARGS;
	}
	if(sscanf(argv[3],"0x%02x",&mem_addr) != 1) {
		return CMD_RESULT_BADARGS;
	}
	for(i = 0; i < argc - 4; i ++) {
		if(sscanf(argv[4 + i],"0x%02x",&value[i]) != 1) {
			return CMD_RESULT_BADARGS;
		}
	}
	if(bus >= MAX_BUSES) {
		return CMD_RESULT_BADARGS;
	}
	i2c = g_i2cbuses[bus];
	if(!i2c) {
		return CMD_RESULT_BADARGS;
	}
	reset_interpreter(i2c);
	i2c->rxbuf_wp = 0;
	mscript_start(i2c,STARTMODE_START);
	mscript_write_byte(i2c,i2caddr & 0xfe);
	mscript_write_byte(i2c,mem_addr);
	for(i = 0; i < argc - 4; i ++) {
		mscript_write_byte(i2c,value[i]);
	}
	mscript_stop(i2c);
        ADD_CODE(i2c,INSTR_INTERRUPT | EV_SCRIPT_DONE);
	ADD_CODE(i2c,INSTR_END); 
//	fprintf(stderr,"The instr counter is %d\n",i2c->icount);
	i2c->req_interp = interp;
	return start_interpreter(interp,i2c);
}

static void
CliI2c_CreateBus(const char *name,unsigned int bus_nr) 
{
	CliI2c *i2c = sg_new(CliI2c); 
	I2C_Timing *t = &i2c->i2c_timing;
	if(bus_nr >= MAX_BUSES) {
		fprintf(stderr,"Illegal index for I2Cbus %d\n",bus_nr);
		exit(1);
	}
	i2c->sda = SigNode_New("%s.i2c%d.sda",name,bus_nr);
	i2c->scl = SigNode_New("%s.i2c%d.scl",name,bus_nr);
	SigNode_Set(i2c->sda,LVL_HIGH);
	SigNode_Set(i2c->scl,LVL_HIGH);
	if(!i2c->sda || !i2c->scl) {
		fprintf(stderr,"Can not create signal lines for CLI-I2C bus %d\n",bus_nr);
		exit(1);
	}
        t->t_hdsta = 600;
        t->t_low = 1300;
        t->t_high = 600;
        t->t_susta = 600;
        t->t_hddat_max = 900;
        t->t_sudat = 100;
        t->t_susto = 600;
       	t->t_buf = 1300;
	g_i2cbuses[bus_nr] = i2c;
}

void
CliI2c_Init(const char *name) 
{
        if(Cmd_Register("i2cr",cmd_i2cr,NULL) < 0) {
                fprintf(stderr,"Can not register setvar command\n");
                exit(1);
        }
        if(Cmd_Register("i2cw",cmd_i2cw,NULL) < 0) {
                fprintf(stderr,"Can not register getvar command\n");
                exit(1);
        }
	/* For now create one bus only */
	CliI2c_CreateBus(name,0);
}
