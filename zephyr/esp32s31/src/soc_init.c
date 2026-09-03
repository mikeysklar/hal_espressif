/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdbool.h>
#include <assert.h>
#include "soc_init.h"
#include "esp32s31/rom/rtc.h"
#include <soc/soc.h>
#include "soc/lp_analog_peri_reg.h"
#include "soc/regi2c_saradc.h"
#include "hal/clk_tree_ll.h"
#include "hal/brownout_ll.h"
#include "hal/regi2c_ctrl_ll.h"
#include "hal/pmu_ll.h"
#include "hal/modem_syscon_ll.h"
#include "hal/modem_lpcon_ll.h"
#include "hal/mspi_ll.h"
#include "hal/cache_hal.h"
#include "hal/cache_ll.h"
#include "esp_private/esp_pmu.h"
#include "esp32s31/rom/spi_flash.h"
#include "modem/modem_lpcon_reg.h"
#include "soc/system_reg.h"
#include "soc/assist_debug_reg.h"
#include "soc/bus_monitor_reg.h"
#include "soc/hp_apm_reg.h"
#include "soc/lp_apm_reg.h"
#include "soc/lp_wdt_reg.h"
#include "hal/rwdt_ll.h"
#include "hal/axi_icm_ll.h"
#include "esp_log.h"

const static char *TAG = "soc_init";

void soc_hw_init(void)
{
#if CONFIG_ESP_HAL_EARLY_LOG_LEVEL == 0
	rtc_suppress_rom_log();
#endif

	/*
	 * Ported from ESP32-C61 without checking that this workaround and
	 * the modem-ICG block below don't apply to this chip at all: real
	 * esp-idf's esp32s31/bootloader_esp32s31.c bootloader_hardware_init()
	 * has neither. ESP32-S31's axi_icm_ll.h (a QoS-arbiter-only ICM_SYS
	 * interconnect, confirmed a different IP block from C5/C61's, which
	 * do have this reset function) has no reset function at all, and
	 * the PMU/modem ICG-code dance isn't part of this chip's real early
	 * init sequence either.
	 */
	_regi2c_ctrl_ll_master_enable_clock(true);
	regi2c_ctrl_ll_master_force_enable_clock(true);
	regi2c_ctrl_ll_master_configure_clock();
}

void ana_super_wdt_reset_config(bool enable)
{
	(void)enable;
}

void ana_bod_reset_config(bool enable)
{
	brownout_ll_ana_reset_enable(enable);
}

void ana_power_glitch_reset_config(bool enable)
{
	/*
	 * Only the VDDPST power glitch is detected.
	 *
	 * PMU_RSTB_PERIF_I2C/PMU_XPD_PERIF_I2C live in PMU_ANA_PERI_PWR_CTRL_REG
	 * on this chip, not PMU_RF_PWC_REG (which only has PMU_XPD_RF_CIRCUIT) --
	 * confirmed against the real pmu_reg.h, not just a naming rename.
	 */
	SET_PERI_REG_MASK(PMU_ANA_PERI_PWR_CTRL_REG, PMU_RSTB_PERIF_I2C);
	SET_PERI_REG_MASK(PMU_ANA_PERI_PWR_CTRL_REG, PMU_XPD_PERIF_I2C);
	REGI2C_WRITE_MASK(I2C_SARADC, POWER_GLITCH_XPD_VDET_PERIF, 0);
	REGI2C_WRITE_MASK(I2C_SARADC, POWER_GLITCH_XPD_VDET_PLLBB, 0);
	REGI2C_WRITE_MASK(I2C_SARADC, POWER_GLITCH_XPD_VDET_PLL, 0);

	REG_SET_FIELD(LP_ANA_FIB_ENABLE_REG, LP_ANA_ANA_FIB_PWR_GLITCH_ENA, 0);
	if (enable) {
		REG_SET_FIELD(LP_ANA_PG_GLITCH_CNTL_REG,
			      LP_ANA_POWER_GLITCH_RESET_ENA, 0xf);
	} else {
		REG_SET_FIELD(LP_ANA_PG_GLITCH_CNTL_REG,
			      LP_ANA_POWER_GLITCH_RESET_ENA, 0);
	}
}

void ana_reset_config(void)
{
	ana_super_wdt_reset_config(true);
	ana_bod_reset_config(true);
	ana_power_glitch_reset_config(true);
}

