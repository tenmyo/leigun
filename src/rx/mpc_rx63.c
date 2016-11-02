/*
 **********************************************************************************************
 * Renesas RX63 Multifunction Pin controller
 *
 * Copyright 2012-2013 Jochen Karrer. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice, this list of
 *       conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright notice, this list
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
 **********************************************************************************************
 */

#include "bus.h"
#include "sgstring.h"
#include "signode.h"
#include "mpc_rx63.h"
#include "cpu_rx.h"

#define REG_PWPR(base) ((base) + 0x11f)
#define     PWPR_PFSWE   (1 << 6)
#define     PWPR_B0WI   (1 << 7)

#define REG_P0nPFS(base, pin) ((base) + 0x140 + (pin))
#define REG_P1nPFS(base, pin) ((base) + 0x148 + (pin))
#define REG_P2nPFS(base, pin) ((base) + 0x150 + (pin))
#define REG_P3nPFS(base, pin) ((base) + 0x158 + (pin))
#define REG_P4nPFS(base, pin) ((base) + 0x160 + (pin))
#define REG_P5nPFS(base, pin) ((base) + 0x168 + (pin))
#define REG_P6nPFS(base, pin) ((base) + 0x170 + (pin))
#define REG_P7nPFS(base, pin) ((base) + 0x178 + (pin))
#define REG_P8nPFS(base, pin) ((base) + 0x180 + (pin))
#define REG_P9nPFS(base, pin) ((base) + 0x188 + (pin))
#define REG_PAnPFS(base, pin) ((base) + 0x190 + (pin))
#define REG_PBnPFS(base, pin) ((base) + 0x198 + (pin))
#define REG_PBnPFS(base, pin) ((base) + 0x198 + (pin))
#define REG_PCnPFS(base, pin) ((base) + 0x1A0 + (pin))
#define REG_PDnPFS(base, pin) ((base) + 0x1A8 + (pin))
#define REG_PEnPFS(base, pin) ((base) + 0x1B0 + (pin))
#define REG_PFnPFS(base, pin) ((base) + 0x1B8 + (pin))
#define REG_PGnPFS(base, pin) ((base) + 0x1C0 + (pin)) /* Only RX63T */
#define REG_PJnPFS(base, pin) ((base) + 0x1D0 + (pin)) /* Only RX63N/631 */

#define REG_USB_DPUPE(base)  ((base) + 0x1D0)
    
typedef struct RxMpc {
    BusDevice bdev;
    uint8_t regPWPR;
    uint8_t regP0nPFS[8];
    uint8_t regP1nPFS[8];
    uint8_t regP2nPFS[8];
    uint8_t regP3nPFS[8];
    uint8_t regP4nPFS[8];
    uint8_t regP5nPFS[8];
    uint8_t regP6nPFS[8];
    uint8_t regP7nPFS[8];
    uint8_t regP8nPFS[8];
    uint8_t regP9nPFS[8];
    uint8_t regPAnPFS[8];
    uint8_t regPBnPFS[8];
    uint8_t regPCnPFS[8];
    uint8_t regPDnPFS[8];
    uint8_t regPEnPFS[8];
    uint8_t regPFnPFS[8];
    uint8_t regPGnPFS[8];
    uint8_t regPJnPFS[8];
} RxMpc;

static uint32_t
pwpr_read(void *clientData, uint32_t address, int rqlen)
{
    RxMpc *mpc = clientData;
    return mpc->regPWPR;
}

static void
pwpr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxMpc *mpc = clientData;
    if (mpc->regPWPR & PWPR_B0WI) {
        mpc->regPWPR = value & PWPR_B0WI;
    } else {
        mpc->regPWPR = value & (PWPR_B0WI | PWPR_PFSWE);
    }
}

static uint32_t
p0npfs_read(void *clientData, uint32_t address, int rqlen)
{
    RxMpc *mpc = clientData;
    unsigned int pin = address & 7;
    return mpc->regP0nPFS[pin];
}

static void
p0npfs_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxMpc *mpc = clientData;
    unsigned int pin = address & 7;
    if (!(mpc->regPWPR & PWPR_PFSWE))  {
        fprintf(stderr, "Register %s i write Protected\n", __func__);
        return;
    }
    if (pin <= 3) {
        mpc->regP0nPFS[pin] = value & 0xDf;
    }
}
static uint32_t
p1npfs_read(void *clientData, uint32_t address, int rqlen)
{
    RxMpc *mpc = clientData;
    unsigned int pin = address & 7;
    return mpc->regP1nPFS[pin];
}

