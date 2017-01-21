#define MPC_CS0 (0)
#define MPC_CS1 (1)
#define MPC_CS2 (2)
#define MPC_CS3 (3)
#define MPC_CS4 (4)
#define MPC_CS5 (5)
#define MPC_CS6 (6)
#define MPC_CS7 (7)

#define SPR_IMMR (638)
/* Internal space base */
#define IMMR_ISB(immr) ((immr) & 0xffff0000);
#define IMMR_PARTNUM(immr) (((immr)&0xff00)>>8)
#define IMMR_MASKNUM(immr) (((immr)&0xff))
typedef struct MPC8xx_MemCo MPC8xx_MemCo;

MPC8xx_MemCo *MPC8xx_MemController_New(PpcCpu *);
/* Register CPU external devices connected to a chip select */
void MPC8xx_RegisterDevice(MPC8xx_MemCo * memco, BusDevice * bdev, uint32_t cs);

/* Register CPU internal devices mapped into the immr area */
void MPC8xx_RegisterIntDev(MPC8xx_MemCo * memco, BusDevice * bdev);
