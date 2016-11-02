#ifndef CAN_SOCKET_H
#define CAN_SOCKET_H

#include <stdint.h>
#include <fio.h>

/*****************************************************************/
/*   CAN message types                                           */
/*****************************************************************/
/* special address description flags for the CAN_ID */
#define CAN_EFF_FLAG 0x80000000U /* EFF/SFF is set in the MSB */
#define CAN_RTR_FLAG 0x40000000U /* remote transmission request */
#define CAN_ERR_FLAG 0x20000000U /* error frame */

/* valid bits in CAN ID for frame formats */
#define CAN_SFF_MASK 0x000007FFU /* standard frame format (SFF) */
#define CAN_EFF_MASK 0x1FFFFFFFU /* extended frame format (EFF) */
#define CAN_ERR_MASK 0x1FFFFFFFU /* omit EFF, RTR, ERR flags */

#define CAN_ID(msg) 		((msg)->can_id & CAN_EFF_MASK)
#define CAN_SET_ID(msg,id) 	((msg)->can_id = ((msg)->can_id & 0xe0000000) | (id))
#define CAN_SET_TYPE(msg,type)	((msg)->can_id = ((msg)->can_id & CAN_EFF_MASK) | type)

#define CAN_MSG_T_11(msg)	CAN_SET_TYPE(msg,0);	
#define CAN_MSG_T_11_RTR(msg)  	CAN_SET_TYPE(msg,CAN_RTR_FLAG) 
#define CAN_MSG_T_29(msg)	CAN_SET_TYPE(msg,CAN_EFF_FLAG);
#define CAN_MSG_T_29_RTR(msg)	CAN_SET_TYPE(msg,CAN_EFF_FLAG | CAN_RTR_FLAG)

#define CAN_MSG_29BIT(msg)	(((msg)->can_id & CAN_EFF_FLAG) == CAN_EFF_FLAG)
#define CAN_MSG_11BIT(msg)	(((msg)->can_id & CAN_EFF_FLAG) == 0)
#define CAN_MSG_RTR(msg)	((msg)->can_id & CAN_RTR_FLAG)


/* The TCP-to-CAN Gateway uses this message because SO_KEEPALIVE  */
/* timeout is 2 hours. Should be swallowed by the forwarder,      */
/* driver ignores it.                                             */
#define CAN_MSG_T_KEEPALIVE (0xff)
#define CAN_MSG_T_ERROR     (0xfe)

/*****************************************************************/
/*   Data structure for a CAN message                            */
/*****************************************************************/

typedef struct CAN_MSG
{
   uint32_t can_id;
   uint8_t  can_dlc;
   uint8_t  pad[3];
   uint8_t  data[8];           /* Data buffer */
} CAN_MSG;

typedef struct CanChipOperations {
	void (*receive) (void *clientData,CAN_MSG *msg);
} CanChipOperations;

typedef struct CanController  CanController;
/*
 * Exported Functions for the Chip emulator
 */
CanController * CanSocketInterface_New(CanChipOperations *cops,const char *name,void *clientData);
void CanSend(CanController *contr,CAN_MSG *msg);
void CanStopRx(CanController *contr);
void CanStartRx(CanController *contr);
#endif

