/*
 * Copyright (C) 2009, Texas Instruments, Incorporated
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <common.h>
#include <asm/cache.h>
#include <asm/arch/clock.h>
#include <asm/arch/cpu.h>
#include <asm/arch/ddr_defs.h>
#include <asm/arch/hardware.h>
#include <asm/arch/sys_proto.h>

#define __raw_readl(a)		(*(volatile unsigned int *)(a))
#define __raw_writel(v, a)	(*(volatile unsigned int *)(a) = (v))
#define __raw_readw(a)		(*(volatile unsigned short *)(a))
#define __raw_writew(v, a)	(*(volatile unsigned short *)(a) = (v))
#define __raw_readc(a)		(*(volatile char *)(a))
#define __raw_writec(v, a)	(*(volatile char *)(a) = (v))

#define WR_MEM_32(a, d) (*(volatile int*)(a) = (d))
#define RD_MEM_32(a) (*(volatile int*)(a))
#define WR_MEM_8(a, d) (*(volatile char*)(a) = (d))
#define RD_MEM_8(a) (*(volatile char*)(a))

DECLARE_GLOBAL_DATA_PTR;

void walking_one_test(unsigned long start_addr, unsigned long end_addr);
void data_walking_test(unsigned long addr, unsigned long mask);
void address_walking_test(unsigned long addr, unsigned long mask);

/*******************************************************
 * Routine: delay
 * Description: spinning delay to use before udelay works
 ******************************************************/
static inline void delay(unsigned long loops)
{
	__asm__ volatile ("1:\n" "subs %0, %1, #1\n"
			  "bne 1b" : "=r" (loops) : "0"(loops));
}

/*
 * Basic board specific setup
 */
int board_init(void)
{
	u32 regVal;

	/* Get Timer and UART out of reset */

	/* UART softreset */
	regVal = __raw_readl(UART_SYSCFG);
	regVal |= 0x2;
	__raw_writel(regVal, UART_SYSCFG);
	while( (__raw_readl(UART_SYSSTS) & 0x1) != 0x1);

	/* Disable smart idle */
	regVal = __raw_readl(UART_SYSCFG);
	regVal |= (1<<3);
	__raw_writel(regVal, UART_SYSCFG);

	gd->bd->bi_arch_number = MACH_TYPE_TI8168EVM;

	/* address of boot parameters */
	gd->bd->bi_boot_params = PHYS_DRAM_1 + 0x100;

	return 0;
}

/*
 * Configure DRAM banks
 *
 * Description: sets uboots idea of sdram size
 */
int dram_init(void)
{
	/* Fill up board info */
	gd->bd->bi_dram[0].start = PHYS_DRAM_1;
	gd->bd->bi_dram[0].size = PHYS_DRAM_1_SIZE;

	gd->bd->bi_dram[1].start = PHYS_DRAM_2;
	gd->bd->bi_dram[1].size = PHYS_DRAM_2_SIZE;

	return 0;
}


/*************************************************************
 *  get_device_type(): tell if GP/HS/EMU/TST
 *************************************************************/
u32 get_device_type(void)
{
	int mode;
	mode = __raw_readl(CONTROL_STATUS) & (DEVICE_MASK);
	return(mode >>= 8);
}

/************************************************
 * get_sysboot_value(void) - return SYS_BOOT[4:0]
 ************************************************/
u32 get_sysboot_value(void)
{
	int mode;
	mode = __raw_readl(CONTROL_STATUS) & (SYSBOOT_MASK);
	return mode;
}

/* assume delay is aprox at least 1us */
void ddr_delay(int d)
{  /*
   * in interactive mode use a user response
   * printf("Pause... Press enter to continue\n");
   * getchar();
   */
  int i;

  /*
   * read a control module register.
   * this is a bit more delay and cannot be optimized by the compiler
   * assuming one read takes 200 cycles and A8 is runing 1 GHz
   * somewhat conservative setting
   */
  for(i=0; i<50*d; i++)
    __raw_readl(CONTROL_STATUS);
}

#ifdef CONFIG_TI816X_EVM_DDR3
/*********************************************************************
 * Init DDR3 on TI816X EVM
 *********************************************************************/