void super_wdt_auto_feed(void)
{
	/*
	 * LP_WDT_SWD_WKEY_VALUE is a HAL-level constant (hal/rwdt_ll.h on
	 * this chip, hal/lpwdt_ll.h elsewhere), not a register-level one --
	 * only the register/field names alias through soc/lp_wdt_reg.h.
	 * ESP32-S31's is RTC_WDT_SWD_WKEY_VALUE, matching the same
	 * 0x50D83AA1 magic value every sibling chip uses.
	 */
	REG_WRITE(LP_WDT_SWD_WPROTECT_REG, RTC_WDT_SWD_WKEY_VALUE);
	REG_SET_BIT(LP_WDT_SWD_CONFIG_REG, LP_WDT_SWD_AUTO_FEED_EN);
	REG_WRITE(LP_WDT_SWD_WPROTECT_REG, 0);
}

void wdt_reset_cpu0_info_enable(void)
{
	/*
	 * ESP32-S31 renamed the assist-debug peripheral to bus_monitor and
	 * moved its clock gate inside its own register block
	 * (BUS_MONITOR_CLOCK_GATE_REG.BUS_MONITOR_CLK_EN, default already
	 * enabled), instead of gating it externally via PCR like
	 * ESP32-C61's PCR_ASSIST_CONF_REG (whose CLK_EN/RST_EN also
	 * default to already-enabled/already-out-of-reset -- this call is
	 * defensive on both chips, not strictly required in the common
	 * case). No separate reset-enable bit is exposed for bus_monitor,
	 * so there's nothing to clear here.
	 */
	REG_SET_BIT(BUS_MONITOR_CLOCK_GATE_REG, BUS_MONITOR_CLK_EN);
	REG_WRITE(ASSIST_DEBUG_CORE_0_RCD_EN_REG,
		  ASSIST_DEBUG_CORE_0_RCD_PDEBUGEN | ASSIST_DEBUG_CORE_0_RCD_RECORDEN);
}

void check_wdt_reset(void)
{
	int wdt_rst = 0;
	soc_reset_reason_t rst_reas;

	/*
	 * ESP32-S31 has combined MWDT/RWDT reset-reason values (no
	 * per-CPU CPU0_* variants like C61's), matching real esp-idf's
	 * bootloader_check_if_wdt_reset() for this chip exactly.
	 */
	rst_reas = esp_rom_get_reset_reason(0);
	if (rst_reas == RESET_REASON_CORE_MWDT0 || rst_reas == RESET_REASON_CORE_MWDT1 ||
	    rst_reas == RESET_REASON_CORE_RWDT || rst_reas == RESET_REASON_CPU_MWDT ||
	    rst_reas == RESET_REASON_CPU_RWDT || rst_reas == RESET_REASON_SYS_RWDT) {
		ESP_EARLY_LOGW(TAG, "PRO CPU has been reset by WDT.");
		wdt_rst = 1;
	}

	(void)wdt_rst;
	wdt_reset_cpu0_info_enable();
}

/* Not supported but common bootloader calls the function. Do nothing */
void ana_clock_glitch_reset_config(bool enable)
{
	(void)enable;
}

#include "soc/pmu_reg.h"
#include "soc/rtc.h"
#include "soc/lp_clkrst_reg.h"
#include "soc/regi2c_dig_reg.h"
#include "esp_rom_serial_output.h"
#include "esp_rom_regi2c.h"
#include "soc/regi2c_bbpll.h"
#include "pmu_param.h"

/*
 * Custom bootloader_clock_configure() for the ESP32-S31 bootloader stage.
 *
 * The bootloader (simple boot or MCUboot) enables PLL and switches the CPU
 * clock source so that flash can run at higher speeds. The ROM bootloader
 * leaves the CPU on XTAL. The default weak bootloader_clock_configure()
 * skips this on a software reset, which would leave the BBPLL un-brought-up
 * and hang the application clock driver's REGI2C re-calibration; this
 * override always brings the PLL up so a software reboot recovers cleanly.
 *
 * Cannot use rtc_clk_init() or REGI2C_WRITE/REGI2C_WRITE_MASK here
 * because without BOOTLOADER_BUILD they go through
 * ANALOG_CLOCK_ENABLE() -> PERIPH_RCC_ACQUIRE_ATOMIC -> irq_lock
 * which requires kernel infrastructure not available during early boot.
 *
 * Instead, use _regi2c_impl_write/write_mask directly for BBPLL
 * register configuration, and LL functions for calibration control
 * and clock source switching.
 */
