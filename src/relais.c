/*
 * ------------------------------------------------------------
 * Crossover relais emulation
 * 	Input  low: a0 -> b0, a1 -> b1 
 * 	Input high: a0 -> b1, a1 -> b0 
 *
 *
 * (C) 2006 Jochen Karrer
 *   Author: Jochen Karrer
 *
 * ------------------------------------------------------------
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "signode.h"
#include "relais.h"
#include "sgstring.h"

typedef struct Relais {
	SigNode *a0Node;	
	SigNode *a1Node;	
	SigNode *b0Node;	
	SigNode *b1Node;	
	SigNode *coilNode;
	SigTrace *trace;
	int state;
} Relais;

#define WITH_SOUND
static void 
relais_switch(SigNode *node,int value,void *clientData)
{
	Relais *relais = (Relais*)clientData;
#ifdef WITH_SOUND
	int fd;
	char buf[50];
	int i;
	fprintf(stderr,"SWITCH to %d\n",value);
	fd=open("/dev/dsp",O_RDWR);
	if(fd>=0) {
		int count = 0;
		int result;
		for(i=0;i<sizeof(buf);i++) {
			buf[i] = rand();
		}
		do { 
			result = write(fd,buf,sizeof(buf));
			if(result <= 0) {
				break;
			}
			count+=result;
		} while(count < sizeof(buf));
		fprintf(stderr,"written %d\n",count);
		close(fd);
	}
#endif
	if(value == relais->state) {
		fprintf(stderr,"Emulator bug, Signal trace invocation without change\n");
	} else {
		relais->state = value;
	}
	if(value) {
		SigNode_RemoveLink(relais->a0Node,relais->b0Node);
		SigNode_RemoveLink(relais->a1Node,relais->b1Node);
		SigNode_Link(relais->a0Node,relais->b1Node);
		SigNode_Link(relais->a1Node,relais->b0Node);
	} else {
		SigNode_RemoveLink(relais->a0Node,relais->b1Node);
		SigNode_RemoveLink(relais->a1Node,relais->b0Node);
		SigNode_Link(relais->a0Node,relais->b0Node);
		SigNode_Link(relais->a1Node,relais->b1Node);
	}
}
void
Relais_New(const char *name)  
{
	Relais *relais = sg_new(Relais);
	relais->coilNode = SigNode_New("%s.coil",name); 
	relais->a0Node = SigNode_New("%s.a0",name); 
	relais->a1Node = SigNode_New("%s.a1",name); 
	relais->b0Node = SigNode_New("%s.b0",name); 
	relais->b1Node = SigNode_New("%s.b1",name); 
	if(!relais->coilNode || !relais->a0Node || !relais->a1Node
	|| !relais->b0Node || !relais->b1Node) {
		fprintf(stderr,"Can't create signal nodes for relais \"%s\"\n",name);
	}
	relais->state = SIG_LOW; 
	SigNode_Link(relais->a0Node,relais->b0Node);
	SigNode_Link(relais->a1Node,relais->b1Node);
	relais->trace = SigNode_Trace(relais->coilNode,relais_switch,relais);
}