static void ddr_init_settings(int emif)
{
	/*
	 * DLL Lockdiff DLL_Lockdiff determines effectively is the
	 * threshold internal to the DLL to indicate that the DLL has
	 * lost lock.  When this happens the PHY currently issues an
	 * internal reset.  The reset value for this is 0x4, which is
	 * insufficient.  Set this to 15 (maximum possible - to
	 * prevent this reset.  If the reset happens it would cause
	 * the data to be corrupted.
	 */
	__raw_writel(0xF, DDRPHY_CONFIG_BASE + 0x028);
	__raw_writel(0xF, DDRPHY_CONFIG_BASE + 0x05C);
	__raw_writel(0xF, DDRPHY_CONFIG_BASE + 0x090);
	__raw_writel(0xF, DDRPHY_CONFIG_BASE + 0x138);
	__raw_writel(0xF, DDRPHY_CONFIG_BASE + 0x1DC);
	__raw_writel(0xF, DDRPHY_CONFIG_BASE + 0x280);
	__raw_writel(0xF, DDRPHY_CONFIG_BASE + 0x324);

	/*
	 * setup use_rank_delays to 1.  This is only necessary when
	 * multiple ranks are in use.  Though the EVM does not have
	 * multiple ranks, this is a good value to set.
	 */
	__raw_writel(1, DDRPHY_CONFIG_BASE + 0x134);
	__raw_writel(1, DDRPHY_CONFIG_BASE + 0x1d8);
	__raw_writel(1, DDRPHY_CONFIG_BASE + 0x27c);
	__raw_writel(1, DDRPHY_CONFIG_BASE + 0x320);

	/* see ddr_defs.h for invert clock setting and details */
	__raw_writel(INVERT_CLOCK, DDRPHY_CONFIG_BASE + 0x02C); /* invert_clk_out cmd0 */
	__raw_writel(INVERT_CLOCK, DDRPHY_CONFIG_BASE + 0x060); /* invert_clk_out cmd0 */
	__raw_writel(INVERT_CLOCK, DDRPHY_CONFIG_BASE + 0x094); /* invert_clk_out cmd0 */

	/* with inv clkout: 0x100. no inv clkout: 0x80.  See ddr_defs.h */
	__raw_writel(CMD_SLAVE_RATIO, DDRPHY_CONFIG_BASE + 0x01C); /* cmd0 slave ratio */
	__raw_writel(CMD_SLAVE_RATIO, DDRPHY_CONFIG_BASE + 0x050); /* cmd1 slave ratio */
	__raw_writel(CMD_SLAVE_RATIO, DDRPHY_CONFIG_BASE + 0x084); /* cmd2 slave ratio */

	/* for ddr3 this needs to be set to 1 */
	__raw_writel(0x1, DDRPHY_CONFIG_BASE + 0x0F8); /* init mode */
	__raw_writel(0x1, DDRPHY_CONFIG_BASE + 0x104);
	__raw_writel(0x1, DDRPHY_CONFIG_BASE + 0x19C);
	__raw_writel(0x1, DDRPHY_CONFIG_BASE + 0x1A8);
	__raw_writel(0x1, DDRPHY_CONFIG_BASE + 0x240);
	__raw_writel(0x1, DDRPHY_CONFIG_BASE + 0x24C);
	__raw_writel(0x1, DDRPHY_CONFIG_BASE + 0x2E4);
	__raw_writel(0x1, DDRPHY_CONFIG_BASE + 0x2F0);

	/****  setup the initial levelinihg ratios ****/
	/* these are derived from board delays and may be different for different boards
	 * see ddr_defs.h
	 * we are setting the values here for both the ranks, though only one is in use
	 */
	__raw_writel((WR_DQS_RATIO_3 << 10) | WR_DQS_RATIO_3, DDRPHY_CONFIG_BASE + 0x0F0); /*  data0 writelvl init ratio */
	__raw_writel(0x00000, DDRPHY_CONFIG_BASE + 0x0F4);   /*   */
	__raw_writel((WR_DQS_RATIO_2 << 10) | WR_DQS_RATIO_2, DDRPHY_CONFIG_BASE + 0x194); /*  data1 writelvl init ratio */
	__raw_writel(0x00000, DDRPHY_CONFIG_BASE + 0x198);   /*   */
	__raw_writel((WR_DQS_RATIO_1 << 10) | WR_DQS_RATIO_1, DDRPHY_CONFIG_BASE + 0x238); /*  data2 writelvl init ratio */
	__raw_writel(0x00000, DDRPHY_CONFIG_BASE + 0x23c);   /*   */
	__raw_writel((WR_DQS_RATIO_0 << 10) | WR_DQS_RATIO_0, DDRPHY_CONFIG_BASE + 0x2dc); /*  data3 writelvl init ratio */
	__raw_writel(0x00000, DDRPHY_CONFIG_BASE + 0x2e0);   /*   */


	__raw_writel((RD_GATE_RATIO_3 << 10) | RD_GATE_RATIO_3, DDRPHY_CONFIG_BASE + 0x0FC); /*  data0 gatelvl init ratio */
	__raw_writel(0x0, DDRPHY_CONFIG_BASE + 0x100);
	__raw_writel((RD_GATE_RATIO_2 << 10) | RD_GATE_RATIO_2, DDRPHY_CONFIG_BASE + 0x1A0); /*  data1 gatelvl init ratio */
	__raw_writel(0x0, DDRPHY_CONFIG_BASE + 0x1A4);
	__raw_writel((RD_GATE_RATIO_1 << 10) | RD_GATE_RATIO_1, DDRPHY_CONFIG_BASE + 0x244); /*  data2 gatelvl init ratio */
	__raw_writel(0x0, DDRPHY_CONFIG_BASE + 0x248);
	__raw_writel((RD_GATE_RATIO_0 << 10) | RD_GATE_RATIO_0, DDRPHY_CONFIG_BASE + 0x2E8); /*  data3 gatelvl init ratio */
	__raw_writel(0x0, DDRPHY_CONFIG_BASE + 0x2EC);

	if(HACK_EYE_TRAINING){
		__raw_writel((RD_DQS_FORCE_3 << 9) | RD_DQS_FORCE_3, DDRPHY_CONFIG_BASE + 0x0D4);
		__raw_writel(0x00000001, DDRPHY_CONFIG_BASE + 0x0D0);

		__raw_writel((RD_DQS_FORCE_2 << 9) | RD_DQS_FORCE_2, DDRPHY_CONFIG_BASE + 0x178);
		__raw_writel(0x00000001, DDRPHY_CONFIG_BASE + 0x174);

		__raw_writel((RD_DQS_FORCE_1 << 9) | RD_DQS_FORCE_1, DDRPHY_CONFIG_BASE + 0x21C);
		__raw_writel(0x00000001, DDRPHY_CONFIG_BASE + 0x218);

		/* rd dqs - lane 0 */
		__raw_writel((RD_DQS_FORCE_0 << 9) | RD_DQS_FORCE_0, DDRPHY_CONFIG_BASE + 0x2C0);
		__raw_writel(0x00000001, DDRPHY_CONFIG_BASE + 0x2BC);
	}
	/* DDR3 */

	__raw_writel(0x5, DDRPHY_CONFIG_BASE + 0x00C);     /* cmd0 io config - output impedance of pad */
	__raw_writel(0x5, DDRPHY_CONFIG_BASE + 0x010);     /* cmd0 io clk config - output impedance of pad */
	__raw_writel(0x5, DDRPHY_CONFIG_BASE + 0x040);     /* cmd1 io config - output impedance of pad */
	__raw_writel(0x5, DDRPHY_CONFIG_BASE + 0x044);     /* cmd1 io clk config - output impedance of pad */
	__raw_writel(0x5, DDRPHY_CONFIG_BASE + 0x074);     /* cmd2 io config - output impedance of pad */
	__raw_writel(0x5, DDRPHY_CONFIG_BASE + 0x078);     /* cmd2 io clk config - output impedance of pad */
	__raw_writel(0x4, DDRPHY_CONFIG_BASE + 0x0A8);     /* data0 io config - output impedance of pad */
	__raw_writel(0x4, DDRPHY_CONFIG_BASE + 0x0AC);     /* data0 io clk config - output impedance of pad */
	__raw_writel(0x4, DDRPHY_CONFIG_BASE + 0x14C);     /* data1 io config - output impedance of pa     */
	__raw_writel(0x4, DDRPHY_CONFIG_BASE + 0x150);     /* data1 io clk config - output impedance of pad */
	__raw_writel(0x4, DDRPHY_CONFIG_BASE + 0x1F0);     /* data2 io config - output impedance of pa */
	__raw_writel(0x4, DDRPHY_CONFIG_BASE + 0x1F4);     /* data2 io clk config - output impedance of pad */
	__raw_writel(0x4, DDRPHY_CONFIG_BASE + 0x294);     /* data3 io config - output impedance of pa */
	__raw_writel(0x4, DDRPHY_CONFIG_BASE + 0x298);     /* data3 io clk config - output impedance of pad */

	__raw_writel(0x5, DDRPHY_CONFIG_BASE + 0x338);     /* fifo_we_out0  - output impedance of pad */
	__raw_writel(0x5, DDRPHY_CONFIG_BASE + 0x340);     /* fifo_we_out1 - output impedance of pad */
	__raw_writel(0x5, DDRPHY_CONFIG_BASE + 0x348);     /* fifo_we_in2 - output impedance of pad */
	__raw_writel(0x5, DDRPHY_CONFIG_BASE + 0x350);     /* fifo_we_in3 - output impedance of pad */

}

