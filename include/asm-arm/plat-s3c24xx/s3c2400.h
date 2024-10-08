/* linux/include/asm-arm/plat-s3c24xx/s3c2400.h
 *
 * Copyright (c) 2004 Simtec Electronics
 *	Ben Dooks <ben@simtec.co.uk>
 *
 * Header file for S3C2400 cpu support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Modifications:
 *     09-Fev-2006 LCVR  First version, based on s3c2410.h
*/

#ifdef CONFIG_CPU_S3C2400

extern  int s3c2400_init(void);

extern void s3c2400_map_io(struct map_desc *mach_desc, int size);

extern void s3c2400_init_uarts(struct s3c2410_uartcfg *cfg, int no);

extern void s3c2400_init_clocks(int xtal);

#else
#define s3c2400_init_clocks NULL
#define s3c2400_init_uarts NULL
#define s3c2400_map_io NULL
#define s3c2400_init NULL
#endif
