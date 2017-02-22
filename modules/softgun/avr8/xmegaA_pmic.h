#ifndef _XMEGA_A_PMIC_H
#define _XMEGA_A_PMIC_H
typedef struct PMic PMic;
PMic *XMegaA_PmicNew(const char *name, unsigned int nr_irqvects);
void PMIC_PostInterrupt(PMic * pmic, unsigned int intno, unsigned int intlv);
#endif