static void emif4p_init(u32 TIM1, u32 TIM2, u32 TIM3, u32 SDREF, u32 SDCFG, u32 RL)
{
	if(USE_EMIF0){
		/*Program EMIF0 CFG Registers*/
		__raw_writel(TIM1, EMIF4_0_SDRAM_TIM_1);
		__raw_writel(TIM1, EMIF4_0_SDRAM_TIM_1_SHADOW);
		__raw_writel(TIM2, EMIF4_0_SDRAM_TIM_2);
		__raw_writel(TIM2, EMIF4_0_SDRAM_TIM_2_SHADOW);
		__raw_writel(TIM3, EMIF4_0_SDRAM_TIM_3);
		__raw_writel(TIM3, EMIF4_0_SDRAM_TIM_3_SHADOW);
		__raw_writel(SDCFG, EMIF4_0_SDRAM_CONFIG);
		//__raw_writel(SDREF, EMIF4_0_SDRAM_REF_CTRL);
		//__raw_writel(SDREF, EMIF4_0_SDRAM_REF_CTRL_SHADOW);
		__raw_writel(RL, EMIF4_0_DDR_PHY_CTRL_1);
		__raw_writel(RL, EMIF4_0_DDR_PHY_CTRL_1_SHADOW);
		__raw_writel(0x0000613B, EMIF4_0_SDRAM_REF_CTRL);   /* initially a large refresh period */
		__raw_writel(0x1000613B, EMIF4_0_SDRAM_REF_CTRL);   /* trigger initialization           */
	}

	if(USE_EMIF1){
	/*Program EMIF1 CFG Registers*/
		__raw_writel(TIM1, EMIF4_1_SDRAM_TIM_1);
		__raw_writel(TIM1, EMIF4_1_SDRAM_TIM_1_SHADOW);
		__raw_writel(TIM2, EMIF4_1_SDRAM_TIM_2);
		__raw_writel(TIM2, EMIF4_1_SDRAM_TIM_2_SHADOW);
		__raw_writel(TIM3, EMIF4_1_SDRAM_TIM_3);
		__raw_writel(TIM3, EMIF4_1_SDRAM_TIM_3_SHADOW);
		__raw_writel(SDCFG, EMIF4_1_SDRAM_CONFIG);
		//__raw_writel(SDREF, EMIF4_1_SDRAM_REF_CTRL);
		//__raw_writel(SDREF, EMIF4_1_SDRAM_REF_CTRL_SHADOW);
		__raw_writel(RL, EMIF4_1_DDR_PHY_CTRL_1);
		__raw_writel(RL, EMIF4_1_DDR_PHY_CTRL_1_SHADOW);
		__raw_writel(0x0000613B, EMIF4_0_SDRAM_REF_CTRL);   /* initially a large refresh period */
		__raw_writel(0x1000613B, EMIF4_0_SDRAM_REF_CTRL);   /* trigger initialization           */
	}

}

static void config_ti816x_sdram_ddr(void)
{
	__raw_writel(0x2, CM_DEFAULT_FW_CLKCTRL);				/*Enable the EMIF Firewall clocks */
	__raw_writel(0x2, CM_DEFAULT_L3_FAST_CLKSTCTRL);			/*Enable the Power Domain Transition of L3 Fast Domain Peripheral*/
	__raw_writel(0x2, CM_DEFAULT_EMIF_0_CLKCTRL);				/*Enable EMIF0 Clock*/
	__raw_writel(0x2, CM_DEFAULT_EMIF_1_CLKCTRL); 				/*Enable EMIF1 Clock*/
	while((__raw_readl(CM_DEFAULT_L3_FAST_CLKSTCTRL) & 0x300) != 0x300);	/*Poll for L3_FAST_GCLK  & DDR_GCLK  are active*/
	while((__raw_readl(CM_DEFAULT_EMIF_0_CLKCTRL)) != 0x2);	/*Poll for Module is functional*/
	while((__raw_readl(CM_DEFAULT_EMIF_1_CLKCTRL)) != 0x2);	/*Poll for Module is functional*/

	if (USE_EMIF0) {
		ddr_init_settings(0);
	}

	if (USE_EMIF1) {
		ddr_init_settings(1);
	}


	__raw_writel(0x2, CM_DEFAULT_DMM_CLKCTRL); 			/*Enable EMIF1 Clock*/
	while((__raw_readl(CM_DEFAULT_DMM_CLKCTRL)) != 0x2);		/*Poll for Module is functional*/

	/*Program the DMM to Access EMIF0*/
	__raw_writel(0x80400100, DMM_LISA_MAP__0);
	__raw_writel(0xC0400110, DMM_LISA_MAP__1);

	/*Program the DMM to Access EMIF1*/
	__raw_writel(0x90400200, DMM_LISA_MAP__2);
	__raw_writel(0xB0400210, DMM_LISA_MAP__3);
	/*Enable Tiled Access*/
	__raw_writel(0x80000000, DMM_PAT_BASE_ADDR);

	emif4p_init(EMIF_TIM1, EMIF_TIM2, EMIF_TIM3, EMIF_SDREF & 0xFFFFFFFF, EMIF_SDCFG, EMIF_PHYCFG);

	if(HACK_EYE_TRAINING) {
		ddr_delay(10000);

	if(USE_EMIF0){
		__raw_writel(((RD_DQS_FORCE_2 + 50) << 9) | (RD_DQS_FORCE_2 + 50), DDRPHY_0_CONFIG_BASE + 0x178);
		__raw_writel(((RD_DQS_FORCE_0 + 50) << 9) | (RD_DQS_FORCE_0 + 50), DDRPHY_0_CONFIG_BASE + 0x21C);
	}

	if(USE_EMIF1){
		__raw_writel(((RD_DQS_FORCE_2 + 50) << 9) | (RD_DQS_FORCE_2 + 50), DDRPHY_1_CONFIG_BASE + 0x178);
		__raw_writel(((RD_DQS_FORCE_0 + 50) << 9) | (RD_DQS_FORCE_0 + 50), DDRPHY_1_CONFIG_BASE + 0x21C);
	}

		ddr_delay(1000); /* wait for 1 ms */

	if(USE_EMIF0) {
		__raw_writel(((RD_DQS_FORCE_3 + 50) << 9) | (RD_DQS_FORCE_3 + 50), DDRPHY_0_CONFIG_BASE + 0x0D4);
		__raw_writel(((RD_DQS_FORCE_1 + 50) << 9) | (RD_DQS_FORCE_1 + 50), DDRPHY_0_CONFIG_BASE + 0x2C0);
	}

	if(USE_EMIF1){
		__raw_writel(((RD_DQS_FORCE_3 + 50) << 9) | (RD_DQS_FORCE_3 + 50), DDRPHY_1_CONFIG_BASE + 0x0D4);
		__raw_writel(((RD_DQS_FORCE_1 + 50) << 9) | (RD_DQS_FORCE_1 + 50), DDRPHY_1_CONFIG_BASE + 0x2C0);
	}

		ddr_delay(1000); /* wait for 1 ms */

	if(USE_EMIF0) {
		__raw_writel(((RD_DQS_FORCE_3 << 9) | RD_DQS_FORCE_3),DDRPHY_0_CONFIG_BASE + 0x0D4);
		__raw_writel(((RD_DQS_FORCE_0 << 9) | RD_DQS_FORCE_0),DDRPHY_0_CONFIG_BASE + 0x2C0);
		__raw_writel(((RD_DQS_FORCE_2 << 9) | RD_DQS_FORCE_2),DDRPHY_0_CONFIG_BASE + 0x178);
		__raw_writel(((RD_DQS_FORCE_1 << 9) | RD_DQS_FORCE_1),DDRPHY_0_CONFIG_BASE + 0x21C);
	}

	if(USE_EMIF1) {
		__raw_writel(((RD_DQS_FORCE_3 << 9) | RD_DQS_FORCE_3),DDRPHY_1_CONFIG_BASE + 0x0D4);
		__raw_writel(((RD_DQS_FORCE_0 << 9) | RD_DQS_FORCE_0),DDRPHY_1_CONFIG_BASE + 0x2C0);
		__raw_writel(((RD_DQS_FORCE_2 << 9) | RD_DQS_FORCE_2),DDRPHY_1_CONFIG_BASE + 0x178);
		__raw_writel(((RD_DQS_FORCE_1 << 9) | RD_DQS_FORCE_1),DDRPHY_1_CONFIG_BASE + 0x21C);
	}

	} /* hack eye training */

	ddr_delay(1000); /* wait for 1 ms */
	/* finish off */

	if(USE_EMIF0){
		__raw_writel(0x10000C30, EMIF4_0_SDRAM_REF_CTRL);
		__raw_readl(EMIF4_0_SDRAM_REF_CTRL);
	}

	if(USE_EMIF1){
		__raw_writel(0x10000C30, EMIF4_1_SDRAM_REF_CTRL);
		__raw_readl(EMIF4_1_SDRAM_REF_CTRL);
	}

	walking_one_test(0x80000000, 0x9fffffff);
}
#endif


