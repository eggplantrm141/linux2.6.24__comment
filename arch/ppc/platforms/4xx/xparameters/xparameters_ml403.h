
/*******************************************************************
*
* CAUTION: This file is automatically generated by libgen.
* Version: Xilinx EDK 7.1.2 EDK_H.12.5.1
* DO NOT EDIT.
*
* Copyright (c) 2005 Xilinx, Inc.  All rights reserved. 
* 
* Description: Driver parameters
*
*******************************************************************/

#define XPAR_PLB_BRAM_IF_CNTLR_0_BASEADDR 0xFFFF0000
#define XPAR_PLB_BRAM_IF_CNTLR_0_HIGHADDR 0xFFFFFFFF

/******************************************************************/

#define XPAR_OPB_EMC_0_MEM0_BASEADDR 0x20000000
#define XPAR_OPB_EMC_0_MEM0_HIGHADDR 0x200FFFFF
#define XPAR_OPB_EMC_0_MEM1_BASEADDR 0x28000000
#define XPAR_OPB_EMC_0_MEM1_HIGHADDR 0x287FFFFF
#define XPAR_OPB_AC97_CONTROLLER_REF_0_BASEADDR 0xA6000000
#define XPAR_OPB_AC97_CONTROLLER_REF_0_HIGHADDR 0xA60000FF
#define XPAR_OPB_EMC_USB_0_MEM0_BASEADDR 0xA5000000
#define XPAR_OPB_EMC_USB_0_MEM0_HIGHADDR 0xA50000FF
#define XPAR_PLB_DDR_0_MEM0_BASEADDR 0x00000000
#define XPAR_PLB_DDR_0_MEM0_HIGHADDR 0x0FFFFFFF

/******************************************************************/

#define XPAR_XEMAC_NUM_INSTANCES 1
#define XPAR_OPB_ETHERNET_0_BASEADDR 0x60000000
#define XPAR_OPB_ETHERNET_0_HIGHADDR 0x60003FFF
#define XPAR_OPB_ETHERNET_0_DEVICE_ID 0
#define XPAR_OPB_ETHERNET_0_ERR_COUNT_EXIST 1
#define XPAR_OPB_ETHERNET_0_DMA_PRESENT 1
#define XPAR_OPB_ETHERNET_0_MII_EXIST 1

/******************************************************************/

#define XPAR_XUARTNS550_NUM_INSTANCES 1
#define XPAR_XUARTNS550_CLOCK_HZ 100000000
#define XPAR_OPB_UART16550_0_BASEADDR 0xA0000000
#define XPAR_OPB_UART16550_0_HIGHADDR 0xA0001FFF
#define XPAR_OPB_UART16550_0_DEVICE_ID 0

/******************************************************************/

#define XPAR_XGPIO_NUM_INSTANCES 3
#define XPAR_OPB_GPIO_0_BASEADDR 0x90000000
#define XPAR_OPB_GPIO_0_HIGHADDR 0x900001FF
#define XPAR_OPB_GPIO_0_DEVICE_ID 0
#define XPAR_OPB_GPIO_0_INTERRUPT_PRESENT 0
#define XPAR_OPB_GPIO_0_IS_DUAL 1
#define XPAR_OPB_GPIO_EXP_HDR_0_BASEADDR 0x90001000
#define XPAR_OPB_GPIO_EXP_HDR_0_HIGHADDR 0x900011FF
#define XPAR_OPB_GPIO_EXP_HDR_0_DEVICE_ID 1
#define XPAR_OPB_GPIO_EXP_HDR_0_INTERRUPT_PRESENT 0
#define XPAR_OPB_GPIO_EXP_HDR_0_IS_DUAL 1
#define XPAR_OPB_GPIO_CHAR_LCD_0_BASEADDR 0x90002000
#define XPAR_OPB_GPIO_CHAR_LCD_0_HIGHADDR 0x900021FF
#define XPAR_OPB_GPIO_CHAR_LCD_0_DEVICE_ID 2
#define XPAR_OPB_GPIO_CHAR_LCD_0_INTERRUPT_PRESENT 0
#define XPAR_OPB_GPIO_CHAR_LCD_0_IS_DUAL 0

/******************************************************************/

