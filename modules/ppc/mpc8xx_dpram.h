BusDevice *MPC8xx_DPRamNew(char *dpram_name, uint32_t size, uint32_t immr_offset);
uint32_t MPC_DPRamRead(BusDevice * bdev, uint32_t addr, int size);
void MPC_DPRamWrite(BusDevice * bdev, uint32_t value, uint32_t addr, int size);
typedef void DPRamWriteProc(void *cd, uint32_t value, uint32_t addr, int rqlen);
void DPRam_Trace(BusDevice * dev, uint32_t addr, int len, DPRamWriteProc * proc, void *cd);