#ifdef CONFIG_TI816X_EVM_DDR2
static void ddr_init_settings(int emif)
{
	/* DLL Lockdiff */
	__raw_writel(0xF, (DDRPHY_CONFIG_BASE + 0x028));
	__raw_writel(0xF, (DDRPHY_CONFIG_BASE + 0x05C));
	__raw_writel(0xF, (DDRPHY_CONFIG_BASE + 0x090));
	__raw_writel(0xF, (DDRPHY_CONFIG_BASE + 0x138));
	__raw_writel(0xF, (DDRPHY_CONFIG_BASE + 0x1DC));
	__raw_writel(0xF, (DDRPHY_CONFIG_BASE + 0x280));
	__raw_writel(0xF, (DDRPHY_CONFIG_BASE + 0x324));

	/* setup rank delays */
	__raw_writel(0x1, (DDRPHY_CONFIG_BASE + 0x134));
	__raw_writel(0x1, (DDRPHY_CONFIG_BASE + 0x1D8));
	__raw_writel(0x1, (DDRPHY_CONFIG_BASE + 0x27C));
	__raw_writel(0x1, (DDRPHY_CONFIG_BASE + 0x320));


	__raw_writel(INVERT_CLK_OUT, (DDRPHY_CONFIG_BASE + 0x02C));	/* invert_clk_out cmd0 */
	__raw_writel(INVERT_CLK_OUT, (DDRPHY_CONFIG_BASE + 0x060));	/* invert_clk_out cmd0 */
	__raw_writel(INVERT_CLK_OUT, (DDRPHY_CONFIG_BASE + 0x094));	/* invert_clk_out cmd0 */


	__raw_writel(CMD_SLAVE_RATIO, (DDRPHY_CONFIG_BASE + 0x01C));	/* cmd0 slave ratio */
	__raw_writel(CMD_SLAVE_RATIO, (DDRPHY_CONFIG_BASE + 0x050));	/* cmd0 slave ratio */
	__raw_writel(CMD_SLAVE_RATIO, (DDRPHY_CONFIG_BASE + 0x084));	/* cmd0 slave ratio */

	__raw_writel(DQS_GATE_BYTE_LANE0, (DDRPHY_CONFIG_BASE + 0x108));
	__raw_writel(0x00000000, (DDRPHY_CONFIG_BASE + 0x10C));
	__raw_writel(DQS_GATE_BYTE_LANE1, (DDRPHY_CONFIG_BASE + 0x1AC));
	__raw_writel(0x00000000, (DDRPHY_CONFIG_BASE + 0x1B0));
	__raw_writel(DQS_GATE_BYTE_LANE2, (DDRPHY_CONFIG_BASE + 0x250));
	__raw_writel(0x00000000, (DDRPHY_CONFIG_BASE + 0x254));
	__raw_writel(DQS_GATE_BYTE_LANE3, (DDRPHY_CONFIG_BASE + 0x2F4));
	__raw_writel(0x00000000, (DDRPHY_CONFIG_BASE + 0x2F8));

	__raw_writel(WR_DQS_RATIO_BYTE_LANE0, (DDRPHY_CONFIG_BASE + 0x0DC));
	__raw_writel(0x0, (DDRPHY_CONFIG_BASE + 0x0E0));
	__raw_writel(WR_DQS_RATIO_BYTE_LANE1, (DDRPHY_CONFIG_BASE + 0x180));
	__raw_writel(0x0, (DDRPHY_CONFIG_BASE + 0x184));
	__raw_writel(WR_DQS_RATIO_BYTE_LANE2, (DDRPHY_CONFIG_BASE + 0x224));
	__raw_writel(0x0, (DDRPHY_CONFIG_BASE + 0x228));
	__raw_writel(WR_DQS_RATIO_BYTE_LANE3, (DDRPHY_CONFIG_BASE + 0x2C8));
	__raw_writel(0x0, (DDRPHY_CONFIG_BASE + 0x2CC));

	__raw_writel(WR_DATA_RATIO_BYTE_LANE0, (DDRPHY_CONFIG_BASE + 0x120));
	__raw_writel(0x0, (DDRPHY_CONFIG_BASE + 0x124));
	__raw_writel(WR_DATA_RATIO_BYTE_LANE1, (DDRPHY_CONFIG_BASE + 0x1C4));
	__raw_writel(0x0, (DDRPHY_CONFIG_BASE + 0x1C8));
	__raw_writel(WR_DATA_RATIO_BYTE_LANE2, (DDRPHY_CONFIG_BASE + 0x268));
	__raw_writel(0x0, (DDRPHY_CONFIG_BASE + 0x26C));
	__raw_writel(WR_DATA_RATIO_BYTE_LANE3, (DDRPHY_CONFIG_BASE + 0x30C));
	__raw_writel(0x0, (DDRPHY_CONFIG_BASE + 0x310));

	__raw_writel(RD_DQS_RATIO, (DDRPHY_CONFIG_BASE + 0x0C8));
	__raw_writel(0x0, (DDRPHY_CONFIG_BASE + 0x0CC));
	__raw_writel(RD_DQS_RATIO, (DDRPHY_CONFIG_BASE + 0x16C));
	__raw_writel(0x0, (DDRPHY_CONFIG_BASE + 0x170));
	__raw_writel(RD_DQS_RATIO, (DDRPHY_CONFIG_BASE + 0x210));
	__raw_writel(0x0, (DDRPHY_CONFIG_BASE + 0x214));
	__raw_writel(RD_DQS_RATIO, (DDRPHY_CONFIG_BASE + 0x2B4));
	__raw_writel(0x0, (DDRPHY_CONFIG_BASE + 0x2B8));

	__raw_writel(0x5, (DDRPHY_CONFIG_BASE + 0x00C));
	__raw_writel(0x5, (DDRPHY_CONFIG_BASE + 0x010));
	__raw_writel(0x5, (DDRPHY_CONFIG_BASE + 0x040));
	__raw_writel(0x5, (DDRPHY_CONFIG_BASE + 0x044));
	__raw_writel(0x5, (DDRPHY_CONFIG_BASE + 0x074));
	__raw_writel(0x5, (DDRPHY_CONFIG_BASE + 0x078));

	__raw_writel(0x4, (DDRPHY_CONFIG_BASE + 0x0A8));
	__raw_writel(0x4, (DDRPHY_CONFIG_BASE + 0x0AC));
	__raw_writel(0x4, (DDRPHY_CONFIG_BASE + 0x14C));
	__raw_writel(0x4, (DDRPHY_CONFIG_BASE + 0x150));
	__raw_writel(0x4, (DDRPHY_CONFIG_BASE + 0x1F0));
	__raw_writel(0x4, (DDRPHY_CONFIG_BASE + 0x1F4));
	__raw_writel(0x4, (DDRPHY_CONFIG_BASE + 0x294));
	__raw_writel(0x4, (DDRPHY_CONFIG_BASE + 0x298));

	__raw_writel(0x5, (DDRPHY_CONFIG_BASE + 0x338));
	__raw_writel(0x5, (DDRPHY_CONFIG_BASE + 0x340));
	__raw_writel(0x5, (DDRPHY_CONFIG_BASE + 0x348));
	__raw_writel(0x5, (DDRPHY_CONFIG_BASE + 0x350));

}

