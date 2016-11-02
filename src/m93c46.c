/*
 *******************************************************************************************************
 *
 * Pin Level Emulation of the M93c46 Microwire EEProm
 * required for the emulation of the STE10/100 
 * Network chip.
 *
 * State: Working
 *
 * Copyright 2004 Jochen Karrer. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice, this list of
 *       conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright notice, this list
 *       of conditions and the following disclaimer in the documentation and/or other materials
 *       provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY Jochen Karrer ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are those of the
 * authors and should not be interpreted as representing official policies, either expressed
 * or implied, of Jochen Karrer.
 *
 *************************************************************************************************
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "configfile.h"
#include "m93c46.h"
#include "sgstring.h"

#if 0
#define dbgprintf(x...) { fprintf(stderr,x); }
#else
#define dbgprintf(x...)
#endif

#define STATE_IDLE 	      (0)
#define STATE_WAIT_FOR_START  (3)
#define STATE_COMMAND         (1)
#define STATE_READ            (6)
#define STATE_WRITE           (7)
#define STATE_IGNORE	      (8)
#define STATE_WRAL	      (9)

#define  CMD_READ	(2)
#define  CMD_ERASE	(3)
#define  CMD_WRITE	(1)

#define BP_START (0)
#define BP_CLK (1)
#define BP_DONE	 (2)

static void 
print_state(uint8_t pinstate,int state) {
	if(pinstate&MW_CS) {
		dbgprintf("S");
	} else {
		dbgprintf("s");
	}
	if(pinstate&MW_SCLK) {
		dbgprintf("C");
	} else {
		dbgprintf("c");
	}
	if(pinstate&MW_SDI) {
		dbgprintf("I");
	} else {
		dbgprintf("i");
	}
	if(pinstate&MW_SDO) {
		dbgprintf("O");
	} else {
		dbgprintf("o");
	}
	dbgprintf(" - ");
	switch(state) {
		case STATE_IDLE: 
			dbgprintf("IDLE ");
			break;
		case STATE_WAIT_FOR_START: 
			dbgprintf("WAIT ");
			break;
		case STATE_COMMAND: 
			dbgprintf("CMD  ");
			break;
		case STATE_WRAL:
			dbgprintf("WRAL");
			break;
		case STATE_READ: 
			dbgprintf("READ ");
			break;
		case STATE_WRITE:
			dbgprintf("WRITE");
			break;
		case STATE_IGNORE:
			dbgprintf("IGN  ");
			break;
	}
}

/*
 * --------------------------------------------------------------------------------------------
 * Feed the state machine
 * --------------------------------------------------------------------------------------------
 */