static void
p1npfs_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxMpc *mpc = clientData;
    unsigned int pin = address & 7;
    if (!(mpc->regPWPR & PWPR_PFSWE))  {
        fprintf(stderr, "Register %s i write Protected\n", __func__);
        return;
    }
    if (pin <= 4) {
        mpc->regP1nPFS[pin] = value & 0x5f;
    }
}
static uint32_t
p2npfs_read(void *clientData, uint32_t address, int rqlen)
{
    RxMpc *mpc = clientData;
    unsigned int pin = address & 7;
    return mpc->regP0nPFS[pin];
}

static void
p2npfs_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxMpc *mpc = clientData;
    unsigned int pin = address & 7;
    if (!(mpc->regPWPR & PWPR_PFSWE))  {
        fprintf(stderr, "Register %s i write Protected\n", __func__);
        return;
    }
    if (pin <= 6) {
        mpc->regP2nPFS[pin] = value & 0x5f;
    }
}

static uint32_t
p3npfs_read(void *clientData, uint32_t address, int rqlen)
{
    RxMpc *mpc = clientData;
    unsigned int pin = address & 7;
    return mpc->regP3nPFS[pin];
}

static void
p3npfs_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxMpc *mpc = clientData;
    unsigned int pin = address & 7;
    if (!(mpc->regPWPR & PWPR_PFSWE))  {
        fprintf(stderr, "Register %s i write Protected\n", __func__);
        return;
    }
    if (pin <= 5) {
        mpc->regP3nPFS[pin] = value & 0x5f;
    }
}
static uint32_t
p4npfs_read(void *clientData, uint32_t address, int rqlen)
{
    RxMpc *mpc = clientData;
    unsigned int pin = address & 7;
    return mpc->regP4nPFS[pin];
}

static void
p4npfs_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxMpc *mpc = clientData;
    unsigned int pin = address & 7;
    if (!(mpc->regPWPR & PWPR_PFSWE))  {
        fprintf(stderr, "Register %s i write Protected\n", __func__);
        return;
    }
    if (pin <= 7) {
        mpc->regP4nPFS[pin] = value & 0xDf;
    }
}
static uint32_t
p5npfs_read(void *clientData, uint32_t address, int rqlen)
{
    RxMpc *mpc = clientData;
    unsigned int pin = address & 7;
    return mpc->regP5nPFS[pin];
}
static void
p5npfs_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxMpc *mpc = clientData;
    unsigned int pin = address & 7;
    if (!(mpc->regPWPR & PWPR_PFSWE))  {
        fprintf(stderr, "Register %s i write Protected\n", __func__);
        return;
    }
    if (pin <= 7) {
        mpc->regP5nPFS[pin] = value & 0x5f;
    }
}

static uint32_t
p6npfs_read(void *clientData, uint32_t address, int rqlen)
{
    RxMpc *mpc = clientData;
    unsigned int pin = address & 7;
    return mpc->regP6nPFS[pin];
}

static void
p6npfs_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxMpc *mpc = clientData;
    unsigned int pin = address & 7;
    if (!(mpc->regPWPR & PWPR_PFSWE))  {
        fprintf(stderr, "Register %s i write Protected\n", __func__);
        return;
    }
    if (pin <= 5) {
        mpc->regP6nPFS[pin] = value & 0x5f;
    }
}

static uint32_t
p7npfs_read(void *clientData, uint32_t address, int rqlen)
{
    RxMpc *mpc = clientData;
    unsigned int pin = address & 7;
    return mpc->regP7nPFS[pin];
}

static void
p7npfs_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxMpc *mpc = clientData;
    unsigned int pin = address & 7;
    if (!(mpc->regPWPR & PWPR_PFSWE))  {
        fprintf(stderr, "Register %s i write Protected\n", __func__);
        return;
    }
    if (pin <= 6) {
        mpc->regP7nPFS[pin] = value & 0x1f;
    }
}
static uint32_t
p8npfs_read(void *clientData, uint32_t address, int rqlen)
{
    RxMpc *mpc = clientData;
    unsigned int pin = address & 7;
    return mpc->regP8nPFS[pin];
}

static void
p8npfs_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxMpc *mpc = clientData;
    unsigned int pin = address & 7;
    if (!(mpc->regPWPR & PWPR_PFSWE))  {
        fprintf(stderr, "Register %s i write Protected\n", __func__);
        return;
    }
    if (pin <= 2) {
        mpc->regP8nPFS[pin] = value & 0x1f;
    }
}
static uint32_t
p9npfs_read(void *clientData, uint32_t address, int rqlen)
{
    RxMpc *mpc = clientData;
    unsigned int pin = address & 7;
    return mpc->regP9nPFS[pin];
}