static void emif4p_init(u32 TIM1, u32 TIM2, u32 TIM3, u32 SDREF, u32 SDCFG, u32 RL)
{
	/*Program EMIF0 CFG Registers*/
	__raw_writel(TIM1, EMIF4_0_SDRAM_TIM_1);
	__raw_writel(TIM1, EMIF4_0_SDRAM_TIM_1_SHADOW);
	__raw_writel(TIM2, EMIF4_0_SDRAM_TIM_2);
	__raw_writel(TIM2, EMIF4_0_SDRAM_TIM_2_SHADOW);
	__raw_writel(TIM3, EMIF4_0_SDRAM_TIM_3);
	__raw_writel(TIM3, EMIF4_0_SDRAM_TIM_3_SHADOW);
	__raw_writel(SDCFG, EMIF4_0_SDRAM_CONFIG);
	//__raw_writel(SDREF, EMIF4_0_SDRAM_REF_CTRL);
	//__raw_writel(SDREF, EMIF4_0_SDRAM_REF_CTRL_SHADOW);
	__raw_writel(RL, EMIF4_0_DDR_PHY_CTRL_1);
	__raw_writel(RL, EMIF4_0_DDR_PHY_CTRL_1_SHADOW);

	if (CONFIG_TI816X_TWO_EMIF){
	__raw_writel(TIM1, EMIF4_1_SDRAM_TIM_1);
	__raw_writel(TIM1, EMIF4_1_SDRAM_TIM_1_SHADOW);
	__raw_writel(TIM2, EMIF4_1_SDRAM_TIM_2);
	__raw_writel(TIM2, EMIF4_1_SDRAM_TIM_2_SHADOW);
	__raw_writel(TIM3, EMIF4_1_SDRAM_TIM_3);
	__raw_writel(TIM3, EMIF4_1_SDRAM_TIM_3_SHADOW);
	__raw_writel(SDCFG, EMIF4_1_SDRAM_CONFIG);
	//__raw_writel(SDREF, EMIF4_0_SDRAM_REF_CTRL);
	//__raw_writel(SDREF, EMIF4_0_SDRAM_REF_CTRL_SHADOW);
	__raw_writel(RL, EMIF4_1_DDR_PHY_CTRL_1);
	__raw_writel(RL, EMIF4_1_DDR_PHY_CTRL_1_SHADOW);
	}

}

static void ddrsetup(void)
{
	/* setup a small control period */
	__raw_writel(0x0000613B, EMIF4_0_SDRAM_REF_CTRL);
	__raw_writel(0x1000613B, EMIF4_0_SDRAM_REF_CTRL);
	__raw_writel(0x10000C30, EMIF4_0_SDRAM_REF_CTRL);

	__raw_writel(EMIF_PHYCFG, EMIF4_0_DDR_PHY_CTRL_1);
	__raw_writel(EMIF_PHYCFG, EMIF4_0_DDR_PHY_CTRL_1_SHADOW);

	if (CONFIG_TI816X_TWO_EMIF){
	/* setup a small control period */
	__raw_writel(0x0000613B, EMIF4_1_SDRAM_REF_CTRL);
	__raw_writel(0x1000613B, EMIF4_1_SDRAM_REF_CTRL);
	__raw_writel(0x10000C30, EMIF4_1_SDRAM_REF_CTRL);

	__raw_writel(EMIF_PHYCFG, EMIF4_1_DDR_PHY_CTRL_1);
	__raw_writel(EMIF_PHYCFG, EMIF4_1_DDR_PHY_CTRL_1_SHADOW);
	}

}