static uint8_t
m93c46_feed(M93C46 *eprom) {
	int sdi = eprom->pinstate & MW_SDI;
	int sclk = eprom->pinstate & MW_SCLK;
	int oldsclk = eprom->oldpinstate & MW_SCLK;
	dbgprintf("eprom state %d, pinstate %02x,old %02x, ",eprom->state,eprom->pinstate,eprom->oldpinstate);
	print_state(eprom->pinstate,eprom->state);
	if(!(eprom->pinstate&MW_CS)) {
		eprom->pinstate |= MW_SDO;
		eprom->state=STATE_IDLE;		
		eprom->inbuf=0;
		dbgprintf(" - not selected\n");
		return eprom->pinstate;
	}
	switch (eprom->state) {
		case STATE_IDLE:
			if(sclk && !oldsclk) {
				eprom->state = STATE_WAIT_FOR_START;
			} 	
			break;

		case STATE_WAIT_FOR_START:
			if(sdi && sclk && !oldsclk) {
				eprom->state = STATE_COMMAND;
				eprom->expected_bits = 8;
				eprom->inbuf = 0;
			}	
			break;
			
		case STATE_COMMAND:
			if(sclk && !oldsclk) {
				eprom->inbuf<<=1;
				if(sdi) {
					eprom->inbuf |=1;
				}	
				eprom->expected_bits--;
				if(eprom->expected_bits==0) {
                                        switch(eprom->inbuf >> 6) {
                                                case  CMD_READ:
                                        		eprom->address=eprom->inbuf & 0x3f;
							eprom->state=STATE_READ;
							eprom->inbuf=0;
							eprom->expected_bits=16;
                                                        break;
                                                case  CMD_WRITE:
                                        		eprom->address=eprom->inbuf & 0x3f;
							eprom->state=STATE_WRITE;
							eprom->inbuf=0;
							eprom->expected_bits=16;
                                                        break;
						case  CMD_ERASE:
							dbgprintf("ERASE\n");
							if(eprom->write_enabled) {
								eprom->address = eprom->inbuf & 0x3f;
								eprom->data[eprom->address*2]=0xff;
								eprom->data[eprom->address*2 + 1]=0xff;
							}
                                                        break;
						case  0:
							if((eprom->inbuf >> 4)  == 0x3) {
								eprom->write_enabled = 1;
								dbgprintf(stderr,"EWEN\n");
							} else if((eprom->inbuf >> 4)  == 0) {
								eprom->write_enabled = 0;
								dbgprintf("EWDS\n");
							} else if((eprom->inbuf >> 4)  == 2) {
								if(eprom->write_enabled) {
									int i;
									for(i=0;i<128;i++) {
										eprom->data[i]=0xff;
									}
								}
								dbgprintf("ERAL\n");
							} else if((eprom->inbuf >> 4)  == 1) {
								dbgprintf("WRAL\n");
                                                        	eprom->state=STATE_WRAL;
								eprom->inbuf = 0;
								eprom->expected_bits = 16;
							}
                                                        eprom->state=STATE_IGNORE;
							break;
                                                default:
							fprintf(stderr,"command %d not implemended\n",eprom->inbuf);
                                                        eprom->state=STATE_IGNORE;
                                                        break;
                                        }
                                }
			}
			break;
			

		case STATE_READ:
			if(sclk && !oldsclk) {
				uint16_t data=eprom->data[2*eprom->address] + (eprom->data[2*eprom->address+1]<<8);
                                int bitnr=eprom->expected_bits-1;
                                dbgprintf("addr %02x data %04x bit %d\n",eprom->address,data,bitnr);
                                if(data&(1<<bitnr)) {
                                	eprom->pinstate|=MW_SDO;
				} else {
                                	eprom->pinstate &= ~MW_SDO;
				}
				eprom->expected_bits--;
				if(eprom->expected_bits == 0) {
                                        eprom->state=STATE_IGNORE;
				}
			}	
			break;
		case STATE_WRAL:
			if(sclk && !oldsclk) {
				eprom->inbuf<<=1;
				if(sdi) {
					eprom->inbuf |=1;
				}	
				eprom->expected_bits--;
				if(eprom->expected_bits==0) {
					if(eprom->write_enabled) {
						int i;
						for(i=0;i<64;i++) {
							eprom->data[2*i]=eprom->inbuf&0xff;
							eprom->data[2*i+1]=(eprom->inbuf>>8)&0xff;
						}
					}
                                        eprom->state=STATE_IDLE;
                                        eprom->inbuf=0;
                                }
			}
			break;
		case STATE_WRITE:
			if(sclk && !oldsclk) {
				eprom->inbuf<<=1;
				if(sdi) {
					eprom->inbuf |=1;
				}	
				eprom->expected_bits--;
				if(eprom->expected_bits==0) {
					if(eprom->write_enabled) {
                                        	eprom->data[2*eprom->address]=eprom->inbuf&0xff;
                                        	eprom->data[2*eprom->address+1]=(eprom->inbuf>>8)&0xff;
					}
                                        eprom->state=STATE_IDLE;
                                        eprom->inbuf=0;
                                }
			}
			break;


		case STATE_IGNORE:
			break;
			
	}
	dbgprintf(" leave: ");
	print_state(eprom->pinstate,eprom->state);
	dbgprintf("\n");
	return eprom->pinstate;
}