static void
p9npfs_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxMpc *mpc = clientData;
    unsigned int pin = address & 7;
    if (!(mpc->regPWPR & PWPR_PFSWE))  {
        fprintf(stderr, "Register %s i write Protected\n", __func__);
        return;
    }
    if (pin <= 6) {
        mpc->regP9nPFS[pin] = value & 0x9f;
    }
}

static uint32_t
panpfs_read(void *clientData, uint32_t address, int rqlen)
{
    RxMpc *mpc = clientData;
    unsigned int pin = address & 7;
    return mpc->regPAnPFS[pin];
}

static void
panpfs_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxMpc *mpc = clientData;
    unsigned int pin = address & 7;
    if (!(mpc->regPWPR & PWPR_PFSWE))  {
        fprintf(stderr, "Register %s i write Protected\n", __func__);
        return;
    }
    if (pin <= 6) {
        mpc->regPAnPFS[pin] = value & 0x5f;
    }
}

static uint32_t
pbnpfs_read(void *clientData, uint32_t address, int rqlen)
{
    RxMpc *mpc = clientData;
    unsigned int pin = address & 7;
    return mpc->regPBnPFS[pin];
}

static void
pbnpfs_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxMpc *mpc = clientData;
    unsigned int pin = address & 7;
    if (!(mpc->regPWPR & PWPR_PFSWE))  {
        fprintf(stderr, "Register %s i write Protected\n", __func__);
        return;
    }
    if (pin <= 7) {
        mpc->regPBnPFS[pin] = value & 0x5f;
    }
}
static uint32_t
pcnpfs_read(void *clientData, uint32_t address, int rqlen)
{
    RxMpc *mpc = clientData;
    unsigned int pin = address & 7;
    return mpc->regPCnPFS[pin];
}

static void
pcnpfs_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxMpc *mpc = clientData;
    unsigned int pin = address & 7;
    if (!(mpc->regPWPR & PWPR_PFSWE))  {
        fprintf(stderr, "Register %s i write Protected\n", __func__);
        return;
    }
    if (pin <= 7) {
        mpc->regPCnPFS[pin] = value & 0x5f;
    }
}

static uint32_t
pdnpfs_read(void *clientData, uint32_t address, int rqlen)
{
    RxMpc *mpc = clientData;
    unsigned int pin = address & 7;
    return mpc->regPDnPFS[pin];
}

static void
pdnpfs_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxMpc *mpc = clientData;
    unsigned int pin = address & 7;
    if (!(mpc->regPWPR & PWPR_PFSWE))  {
        fprintf(stderr, "Register %s i write Protected\n", __func__);
        return;
    }
    if (pin <= 7) {
        mpc->regPDnPFS[pin] = value & 0x5f;
    }
}

static uint32_t
penpfs_read(void *clientData, uint32_t address, int rqlen)
{
    RxMpc *mpc = clientData;
    unsigned int pin = address & 7;
    return mpc->regPEnPFS[pin];
}

static void
penpfs_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxMpc *mpc = clientData;
    unsigned int pin = address & 7;
    if (!(mpc->regPWPR & PWPR_PFSWE))  {
        fprintf(stderr, "Register %s i write Protected\n", __func__);
        return;
    }
    if (pin <= 5) {
        mpc->regPEnPFS[pin] = value & 0x5f;
    }
}

static uint32_t
pfnpfs_read(void *clientData, uint32_t address, int rqlen)
{
    RxMpc *mpc = clientData;
    unsigned int pin = address & 7;
    return mpc->regPFnPFS[pin];
}

static void
pfnpfs_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxMpc *mpc = clientData;
    unsigned int pin = address & 7;
    if (!(mpc->regPWPR & PWPR_PFSWE))  {
        fprintf(stderr, "Register %s i write Protected\n", __func__);
        return;
    }
    if ((pin == 2) || (pin == 3)) {
        mpc->regPFnPFS[pin] = value & 0x5f;
    }
}

static uint32_t
pgnpfs_read(void *clientData, uint32_t address, int rqlen)
{
    RxMpc *mpc = clientData;
    unsigned int pin = address & 7;
    return mpc->regPGnPFS[pin];
}

static void
pgnpfs_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxMpc *mpc = clientData;
    unsigned int pin = address & 7;
    if (!(mpc->regPWPR & PWPR_PFSWE))  {
        fprintf(stderr, "Register %s i write Protected\n", __func__);
        return;
    }
    if (pin <= 6) {
        mpc->regPGnPFS[pin] = value & 0x5f;
    }
}

static uint32_t
pjnpfs_read(void *clientData, uint32_t address, int rqlen)
{
    RxMpc *mpc = clientData;
    unsigned int pin = address & 7;
    return mpc->regPJnPFS[pin];
}