static void update_dqs(int emif)
{
}

static void config_ti816x_sdram_ddr(void)
{
	__raw_writel(0x2, CM_DEFAULT_L3_FAST_CLKSTCTRL);			/*Enable the Power Domain Transition of L3 Fast Domain Peripheral*/
	__raw_writel(0x2, CM_DEFAULT_EMIF_0_CLKCTRL);				/*Enable EMIF0 Clock*/
	__raw_writel(0x2, CM_DEFAULT_EMIF_1_CLKCTRL); 				/*Enable EMIF1 Clock*/
	while((__raw_readl(CM_DEFAULT_L3_FAST_CLKSTCTRL) & 0x300) != 0x300);	/*Poll for L3_FAST_GCLK  & DDR_GCLK  are active*/
	while((__raw_readl(CM_DEFAULT_EMIF_0_CLKCTRL)) != 0x2);	/*Poll for Module is functional*/
	while((__raw_readl(CM_DEFAULT_EMIF_1_CLKCTRL)) != 0x2);	/*Poll for Module is functional*/

	ddr_init_settings(0);

	if (CONFIG_TI816X_TWO_EMIF){
		ddr_init_settings(1);
	}

	__raw_writel(0x2, CM_DEFAULT_DMM_CLKCTRL); 				/*Enable EMIF1 Clock*/
	while((__raw_readl(CM_DEFAULT_DMM_CLKCTRL)) != 0x2);		/*Poll for Module is functional*/

	/*Program the DMM to Access EMIF0*/
	__raw_writel(0x80640300, DMM_LISA_MAP__0);
	__raw_writel(0xC0640320, DMM_LISA_MAP__1);

	/*Program the DMM to Access EMIF1*/
	__raw_writel(0x80640300, DMM_LISA_MAP__2);
	__raw_writel(0xC0640320, DMM_LISA_MAP__3);

	/*Enable Tiled Access*/
	__raw_writel(0x80000000, DMM_PAT_BASE_ADDR);

	emif4p_init(EMIF_TIM1, EMIF_TIM2, EMIF_TIM3, EMIF_SDREF & 0xFFFFFFFF, EMIF_SDCFG, 0x10B);
	ddrsetup();
	update_dqs(0);
	update_dqs(1);
}
#endif

/*
 * TI816X specific functions
 */
static void main_pll_init_ti816x(u32 sil_index, u32 clk_index)
{
	u32 main_pll_ctrl=0;

	/* Sequence to be followed:
	 * 1. Put the PLL in bypass mode by setting BIT2 in its ctrl reg
	 * 2. Write the values of N,P in the CTRL reg
	 * 3. Program the freq values, divider values for the required output in the Control module reg
	 * 4. Note: Loading the freq value requires a particular bit to be set in the freq reg.
	 * 4. Program the CM divider value in the CM module reg
	 * 5. Enable the PLL by setting the appropriate bit in the CTRL reg of the PLL
	 */

	 /* If the registers have been set by the ROM code dont do anything
	  */

	 main_pll_ctrl = __raw_readl(MAINPLL_CTRL);
	 main_pll_ctrl &= 0xFFFFFFFB;
	 main_pll_ctrl |= 4;
	 __raw_writel(main_pll_ctrl, MAINPLL_CTRL);

	 main_pll_ctrl = __raw_readl(MAINPLL_CTRL);
	 main_pll_ctrl &= 0xFFFFFFF7;
	 main_pll_ctrl |= 8;
	 __raw_writel(main_pll_ctrl, MAINPLL_CTRL);

	 main_pll_ctrl = __raw_readl(MAINPLL_CTRL);
	 main_pll_ctrl &= 0xFF;
	 main_pll_ctrl |= (MAIN_N<<16 | MAIN_P<<8);
	 __raw_writel(main_pll_ctrl, MAINPLL_CTRL);

	 __raw_writel(0x0, MAINPLL_PWD);

	 __raw_writel((1<<31 | 1<<28 | (MAIN_INTFREQ1<<24) | MAIN_FRACFREQ1), MAINPLL_FREQ1);
	 __raw_writel(((1<<8) | MAIN_MDIV1), MAINPLL_DIV1);

	 __raw_writel((1<<31 | 1<<28 | (MAIN_INTFREQ2<<24) | MAIN_FRACFREQ2), MAINPLL_FREQ2);
	 __raw_writel(((1<<8) | MAIN_MDIV2), MAINPLL_DIV2);

	 __raw_writel((1<<31 | 1<<28 | (MAIN_INTFREQ3<<24) | MAIN_FRACFREQ3), MAINPLL_FREQ3);
	 __raw_writel(((1<<8) | MAIN_MDIV3), MAINPLL_DIV3);

	 __raw_writel((1<<31 | 1<<28 | (MAIN_INTFREQ4<<24) | MAIN_FRACFREQ4), MAINPLL_FREQ4);
	 __raw_writel(((1<<8) | MAIN_MDIV4), MAINPLL_DIV4);

	 __raw_writel((1<<31 | 1<<28 | (MAIN_INTFREQ5<<24) | MAIN_FRACFREQ5), MAINPLL_FREQ5);
	 __raw_writel(((1<<8) | MAIN_MDIV5), MAINPLL_DIV5);

	 __raw_writel((1<<8 | MAIN_MDIV6), MAINPLL_DIV6);

	 __raw_writel((1<<8 | MAIN_MDIV7), MAINPLL_DIV7);

	 while((__raw_readl(MAINPLL_CTRL) & 0x80) != 0x80);

	 main_pll_ctrl = __raw_readl(MAINPLL_CTRL);
	 main_pll_ctrl &= 0xFFFFFFFB;

	 __raw_writel(main_pll_ctrl, MAINPLL_CTRL);

}

