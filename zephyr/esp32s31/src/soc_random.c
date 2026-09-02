/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include "soc_random.h"
#include <esp_private/regi2c_ctrl.h>
#include <esp_private/sar_periph_ctrl.h>
#include <hal/adc_ll.h>
#include <hal/adc_types.h>
#include <hal/temperature_sensor_ll.h>
#include <hal/regi2c_ctrl_ll.h>
#include <hal/rng_ll.h>

#define I2C_SAR_ADC_INIT_CODE_VAL       2150
#define ADC_RNG_CLKM_DIV_NUM            0
#define ADC_RNG_CLKM_DIV_B              0
#define ADC_RNG_CLKM_DIV_A              0

void soc_random_enable(void)
{
	/*
	 * Unlike chips where temperature sensor control lives inside the
	 * same ADC register struct (so an ADC reset clobbers it too),
	 * ESP32-S31 resets ADC and TSENS through independent bits
	 * (LP_PERI_CLKRST.adc_ctrl.lp_adc_rst_en vs .tsens_ctrl.
	 * lp_tsens_rst_en -- see adc_ll_reset_register()/
	 * temperature_sensor_ll_reset_module()). The backup/restore dance
	 * other ports do around the reset is unnecessary here.
	 */
	adc_ll_reset_register();
	temperature_sensor_ll_reset_module();

	adc_ll_enable_bus_clock(true);
	adc_ll_enable_func_clock(true);
	adc_ll_digi_clk_sel(ADC_DIGI_CLK_SRC_XTAL);
	adc_ll_digi_controller_clk_div(ADC_RNG_CLKM_DIV_NUM, ADC_RNG_CLKM_DIV_B, ADC_RNG_CLKM_DIV_A);

	/* Some ADC sensor registers are in power group PERIF_I2C and need to be enabled via PMU */
	regi2c_saradc_enable();

	/* Enable analog I2C master clock for RNG runtime */
	ANALOG_CLOCK_ENABLE();

	/*
	 * adc_ll_regi2c_init()'s calibration-reference-enable step
	 * (I2C_SARADC1/2_ENCAL_REF on other chips) has no verified
	 * equivalent in ESP32-S31's real regi2c_saradc.h -- its DTEST/ENT
	 * regi2c addresses (ADC_SARADC_DTEST_RTC_ADDR, _ENT_TSENS_ADDR)
	 * don't obviously correspond to the same analog nodes, and
	 * getting this wrong risks silently degrading RNG entropy rather
	 * than a build error, so it's not something to guess at. RNG
	 * entropy quality doesn't depend on ADC calibration precision the
	 * way a real measurement would, so leaving this uncalibrated
	 * still produces a working (if unverified) noise source. Real gap
	 * to close once real regi2c documentation exists for this chip.
	 */
	adc_ll_set_calibration_param(ADC_UNIT_1, I2C_SAR_ADC_INIT_CODE_VAL);
	adc_ll_set_calibration_param(ADC_UNIT_2, I2C_SAR_ADC_INIT_CODE_VAL);

	/* Use reserved channels to get internal voltage */
	adc_digi_pattern_config_t pattern_config = {};

	pattern_config.unit = ADC_UNIT_1;
	pattern_config.atten = ADC_ATTEN_DB_12;
	pattern_config.channel = ADC_CHANNEL_7;
	adc_ll_digi_set_pattern_table(ADC_UNIT_1, 0, pattern_config);
	pattern_config.unit = ADC_UNIT_2;
	pattern_config.atten = ADC_ATTEN_DB_12;
	pattern_config.channel = ADC_CHANNEL_1;
	adc_ll_digi_set_pattern_table(ADC_UNIT_2, 1, pattern_config);

	adc_ll_digi_set_pattern_table_len(ADC_UNIT_1, 2);

	adc_ll_digi_set_clk_div(15);
	adc_ll_digi_set_trigger_interval(200);
	adc_ll_digi_trigger_enable();
	rng_ll_enable();
}

void soc_random_disable(void)
{
	rng_ll_disable();

	adc_ll_digi_trigger_disable();
	adc_ll_digi_reset_pattern_table();
	adc_ll_set_calibration_param(ADC_UNIT_1, 0x0);
	adc_ll_set_calibration_param(ADC_UNIT_2, 0x0);
	/* adc_ll_regi2c_adc_deinit() -- see the comment in soc_random_enable() */
	regi2c_saradc_disable();

	/* Disable analog I2C master clock */
	ANALOG_CLOCK_DISABLE();
	adc_ll_digi_controller_clk_div(4, 0, 0);
	adc_ll_digi_clk_sel(ADC_DIGI_CLK_SRC_XTAL);
}