/*
 * -------------------------------------------------------------------
 * This trace of the signal node is invoked whenever CLK line changes
 * -------------------------------------------------------------------
 */
static void 
SCLK_Change(SigNode *node,int value,void *clientData)
{
	M93C46 *eprom = clientData;
	int result;
	if(value==SIG_HIGH) {
		eprom->pinstate |= MW_SCLK;
	} else if (value == SIG_LOW)  {
		eprom->pinstate &= ~MW_SCLK;
	}
	result = m93c46_feed(eprom); 
	eprom->oldpinstate = eprom->pinstate;
	if(result & MW_SDO) {
	 	SigNode_Set(eprom->sdo,SIG_HIGH);
	} else {
	 	SigNode_Set(eprom->sdo,SIG_LOW);
	}
}

static void 
CSel_Change(SigNode *node,int value,void *clientData)
{
	M93C46 *eprom = clientData;
	int result;
        if(value == SIG_HIGH) {
                eprom->pinstate |= MW_CS;
        } else {
                eprom->pinstate &= ~MW_CS;
        }
        result = m93c46_feed(eprom);
        if(result & MW_SDO) {
                SigNode_Set(eprom->sdo,SIG_HIGH);
        } else {
                SigNode_Set(eprom->sdo,SIG_LOW);
        }
}

static void 
SDI_Change(SigNode *node,int value,void *clientData)
{
	M93C46 *eprom = clientData;
	int result;
	if(value==SIG_HIGH) {
		eprom->pinstate |= MW_SDI;
	} else if (value == SIG_LOW)  {
		eprom->pinstate &= ~MW_SDI;
	}
	result = m93c46_feed(eprom); 
	eprom->oldpinstate = eprom->pinstate;
	if(result & MW_SDO) {
	 	SigNode_Set(eprom->sdo,SIG_HIGH);
	} else {
	 	SigNode_Set(eprom->sdo,SIG_LOW);
	}
}

M93C46 *
m93c46_New(const char *name) {
	int i;
	char *dirname;
	M93C46 *eprom;
	eprom = sg_new(M93C46);
	eprom->size = 128;
        eprom->sclk = SigNode_New("%s.sclk",name);
        if(!eprom->sclk) {
                fprintf(stderr,"Can not create Microwire SCLK\n");
                exit(2763);
        }
	eprom->sclkTrace = SigNode_Trace(eprom->sclk,SCLK_Change,eprom);
        eprom->sdi = SigNode_New("%s.sdi",name);
        if(!eprom->sdi) {
                fprintf(stderr,"Can not create Microwire SDI\n");
                exit(2763);
        }
	eprom->sdiTrace = SigNode_Trace(eprom->sdi,SDI_Change,eprom);
        eprom->sdo = SigNode_New("%s.sdo",name);
        if(!eprom->sdo) {
                fprintf(stderr,"Can not create Microwire SDO\n");
                exit(2763);
        }
	eprom->csel = SigNode_New("%s.csel",name);
	if(!eprom->csel) {
                fprintf(stderr,"Can not create Chip select for Microwire\n");
                exit(2763);

	}
	eprom->cselTrace = SigNode_Trace(eprom->csel,CSel_Change,eprom);
	dirname=Config_ReadVar("global","imagedir");
        if(dirname) {
                char *imagename = alloca(strlen(dirname) + strlen(name) + 20);
                sprintf(imagename,"%s/%s.img",dirname,name);
                eprom->disk_image = DiskImage_Open(imagename,eprom->size,DI_RDWR | DI_CREAT_FF);
                if(!eprom->disk_image) {
                        fprintf(stderr,"Failed to open diskimage \"%s\"\n",imagename);
                        exit(1);
                }
                eprom->data = DiskImage_Mmap(eprom->disk_image);
	} else {
		eprom->data = sg_calloc(128);
		for(i=0;i<128;i++) {
			eprom->data[i]=0xff;
		}
	}
	fprintf(stderr,"M93C64 Microwire EEProm \"%s\" created\n",name);
	return eprom;
}