#define XPAR_XPS2_NUM_INSTANCES 2
#define XPAR_OPB_PS2_DUAL_REF_0_DEVICE_ID_0 0
#define XPAR_OPB_PS2_DUAL_REF_0_BASEADDR_0 0xA9000000
#define XPAR_OPB_PS2_DUAL_REF_0_HIGHADDR_0 (0xA9000000+0x3F)
#define XPAR_OPB_PS2_DUAL_REF_0_DEVICE_ID_1 1
#define XPAR_OPB_PS2_DUAL_REF_0_BASEADDR_1 (0xA9000000+0x1000)
#define XPAR_OPB_PS2_DUAL_REF_0_HIGHADDR_1 (0xA9000000+0x103F)

/******************************************************************/

#define XPAR_XIIC_NUM_INSTANCES 1
#define XPAR_OPB_IIC_0_BASEADDR 0xA8000000
#define XPAR_OPB_IIC_0_HIGHADDR 0xA80001FF
#define XPAR_OPB_IIC_0_DEVICE_ID 0
#define XPAR_OPB_IIC_0_TEN_BIT_ADR 0
#define XPAR_OPB_IIC_0_GPO_WIDTH 1

/******************************************************************/

#define XPAR_INTC_MAX_NUM_INTR_INPUTS 10
#define XPAR_XINTC_HAS_IPR 1
#define XPAR_XINTC_USE_DCR 0
#define XPAR_XINTC_NUM_INSTANCES 1
#define XPAR_OPB_INTC_0_BASEADDR 0xD1000FC0
#define XPAR_OPB_INTC_0_HIGHADDR 0xD1000FDF
#define XPAR_OPB_INTC_0_DEVICE_ID 0
#define XPAR_OPB_INTC_0_KIND_OF_INTR 0x00000000

/******************************************************************/

#define XPAR_INTC_SINGLE_BASEADDR 0xD1000FC0
#define XPAR_INTC_SINGLE_HIGHADDR 0xD1000FDF
#define XPAR_INTC_SINGLE_DEVICE_ID XPAR_OPB_INTC_0_DEVICE_ID
#define XPAR_OPB_ETHERNET_0_IP2INTC_IRPT_MASK 0X000001
#define XPAR_OPB_INTC_0_OPB_ETHERNET_0_IP2INTC_IRPT_INTR 0
#define XPAR_SYSTEM_USB_HPI_INT_MASK 0X000002
#define XPAR_OPB_INTC_0_SYSTEM_USB_HPI_INT_INTR 1
#define XPAR_MISC_LOGIC_0_PHY_MII_INT_MASK 0X000004
#define XPAR_OPB_INTC_0_MISC_LOGIC_0_PHY_MII_INT_INTR 2
#define XPAR_OPB_SYSACE_0_SYSACE_IRQ_MASK 0X000008
#define XPAR_OPB_INTC_0_OPB_SYSACE_0_SYSACE_IRQ_INTR 3
#define XPAR_OPB_AC97_CONTROLLER_REF_0_RECORD_INTERRUPT_MASK 0X000010
#define XPAR_OPB_INTC_0_OPB_AC97_CONTROLLER_REF_0_RECORD_INTERRUPT_INTR 4
#define XPAR_OPB_AC97_CONTROLLER_REF_0_PLAYBACK_INTERRUPT_MASK 0X000020
#define XPAR_OPB_INTC_0_OPB_AC97_CONTROLLER_REF_0_PLAYBACK_INTERRUPT_INTR 5
#define XPAR_OPB_IIC_0_IP2INTC_IRPT_MASK 0X000040
#define XPAR_OPB_INTC_0_OPB_IIC_0_IP2INTC_IRPT_INTR 6
#define XPAR_OPB_PS2_DUAL_REF_0_SYS_INTR2_MASK 0X000080
#define XPAR_OPB_INTC_0_OPB_PS2_DUAL_REF_0_SYS_INTR2_INTR 7
#define XPAR_OPB_PS2_DUAL_REF_0_SYS_INTR1_MASK 0X000100
#define XPAR_OPB_INTC_0_OPB_PS2_DUAL_REF_0_SYS_INTR1_INTR 8
#define XPAR_OPB_UART16550_0_IP2INTC_IRPT_MASK 0X000200
#define XPAR_OPB_INTC_0_OPB_UART16550_0_IP2INTC_IRPT_INTR 9

