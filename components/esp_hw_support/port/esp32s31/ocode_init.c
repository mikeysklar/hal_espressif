/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

/*
 * STUB, not a real port. esp_ocode_calib_init() calibrates the ADC/temp
 * sensor bandgap "o-code" reference, normally via the regi2c ULP bus
 * (see e.g. esp32c6's or esp32c61's ocode_init.c, which uses
 * soc/regi2c_lp_bias.h's I2C_ULP_* fields).
 *
 * ESP32-S31 doesn't have a regi2c_lp_bias.h in hal_espressif at all (only
 * regi2c_bias.h, which is not confirmed to have the same fields/bus), so
 * this chip's real o-code calibration sequence is unknown here -- it was
 * not guessed. This stub exists only so unconditionally-referenced callers
 * link; it leaves the bandgap reference at its efuse/POR default instead
 * of software- or efuse-calibrating it, which affects ADC/temp-sensor
 * absolute accuracy, not digital I/O, boot, or core operation.
 */

#include "esp_attr.h"

void esp_ocode_calib_init(void)
{
}