static void ddr_pll_init_ti816x(u32 sil_index, u32 clk_index)
{
	u32 ddr_pll_ctrl=0;

	/* Sequence to be followed:
	 * 1. Put the PLL in bypass mode by setting BIT2 in its ctrl reg
	 * 2. Write the values of N,P in the CTRL reg
	 * 3. Program the freq values, divider values for the required output in the Control module reg
	 * 4. Note: Loading the freq value requires a particular bit to be set in the freq reg.
	 * 4. Program the CM divider value in the CM module reg
	 * 5. Enable the PLL by setting the appropriate bit in the CTRL reg of the PLL
	 */

	 /* If the registers have been set by the ROM code dont do anything
	  */

	ddr_pll_ctrl = __raw_readl(DDRPLL_CTRL);
	ddr_pll_ctrl &= 0xFFFFFFFB;
	__raw_writel(ddr_pll_ctrl, DDRPLL_CTRL);

	ddr_pll_ctrl = __raw_readl(DDRPLL_CTRL);
	ddr_pll_ctrl &= 0xFFFFFFF7;
	ddr_pll_ctrl |= 8;
	__raw_writel(ddr_pll_ctrl, DDRPLL_CTRL);

	ddr_pll_ctrl = __raw_readl(DDRPLL_CTRL);
	ddr_pll_ctrl &= 0xFF;
	ddr_pll_ctrl |= (DDR_N<<16 | DDR_P<<8);
	__raw_writel(ddr_pll_ctrl, DDRPLL_CTRL);

	/* 10usec delay */
	ddr_delay(10);

	__raw_writel(0x0,DDRPLL_PWD);

	__raw_writel(((0<<8) | DDR_MDIV1), DDRPLL_DIV1);
	ddr_delay(1);
	__raw_writel(((1<<8) | DDR_MDIV1), DDRPLL_DIV1);

	__raw_writel((1<<31 | 1<<28 | (DDR_INTFREQ2<<24) | DDR_FRACFREQ2), DDRPLL_FREQ2);
	__raw_writel(((1<<8) | DDR_MDIV2), DDRPLL_DIV2);

	__raw_writel(((0<<8) | DDR_MDIV3), DDRPLL_DIV3);
	ddr_delay(1);
	__raw_writel(((1<<8) | DDR_MDIV3), DDRPLL_DIV3);
	ddr_delay(1);
	__raw_writel((0<<31 | 1<<28 | (DDR_INTFREQ3<<24) | DDR_FRACFREQ3), DDRPLL_FREQ3);
	ddr_delay(1);
	__raw_writel((1<<31 | 1<<28 | (DDR_INTFREQ3<<24) | DDR_FRACFREQ3), DDRPLL_FREQ3);

	ddr_delay(5);

	/* Wait for PLL to lock */
	while((__raw_readl(DDRPLL_CTRL) & 0x80) != 0x80);

	ddr_pll_ctrl = __raw_readl(DDRPLL_CTRL);
	ddr_pll_ctrl &= 0xFFFFFFFB;
	ddr_pll_ctrl |= 4;
	__raw_writel(ddr_pll_ctrl, DDRPLL_CTRL);

	__raw_writel(0x1, DDR_RCD);

}

/*******************************************************
 * Routine: misc_init_r
 ********************************************************/
int misc_init_r (void)
{
	#ifdef CONFIG_TI816X_ASCIIART
	int i = 0, j = 0;
	char ti816x[23][79] = {
":,;;:;:;;;;;;;;r;;:,;;:;:;;;;;;;;:,;;:;:;;;;;;;;:,;;:;:;;;;;;;;:;;;;;;;;:,;;:;:",
";,;:::;;;;r;;;rssiSiS552X5252525259GX2X9hX9X9XX2325S55252i5:,;;:;:;;;;;;;;:,;;:",
";:;;;;;;;rrssSsS52S22h52299GGAAMHMM#BBH#B#HMM#HMBA&&XX2255S2S5Si:,;;:;:;;;;;;;;",
";:;;r;;rsrrriiXS5S329&A&MH#BMB#A&9XXA252GXiSXX39AAMMMBB&G22S5i2SSiiiisi:,;;:;:;",
";;;;;r;rr2iisiih393HB#B#AA99i22irrrX3X52AGsisss2Xii2299HBMA&X2S5S5iSiisSsi:,;;:",
"r:r;rrsrsihXSi2&##MHB&Ahh3AGHGA9G9h&#H##@@@##MAMMXXX9SSS29&&HGGX2i5iisiiisisi:,",
";;rrrrsSiiiA&ABH&A9GAGhAhBAMHA9HM@@@@@@@@@@@@@@@@@@@HHhAh2S2SX9&Gh22SSiisiiisii",
"r:rrssisiS2XM##&h3AGAX&3GG3Ssr5H@M#HM2; ;2X&&&MHMB###GBB#B&XXSSs529XX55iSsisisi",
"r;rsrisSi2XHAhX99A3XXG&&XS;:,rH#HGhAS   @@@@3rs2XBM@@A552&&AHA2XiisSS252SSsisSs",
"r;issi5S22&&3iSSX292&hXsr;;:;h@&G339&S9@@@@2@MA&9&HB##Xris29ABMAAX2ir;rsSi5iss5",
"rrsSi2XhG&9GXh399&X99i;;;;;;r#H&293H9X#@@@@@@@B&9GhAH@XrrsrsiXABHB&HG2rr;rrSiSi",
";:rsisS599&AA9XG&3A35r;:::;,;BMh&&2iX5A@@@@@@@&392X5GB2;;;r;iSX393A##A&Xi:::rsi",
";:rss552222X553&XHMhir;;::,:,h#HhGSXhG3#@@@@#AXXS2XAHA;::;;;;ss55XShBA3239r:,;;",
"r;ii2S5SSi2i53hirsh2srr::,,,,;MMXX359&Ah3h&Si59SX99A#i:,::::;;sri2,.2r;:SGAr;,:",
";:;rrrrssiriXGSi::shs;;;,,,:,,rBBA9h5s5h5iS5isi2SAHB5:,,,:::;rrs5&SrisSX5Srrr:,",
";,r;;;;rsriSSrrrr;;5Xrr;;,:,,.,;9AA2SsisS5323XXXG9&i:.,,::;;r;;;srrrrrr;;:;::::",
":,;r;r;rrissrrr;:;::;s;;;;,:,,..,r293h222hXXAAGGGX;:,,,:,:,::;:;::,:,,,,...,,,,",
";,;;;;rrrrrrrrirr;,.,,:::::::,,,,.,;SX&ABAB2hhXir:,,.,,.,,:,,,,..,,,..,..,,,..:",
":.:;:;;;:;;;;r;rrs;:.. ,,:::::,:,:,,.::rrsrr;;,,.......,..,....,,,,,,,...,.,,:,",
":.:::,::::::;;r;rrr;:.......,.,.,,:::,,...............,,::.,,,,:,::,,:,:,,,:,;:",
",.::,:,,,,,;;;;;;;;r;;::,..............................;;;:;::::,:::::::,:,:,,,",
": ,,:,,,,,,,,,,,,,:;rrr;;:;,,,,,,,::,.,.:.,.,;s,:;;;;:;:;;;;;::::,:::,:::,:,:,:",
",.,,,,,,,...,,.,,....................................:,............:,,,:,:,,,,,"};

	for (i = 0; i<23; i++)
	{
		for(j = 0; j<79; j++)
			printf("%c",ti816x[i][j]);
			printf("\n");
	}
	printf("\n");
	#endif
	return 0;
}

