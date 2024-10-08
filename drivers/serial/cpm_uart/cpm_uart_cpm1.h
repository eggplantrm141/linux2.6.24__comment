/*
 * linux/drivers/serial/cpm_uart/cpm_uart_cpm1.h
 *
 * Driver for CPM (SCC/SMC) serial ports
 * 
 * definitions for cpm1
 *
 */

#ifndef CPM_UART_CPM1_H
#define CPM_UART_CPM1_H

#include <asm/commproc.h>

/* defines for IRQs */
#ifndef CONFIG_PPC_CPM_NEW_BINDING
#define SMC1_IRQ	(CPM_IRQ_OFFSET + CPMVEC_SMC1)
#define SMC2_IRQ	(CPM_IRQ_OFFSET + CPMVEC_SMC2)
#define SCC1_IRQ	(CPM_IRQ_OFFSET + CPMVEC_SCC1)
#define SCC2_IRQ	(CPM_IRQ_OFFSET + CPMVEC_SCC2)
#define SCC3_IRQ	(CPM_IRQ_OFFSET + CPMVEC_SCC3)
#define SCC4_IRQ	(CPM_IRQ_OFFSET + CPMVEC_SCC4)
#endif

static inline void cpm_set_brg(int brg, int baud)
{
	cpm_setbrg(brg, baud);
}

static inline void cpm_set_scc_fcr(scc_uart_t __iomem * sup)
{
	out_8(&sup->scc_genscc.scc_rfcr, SMC_EB);
	out_8(&sup->scc_genscc.scc_tfcr, SMC_EB);
}

static inline void cpm_set_smc_fcr(smc_uart_t __iomem * up)
{
	out_8(&up->smc_rfcr, SMC_EB);
	out_8(&up->smc_tfcr, SMC_EB);
}

#define DPRAM_BASE	((u8 __iomem __force *)cpm_dpram_addr(0))

#endif
