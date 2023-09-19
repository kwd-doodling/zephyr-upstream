/*
 * Copyright (c) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/init.h>
#include "soc.h"

#if defined(CONFIG_HPET_TIMER)
#include "sedi_driver_hpet.h"
#endif
#include "sedi_soc_regs.h"
#include <zephyr/drivers/interrupt_controller/ioapic.h>

#define IPC_HOST_BASE (0x4100000)
#define ISH_RST_REG (IPC_HOST_BASE + 0x10000 + 0x44)
#define IPC_ISH_RMP2 (IPC_HOST_BASE + 0x368)

#define PMU_RST_PREP (SEDI_PMU_BASE + 0x5c)
#define PMU_D3_STATUS (SEDI_PMU_BASE + 0x100)
#define PMU_PCE_D3_STATUS_1 (SEDI_PMU_BASE + 0x104)

#define PMU_BME_RISING_EDGE_STATUS  (1 << 25)
#define PMU_BME_SET                 (1 << 24)
#define PMU_D3_SET                  (1 << 16)
#define PMU_D0I3_ENABLE_MASK        (1 << 23)
#define PMU_D3_FALLING_EDGE_STATUS  (1 << 18)
#define PMU_D3_RISING_EDGE_STATUS   (1 << 17)

static void sedi_pm_pciedev_isr(const struct device *dev)
{
	uint32_t d3_sts = read32(PMU_D3_STATUS);

	printk("D3 0x%x\n", d3_sts);

	write32(PMU_D3_STATUS, d3_sts);

	if ((d3_sts & PMU_BME_SET) && (d3_sts & PMU_BME_RISING_EDGE_STATUS )) {
		/*
		 * Indicate completion of servicing the interrupt to IOAPIC
		 * first then indicate completion of servicing the interrupt
		 * to LAPIC
		 */
		printk("D3 reset...\n");
		write32(SEDI_IOAPIC_EOI, SEDI_IRQ_PCIEDEV);
		write32(LOAPIC_EOI, 0x0);

		do {
		} while (!(read32(IPC_ISH_RMP2) & 0x1));
		write32(ISH_RST_REG, 0);
		write32(ISH_RST_REG, 1);
	}
}

static void sedi_pm_reset_prep_isr(const struct device *dev)
{
	printk("RESET_PREP reset\n");
	write32(ISH_RST_REG, 0);
	write32(ISH_RST_REG, 1);
}

static int intel_ish_init(void)
{
#if defined(CONFIG_HPET_TIMER)
	sedi_hpet_set_min_delay(HPET_CMP_MIN_DELAY);
#endif

	printk("irq connect\n");
	IRQ_CONNECT(SEDI_IRQ_PCIEDEV, 2, sedi_pm_pciedev_isr,
			0, IOAPIC_LEVEL);
	IRQ_CONNECT(SEDI_IRQ_RESET_PREP, 0, sedi_pm_reset_prep_isr,
			0, IOAPIC_LEVEL);
	irq_enable(SEDI_IRQ_PCIEDEV);
	irq_enable(SEDI_IRQ_RESET_PREP);

	write32(PMU_RST_PREP, 0);
	write32(PMU_PCE_D3_STATUS_1, 0xffffffff);
#if 0
	write32(PMU_D3_STATUS, read32(PMU_D3_STATUS) &
			(PMU_D0I3_ENABLE_MASK | PMU_D3_SET | PMU_BME_SET));
#endif
	write32(PMU_D3_STATUS, read32(PMU_D3_STATUS));
	write32(PMU_D3_STATUS, 0x0);

	return 0;
}

SYS_INIT(intel_ish_init, PRE_KERNEL_2, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
