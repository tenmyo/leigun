/*
 *************************************************************************************************
 *
 * Emulation of the MCF5282 Fast Ethernet Controller 
 *
 * State:
 *	Not implemented
 *
 * Copyright 2008 Jochen Karrer. All rights reserved.
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

#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <linux-tap.h>
#include <bus.h>
#include <phy.h>
#include <signode.h>
#include <cycletimer.h>
#include <sgstring.h>
#include "coldfire/mcf5282_fec.h"

#define FEC_EIR(base)		((base) + 0x1004)
#define FEC_EIMR(base)		((base) + 0x1008)
#define FEC_RDAR(base)		((base) + 0x1010)
#define FEC_TDAR(base)		((base) + 0x1014)
#define FEC_ECR(base)		((base) + 0x1024)
#define FEC_MDATA(base)		((base) + 0x1040)
#define FEC_MSCR(base)		((base) + 0x1044)
#define FEC_MIBC(base)		((base) + 0x1064)
#define FEC_RCR(base)		((base) + 0x1084)
#define FEC_TCR(base)		((base) + 0x10c4)
#define FEC_PALR(base)		((base) + 0x10e4)
#define FEC_PAUR(base)		((base) + 0x10e8)
#define FEC_OPD(base)		((base) + 0x10ec)
#define FEC_IAUR(base)		((base) + 0x1118)
#define FEC_IALR(base)		((base) + 0x111c)
#define FEC_GAUR(base)		((base) + 0x1120)
#define FEC_GALR(base)		((base) + 0x1124)
#define FEC_TFWR(base)		((base) + 0x1144)
#define FEC_FRBR(base)		((base) + 0x114c)
#define FEC_FRSR(base)		((base) + 0x1150)
#define FEC_ERDSR(base)		((base) + 0x1180)
#define FEC_ETDSR(base)		((base) + 0x1184)
#define FEC_EMRBR(base)		((base) + 0x1188)

#define FEC_RMON_T_DROP(base)		((base) + 0x1200)
#define FEC_RMON_T_PACKETS(base)	((base) + 0x1204)
#define FEC_RMON_T_BC_PKT(base)		((base) + 0x1208)
#define FEC_RMON_T_MC_PKT(base)		((base) + 0x120c)
#define FEC_RMON_T_CRC_ALIGN(base)	((base) + 0x1210)
#define FEC_RMON_T_UNDERSIZE(base)	((base) + 0x1214)
#define FEC_RMON_T_OVERSIZE(base)	((base) + 0x1218)
#define FEC_RMON_T_FRAG(base)		((base) + 0x121c)
#define FEC_RMON_T_JAB(base)		((base) + 0x1220)
#define FEC_RMON_T_COL(base)		((base) + 0x1224)
#define FEC_RMON_T_P64(base)		((base) + 0x1228)
#define FEC_RMON_T_P65TO127(base)	((base) + 0x122c)
#define FEC_RMON_T_P128TO255(base)	((base) + 0x1230)
#define FEC_RMON_T_P256TO511(base)	((base) + 0x1234)
#define FEC_RMON_T_P512TO1023(base)	((base) + 0x1238)
#define FEC_RMON_T_P1024TO2047(base)	((base) + 0x123c)
#define FEC_RMON_T_P_GTE2048		((base) + 0x1240)
#define FEC_RMON_T_OCTETS(base)		((base) + 0x1244)
#define FEC_IEEE_T_DROP(base)		((base) + 0x1248)
#define FEC_IEEE_T_FRAME_OK(base)	((base) + 0x124c)
#define FEC_IEEE_T_1COL(base)		((base) + 0x1250)
#define FEC_IEEE_T_MCOL(base)		((base) + 0x1254)
#define FEC_IEEE_T_DEF(base)		((base) + 0x1258)
#define FEC_IEEE_T_LCOL(base)		((base) + 0x125c)
#define FEC_IEEE_T_EXCOL(base)		((base) + 0x1260)
#define FEC_IEEE_T_MACERR(base)		((base) + 0x1264)
#define FEC_IEEE_T_CSERR(base)		((base) + 0x1268)
#define FEC_IEEE_T_SQE(base)		((base) + 0x126c)
#define FEC_IEEE_T_FDXFC(base)		((base) + 0x1270)
#define FEC_IEEE_T_OCTETS_OK(base)	((base) + 0x1274)
#define FEC_RMON_R_PACKETS(base)	((base) + 0x1284)
#define FEC_RMON_R_BC_PKT(base)		((base) + 0x1288)

#define FEC_RMON_R_MC_PKT(base)		((base) + 0x128c)
#define FEC_RMON_R_CRC_ALIGN(base)	((base) + 0x1290)
#define FEC_RMON_R_UNDERSIZE(base)	((base) + 0x1294)
#define FEC_RMON_R_OVERSIZE(base)	((base) + 0x1298)
#define FEC_RMON_R_FRAG(base)		((base) + 0x129c)
#define FEC_RMON_R_JAB(base)		((base) + 0x12a0)
#define FEC_RMON_R_RESVD_0(base)	((base) + 0x12a4)
#define FEC_RMON_R_P64(base)		((base) + 0x12a8)
#define FEC_RMON_R_P65TO127(base)	((base) + 0x12ac)
#define FEC_RMON_R_P128TO255(base)	((base) + 0x12b0)
#define FEC_RMON_R_P256TO511(base)	((base) + 0x12b4)
#define FEC_RMON_R_P512TO1023(base)	((base) + 0x12b8)
#define FEC_RMON_R_P1024TO2047(base)	((base) + 0x12bc)
#define FEC_RMON_R_P_GTE2048(base)	((base) + 0x12c0)
#define FEC_RMON_R_OCTETS(base)		((base) + 0x12c4)
#define FEC_IEEE_R_DROP(base)		((base) + 0x12c8)
#define FEC_IEEE_R_FRAME_OK(base)	((base) + 0x12cc)
#define FEC_IEEE_R_CRC(base)		((base) + 0x12d0)
#define FEC_IEEE_R_ALIGN(base)		((base) + 0x12d4)
#define FEC_IEEE_R_MACERR(base)		((base) + 0x12d8)
#define FEC_IEEE_R_FDXFC(base)		((base) + 0x12dc)
#define FEC_IEEE_R_OCTETS_OK(base)	((base) + 0x12e0)

typedef struct Fec {
	BusDevice bdev;
} Fec;

static void
Fec_Unmap(void *owner, uint32_t base, uint32_t mask)
{
//        IOH_Delete32(SCM_IPSBAR(base));
}

static void
Fec_Map(void *owner, uint32_t base, uint32_t mask, uint32_t mapflags)
{
//       Fec *fec = (Fec *) owner;
//        IOH_New32(SCM_IPSBAR(base),ipsbar_read,ipsbar_write,scm);
}

BusDevice *
MCF5282_FecNew(const char *name)
{
	Fec *fec = sg_calloc(sizeof(Fec));
	fec->bdev.first_mapping = NULL;
	fec->bdev.Map = Fec_Map;
	fec->bdev.UnMap = Fec_Unmap;
	fec->bdev.owner = fec;
	fec->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	return &fec->bdev;

}