/******************************************************************/

#define XPAR_XTFT_NUM_INSTANCES 1
#define XPAR_PLB_TFT_CNTLR_REF_0_DCR_BASEADDR 0xD0000200
#define XPAR_PLB_TFT_CNTLR_REF_0_DCR_HIGHADDR 0xD0000207
#define XPAR_PLB_TFT_CNTLR_REF_0_DEVICE_ID 0

/******************************************************************/

#define XPAR_XSYSACE_MEM_WIDTH 16
#define XPAR_XSYSACE_NUM_INSTANCES 1
#define XPAR_OPB_SYSACE_0_BASEADDR 0xCF000000
#define XPAR_OPB_SYSACE_0_HIGHADDR 0xCF0001FF
#define XPAR_OPB_SYSACE_0_DEVICE_ID 0
#define XPAR_OPB_SYSACE_0_MEM_WIDTH 16

/******************************************************************/

#define XPAR_CPU_PPC405_CORE_CLOCK_FREQ_HZ 300000000

/******************************************************************/


/******************************************************************/

/* Linux Redefines */

/******************************************************************/

#define XPAR_UARTNS550_0_BASEADDR (XPAR_OPB_UART16550_0_BASEADDR+0x1000)
#define XPAR_UARTNS550_0_HIGHADDR XPAR_OPB_UART16550_0_HIGHADDR
#define XPAR_UARTNS550_0_CLOCK_FREQ_HZ XPAR_XUARTNS550_CLOCK_HZ
#define XPAR_UARTNS550_0_DEVICE_ID XPAR_OPB_UART16550_0_DEVICE_ID

/******************************************************************/

#define XPAR_INTC_0_BASEADDR XPAR_OPB_INTC_0_BASEADDR
#define XPAR_INTC_0_HIGHADDR XPAR_OPB_INTC_0_HIGHADDR
#define XPAR_INTC_0_KIND_OF_INTR XPAR_OPB_INTC_0_KIND_OF_INTR
#define XPAR_INTC_0_DEVICE_ID XPAR_OPB_INTC_0_DEVICE_ID

/******************************************************************/

#define XPAR_INTC_0_EMAC_0_VEC_ID XPAR_OPB_INTC_0_OPB_ETHERNET_0_IP2INTC_IRPT_INTR
#define XPAR_INTC_0_SYSACE_0_VEC_ID XPAR_OPB_INTC_0_OPB_SYSACE_0_SYSACE_IRQ_INTR
#define XPAR_INTC_0_IIC_0_VEC_ID XPAR_OPB_INTC_0_OPB_IIC_0_IP2INTC_IRPT_INTR
#define XPAR_INTC_0_PS2_1_VEC_ID XPAR_OPB_INTC_0_OPB_PS2_DUAL_REF_0_SYS_INTR2_INTR
#define XPAR_INTC_0_PS2_0_VEC_ID XPAR_OPB_INTC_0_OPB_PS2_DUAL_REF_0_SYS_INTR1_INTR
#define XPAR_INTC_0_UARTNS550_0_VEC_ID XPAR_OPB_INTC_0_OPB_UART16550_0_IP2INTC_IRPT_INTR

/******************************************************************/

#define XPAR_TFT_0_BASEADDR XPAR_PLB_TFT_CNTLR_REF_0_DCR_BASEADDR

/******************************************************************/

#define XPAR_EMAC_0_BASEADDR XPAR_OPB_ETHERNET_0_BASEADDR
#define XPAR_EMAC_0_HIGHADDR XPAR_OPB_ETHERNET_0_HIGHADDR
#define XPAR_EMAC_0_DMA_PRESENT XPAR_OPB_ETHERNET_0_DMA_PRESENT
#define XPAR_EMAC_0_MII_EXIST XPAR_OPB_ETHERNET_0_MII_EXIST
#define XPAR_EMAC_0_ERR_COUNT_EXIST XPAR_OPB_ETHERNET_0_ERR_COUNT_EXIST
#define XPAR_EMAC_0_DEVICE_ID XPAR_OPB_ETHERNET_0_DEVICE_ID

/******************************************************************/