#ifdef CONFIG_TI816X_EVM_DDR3
void walking_one_test(unsigned long start_addr, unsigned long end_addr)
{
	data_walking_test(start_addr, 0xffffffff);
	address_walking_test(start_addr, (end_addr - start_addr));
}

void data_walking_test(unsigned long addr, unsigned long mask)
{
	unsigned bit = 1;
	int i;

	for(i=0; i<32; i++){
		if(mask & bit){
			__raw_writel(bit, addr);
			if(__raw_readl(addr) != bit){
			/* If this happens DDR3 init has failed */
			while (1);
			}
		}

		bit = bit << 1;
	}
}

void address_walking_test(unsigned long addr, unsigned long mask)
{
	unsigned bit = 1;
	int i;
	unsigned char write_value = 1;

	/* perform the writes */
	for(i=0; i<32; i++){
		if(mask & bit){
			__raw_writec(write_value, addr + bit);
		}

		bit = bit << 1;
		write_value++;
	}

	/* check that it was all good */
	write_value = 1;

	for(i=0; i<32; i++){
		if(mask & bit){
			if(__raw_readc(addr + bit) != write_value){
			/* If this happens DDR3 init has failed */
			while (1);
			}
		}

		bit = bit << 1;
		write_value++;
	}
}
#endif

/*****************************************************************
 * Routine: peripheral_enable
 * Description: Enable the clks & power for perifs (TIMER1, UART0,...)
 *
 ******************************************************************/
static void peripheral_enable(void)
{
	/* DMTimers */
	__raw_writel(0x2, CM_ALWON_L3_SLOW_CLKSTCTRL);

	/* Note on Timers:
	 * There are 8 timers(0-7) out of which timer 0 is a secure timer.
	 * Timer 0 mux should not be changed
	 * For other timers, there are 3 inputs TCLKIN, 32KHz (external clk or SYSCLK18?) and CLKIN(27MHz)
	 * We select CLKIN and use that
	 */

	/* First we need to enable the modules and setup the clk path
	 * Then the timers need to be configured by writing to their registers
	 * To access the timer registers we need the module to be
	 * enabled which is what we do in the first step
	 */

	/* TIMER 1 */
	__raw_writel(0x2, CM_ALWON_TIMER_1_CLKCTRL);

	/* Selects CLKIN (27MHz) */
	__raw_writel(0x2, CM_TIMER1_CLKSEL);

	while(((__raw_readl(CM_ALWON_L3_SLOW_CLKSTCTRL) & (0x80000<<1)) >> (19+1)) != 1);

	while(((__raw_readl(CM_ALWON_TIMER_1_CLKCTRL) & 0x30000)>>16) !=0);


	__raw_writel(0x2,(DM_TIMER1_BASE + 0x54));
	while(__raw_readl(DM_TIMER1_BASE + 0x10) & 1);

	__raw_writel(0x1,(DM_TIMER1_BASE + 0x38));

	/* UARTs */
	/* Note: The clock has been set to correct rate before this step */
	__raw_writel(0x2, CM_ALWON_UART_0_CLKCTRL);
	while(__raw_readl(CM_ALWON_UART_0_CLKCTRL) != 0x2);

	__raw_writel(0x2, CM_ALWON_UART_1_CLKCTRL);
	while(__raw_readl(CM_ALWON_UART_1_CLKCTRL) != 0x2);

	__raw_writel(0x2, CM_ALWON_UART_2_CLKCTRL);
	while(__raw_readl(CM_ALWON_UART_2_CLKCTRL) != 0x2);

	while((__raw_readl(CM_ALWON_L3_SLOW_CLKSTCTRL) & 0x2100) != 0x2100);

	/* eFuse */
	__raw_writel(0x2, CM_ALWON_CUST_EFUSE_CLKCTRL);
	while(__raw_readl(CM_ALWON_CUST_EFUSE_CLKCTRL) != 0x2);

	/* GPIO0 */
	__raw_writel(0x2, CM_ALWON_GPIO_0_CLKCTRL);
	while(__raw_readl(CM_ALWON_GPIO_0_CLKCTRL) != 0x2);

	__raw_writel((BIT(8)), CM_ALWON_GPIO_0_OPTFCLKEN_DBCLK);

	/* SPI */
	__raw_writel(0x2, CM_ALWON_SPI_CLKCTRL);
	while(__raw_readl(CM_ALWON_SPI_CLKCTRL) != 0x2);

	/* I2C0 */
	__raw_writel(0x2, CM_ALWON_I2C_0_CLKCTRL);
	while(__raw_readl(CM_ALWON_I2C_0_CLKCTRL) != 0x2);

	/* Ethernet */
	__raw_writel(0x2, CM_ETHERNET_CLKSTCTRL);
	__raw_writel(0x2, CM_ALWON_ETHERNET_0_CLKCTRL);
	__raw_writel(0x2, CM_ALWON_ETHERNET_1_CLKCTRL);



}

/******************************************************************************
 * prcm_init() - inits clocks for PRCM as defined in clocks.h
 *****************************************************************************/
void prcm_init(u32 in_ddr)
{
	/* For future */
	u32 clk_index = 0, sil_index = 0;

	__raw_writel(0x2, 0x48200010);
	/* Enable the control module */
	__raw_writel(0x2, CM_ALWON_CONTROL_CLKCTRL);

	//if (is_cpu_family() == CPU_TI816X) {
		main_pll_init_ti816x(clk_index, sil_index);
		if (!in_ddr)
			ddr_pll_init_ti816x(clk_index, sil_index);
	//}

	/* With clk freqs setup to desired values, enable the required peripherals */
	peripheral_enable();
}

/**********************************************************
 * Routine: s_init
 * Description: Does early system init of muxing and clocks.
 * - Called at time when only stack is available.
 **********************************************************/
void s_init(u32 in_ddr)
{
	l2_cache_enable();		/* Can be removed as A8 comes up with L2 enabled */
	prcm_init(in_ddr);			/* Setup the PLLs and the clocks for the peripherals */
	if (!in_ddr)
		config_ti816x_sdram_ddr();	/* Do DDR settings */
}

/* optionally do something like blinking LED */
void board_hang (void)
{ while (0) {};}

/* Reset the board */
void reset_cpu (ulong addr)
{
	addr = __raw_readl(PRM_DEVICE_RSTCTRL);
	addr &= ~BIT(1);
	addr |= BIT(1);
	__raw_writel(addr, PRM_DEVICE_RSTCTRL);
}