void bootloader_clock_configure(void)
{
	esp_rom_output_tx_wait_idle(0);

	/*
	 * Ported from ESP32-C61's manual BBPLL calibration sequence without
	 * checking that it doesn't apply to this chip at all: ESP32-S31's
	 * real esp-idf (v6.1-beta1) never calls clk_ll_bbpll_enable() or any
	 * BBPLL calibration function anywhere in its esp32s31 port -- BBPLL
	 * is a fixed, always-on PLL on this chip that needs no software
	 * calibration, unlike C61's. The old code also targeted
	 * SOC_CPU_CLK_SRC_PLL_F160M, a clock source this chip's real
	 * clk_tree_defs.h doesn't even declare -- ESP32-S31's real CPU PLL
	 * target is SOC_CPU_CLK_SRC_PLL_F240M (240 MHz), and get_act_hp_dbias()/
	 * get_act_lp_dbias() (the pmu_param.c trim table this port doesn't have)
	 * were only needed by that wrong manual sequence, not by the real one
	 * below.
	 *
	 * rtc_clk_init() (ported from esp-idf's esp32s31/rtc_clk_init.c) is
	 * the same function real esp-idf's bootloader_clock_configure() calls
	 * for every chip in this family. Its REGI2C_WRITE_MASK calls go
	 * through ANALOG_CLOCK_ENABLE() -> PERIPH_RCC_ACQUIRE_ATOMIC(), a
	 * critical section (esp_os_enter_critical(), a Zephyr spinlock) --
	 * safe this early since Zephyr spinlocks are lock-free atomics, not
	 * scheduler-dependent. Its rtc_clk_cpu_freq_set_config() call for
	 * SOC_CPU_CLK_SRC_PLL_F240M only tries to acquire the PLL through
	 * esp_clk_tree_enable_src(), which no-ops safely
	 * (`if (!s_clk_tree_initialized) return ESP_OK;`) until the full
	 * clock-tree subsystem initializes later -- matching real esp-idf's
	 * own bootloader-stage behavior, where the equivalent enable call is
	 * compiled out entirely under BOOTLOADER_BUILD for this exact
	 * source. The actual 240 MHz switch (rtc_clk_cpu_freq_to_pll_240_mhz)
	 * is just divider/mux register writes with no calibration wait.
	 */
	rtc_clk_config_t clk_cfg = {
		.xtal_freq = SOC_XTAL_FREQ_40M,
		.cpu_freq_mhz = 240,
		.fast_clk_src = SOC_RTC_FAST_CLK_SRC_RC_FAST,
		.slow_clk_src = SOC_RTC_SLOW_CLK_SRC_RC_SLOW,
		.clk_rtc_clk_div = 0,
		.clk_8m_clk_div = 0,
		.slow_clk_dcap = RTC_CNTL_SCK_DCAP_DEFAULT,
		.clk_8m_dfreq = RTC_CNTL_CK8M_DFREQ_DEFAULT,
		.rc32k_dfreq = RTC_CNTL_RC32K_DFREQ_DEFAULT,
	};
	enable_timer_group0_for_calibration();
	rtc_clk_init(clk_cfg);

	/*
	 * Point the MSPI flash clock at BBPLL now that the CPU is running
	 * from PLL_F240M, matching real esp-idf's
	 * bootloader_flash_config_esp32s31.c: bootloader_init_mspi_clock().
	 * Cache must be disabled while the flash clock source/divider
	 * changes underneath it.
	 */
	cache_hal_disable(CACHE_LL_LEVEL_EXT_MEM, CACHE_TYPE_ALL);
	_mspi_timing_ll_set_flash_core_clock(0, 80);
	_mspi_timing_ll_set_flash_clk_src(0, FLASH_CLK_SRC_BBPLL);
	cache_hal_enable(CACHE_LL_LEVEL_EXT_MEM, CACHE_TYPE_ALL);
#if CONFIG_ESPTOOLPY_FLASHFREQ_80M
	esp_rom_spiflash_config_clk(1, 0);
	esp_rom_spiflash_config_clk(1, 1);
#endif

	/* Clear any pending LP/RTC interrupts */
	CLEAR_PERI_REG_MASK(LP_WDT_INT_ENA_REG, LP_WDT_SUPER_WDT_INT_ENA);
	CLEAR_PERI_REG_MASK(LP_ANA_LP_INT_ENA_REG,
			    LP_ANA_BOD_MODE0_LP_INT_ENA);
	CLEAR_PERI_REG_MASK(LP_WDT_INT_ENA_REG, LP_WDT_LP_WDT_INT_ENA);
	CLEAR_PERI_REG_MASK(PMU_HP_INT_ENA_REG, PMU_SOC_WAKEUP_INT_ENA);
	CLEAR_PERI_REG_MASK(PMU_HP_INT_ENA_REG, PMU_SOC_SLEEP_REJECT_INT_ENA);

	SET_PERI_REG_MASK(LP_WDT_INT_CLR_REG, LP_WDT_SUPER_WDT_INT_CLR);
	SET_PERI_REG_MASK(LP_ANA_LP_INT_CLR_REG,
			  LP_ANA_BOD_MODE0_LP_INT_CLR);
	SET_PERI_REG_MASK(LP_WDT_INT_CLR_REG, LP_WDT_LP_WDT_INT_CLR);
}