#define XPAR_GPIO_0_BASEADDR XPAR_OPB_GPIO_0_BASEADDR_0
#define XPAR_GPIO_0_HIGHADDR XPAR_OPB_GPIO_0_HIGHADDR_0
#define XPAR_GPIO_0_DEVICE_ID XPAR_OPB_GPIO_0_DEVICE_ID_0
#define XPAR_GPIO_1_BASEADDR XPAR_OPB_GPIO_0_BASEADDR_1
#define XPAR_GPIO_1_HIGHADDR XPAR_OPB_GPIO_0_HIGHADDR_1
#define XPAR_GPIO_1_DEVICE_ID XPAR_OPB_GPIO_0_DEVICE_ID_1
#define XPAR_GPIO_2_BASEADDR XPAR_OPB_GPIO_EXP_HDR_0_BASEADDR_0
#define XPAR_GPIO_2_HIGHADDR XPAR_OPB_GPIO_EXP_HDR_0_HIGHADDR_0
#define XPAR_GPIO_2_DEVICE_ID XPAR_OPB_GPIO_EXP_HDR_0_DEVICE_ID_0
#define XPAR_GPIO_3_BASEADDR XPAR_OPB_GPIO_EXP_HDR_0_BASEADDR_1
#define XPAR_GPIO_3_HIGHADDR XPAR_OPB_GPIO_EXP_HDR_0_HIGHADDR_1
#define XPAR_GPIO_3_DEVICE_ID XPAR_OPB_GPIO_EXP_HDR_0_DEVICE_ID_1
#define XPAR_GPIO_4_BASEADDR XPAR_OPB_GPIO_CHAR_LCD_0_BASEADDR
#define XPAR_GPIO_4_HIGHADDR XPAR_OPB_GPIO_CHAR_LCD_0_HIGHADDR
#define XPAR_GPIO_4_DEVICE_ID XPAR_OPB_GPIO_CHAR_LCD_0_DEVICE_ID

/******************************************************************/

#define XPAR_PS2_0_BASEADDR XPAR_OPB_PS2_DUAL_REF_0_BASEADDR_0
#define XPAR_PS2_0_HIGHADDR XPAR_OPB_PS2_DUAL_REF_0_HIGHADDR_0
#define XPAR_PS2_0_DEVICE_ID XPAR_OPB_PS2_DUAL_REF_0_DEVICE_ID_0
#define XPAR_PS2_1_BASEADDR XPAR_OPB_PS2_DUAL_REF_0_BASEADDR_1
#define XPAR_PS2_1_HIGHADDR XPAR_OPB_PS2_DUAL_REF_0_HIGHADDR_1
#define XPAR_PS2_1_DEVICE_ID XPAR_OPB_PS2_DUAL_REF_0_DEVICE_ID_1

/******************************************************************/

#define XPAR_SYSACE_0_BASEADDR XPAR_OPB_SYSACE_0_BASEADDR
#define XPAR_SYSACE_0_HIGHADDR XPAR_OPB_SYSACE_0_HIGHADDR
#define XPAR_SYSACE_0_DEVICE_ID XPAR_OPB_SYSACE_0_DEVICE_ID

/******************************************************************/

#define XPAR_IIC_0_BASEADDR XPAR_OPB_IIC_0_BASEADDR
#define XPAR_IIC_0_HIGHADDR XPAR_OPB_IIC_0_HIGHADDR
#define XPAR_IIC_0_TEN_BIT_ADR XPAR_OPB_IIC_0_TEN_BIT_ADR
#define XPAR_IIC_0_DEVICE_ID XPAR_OPB_IIC_0_DEVICE_ID

/******************************************************************/

#define XPAR_PLB_CLOCK_FREQ_HZ 100000000
#define XPAR_CORE_CLOCK_FREQ_HZ XPAR_CPU_PPC405_CORE_CLOCK_FREQ_HZ
#define XPAR_DDR_0_SIZE 0x4000000

/******************************************************************/

#define XPAR_PERSISTENT_0_IIC_0_BASEADDR 0x00000400
#define XPAR_PERSISTENT_0_IIC_0_HIGHADDR 0x000007FF
#define XPAR_PERSISTENT_0_IIC_0_EEPROMADDR 0xA0

/******************************************************************/

#define XPAR_PCI_0_CLOCK_FREQ_HZ    0

/******************************************************************/