static void
pjnpfs_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RxMpc *mpc = clientData;
    unsigned int pin = address & 7;
    if (!(mpc->regPWPR & PWPR_PFSWE))  {
        fprintf(stderr, "Register %s i write Protected\n", __func__);
        return;
    }
    if (pin == 3) {
        mpc->regPJnPFS[pin] = value & 0x1f;
    }
}

static void
MPC_UnMap(void *module_owner, uint32_t base, uint32_t mapsize)
{
    unsigned int pin;
    IOH_Delete8(REG_PWPR(base)); 
    for (pin = 0; pin < 8; pin++) { 
        IOH_Delete8(REG_P0nPFS(base, pin));
        IOH_Delete8(REG_P1nPFS(base, pin));
        IOH_Delete8(REG_P2nPFS(base, pin));
        IOH_Delete8(REG_P3nPFS(base, pin));
        IOH_Delete8(REG_P4nPFS(base, pin));
        IOH_Delete8(REG_P5nPFS(base, pin));
        IOH_Delete8(REG_P6nPFS(base, pin));
        IOH_Delete8(REG_P7nPFS(base, pin));
        IOH_Delete8(REG_P8nPFS(base, pin));
        IOH_Delete8(REG_P9nPFS(base, pin));
        IOH_Delete8(REG_PAnPFS(base, pin));
        IOH_Delete8(REG_PBnPFS(base, pin));
        IOH_Delete8(REG_PCnPFS(base, pin));
        IOH_Delete8(REG_PDnPFS(base, pin));
        IOH_Delete8(REG_PEnPFS(base, pin));
        IOH_Delete8(REG_PFnPFS(base, pin));
        IOH_Delete8(REG_PGnPFS(base, pin));
        IOH_Delete8(REG_PJnPFS(base, pin));
    }
}

static void
MPC_Map(void *module_owner, uint32_t base, uint32_t mapsize, uint32_t flags)
{
    RxMpc *mpc = module_owner;
    int pin;
    IOH_New8(REG_PWPR(base), pwpr_read, pwpr_write, mpc); 
    for (pin = 0; pin < 8; pin++) { 
        IOH_New8(REG_P0nPFS(base, pin), p0npfs_read,p0npfs_write, mpc);
        IOH_New8(REG_P1nPFS(base, pin), p1npfs_read,p1npfs_write, mpc);
        IOH_New8(REG_P2nPFS(base, pin), p2npfs_read,p2npfs_write, mpc);
        IOH_New8(REG_P3nPFS(base, pin), p3npfs_read,p3npfs_write, mpc);
        IOH_New8(REG_P4nPFS(base, pin), p4npfs_read,p4npfs_write, mpc);
        IOH_New8(REG_P5nPFS(base, pin), p5npfs_read,p5npfs_write, mpc);
        IOH_New8(REG_P6nPFS(base, pin), p6npfs_read,p6npfs_write, mpc);
        IOH_New8(REG_P7nPFS(base, pin), p7npfs_read,p7npfs_write, mpc);
        IOH_New8(REG_P8nPFS(base, pin), p8npfs_read,p8npfs_write, mpc);
        IOH_New8(REG_P9nPFS(base, pin), p9npfs_read,p9npfs_write, mpc);
        IOH_New8(REG_PAnPFS(base, pin), panpfs_read,panpfs_write, mpc);
        IOH_New8(REG_PBnPFS(base, pin), pbnpfs_read,pbnpfs_write, mpc);
        IOH_New8(REG_PCnPFS(base, pin), pcnpfs_read,pcnpfs_write, mpc);
        IOH_New8(REG_PDnPFS(base, pin), pdnpfs_read,pdnpfs_write, mpc);
        IOH_New8(REG_PEnPFS(base, pin), penpfs_read,penpfs_write, mpc);
        IOH_New8(REG_PFnPFS(base, pin), pfnpfs_read,pfnpfs_write, mpc);
        IOH_New8(REG_PGnPFS(base, pin), pgnpfs_read,pgnpfs_write, mpc);
        IOH_New8(REG_PJnPFS(base, pin), pjnpfs_read,pjnpfs_write, mpc);
    }
}

BusDevice *
Rx63Mpc_New(const char *name)
{
    RxMpc *mpc = sg_new(RxMpc);
    mpc->bdev.first_mapping = NULL;
    mpc->bdev.Map = MPC_Map;
    mpc->bdev.UnMap = MPC_UnMap;
    mpc->bdev.owner = mpc;
    mpc->bdev.hw_flags = MEM_FLAG_READABLE | MEM_FLAG_WRITABLE;
    mpc->regPWPR = 0x80;
    return &mpc->bdev;
}
