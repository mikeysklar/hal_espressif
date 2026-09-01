/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

/*
 * NOT a full apm_ll.h port. hal_espressif has no esp_hal_security/esp32s31
 * driver yet; this covers only the two functions the S31 zephyr/hw_init.c
 * bring-up calls (apm_ll_hp_apm_enable_ctrl_filter_all/
 * apm_ll_lp_apm_enable_ctrl_filter_all), using this chip's own real
 * registers -- not ported from a sibling chip:
 *   - soc/hp_apm_reg.h: HP_APM_REGION_FILTER_EN_REG, bits [15:0], one
 *     enable bit per region (16 regions on HP_APM), default 1 (enabled).
 *   - soc/lp_apm_reg.h: LP_APM_REGION_FILTER_EN_REG, bits [7:0], same
 *     idea with 8 regions on LP_APM.
 * Other chips (e.g. ESP32-C61) instead have a single "FUNC_CTRL_REG"
 * doing the same enable/disable-all job; ESP32-S31 names/shapes it
 * differently (a per-region bitmask), so this is not a rename shim like
 * lp_timer_reg.h/lp_wdt_reg.h -- it's a from-scratch two-function driver
 * against this chip's actual register layout.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "soc/soc.h"
#include "soc/hp_apm_reg.h"
#include "soc/lp_apm_reg.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Enable/disable all region filters in HP-APM
 *
 * @param enable True to enable, false to disable
 */
static inline void apm_ll_hp_apm_enable_ctrl_filter_all(bool enable)
{
    REG_WRITE(HP_APM_REGION_FILTER_EN_REG, enable ? HP_APM_REGION_FILTER_EN_V : 0);
}

/**
 * @brief Enable/disable all region filters in LP-APM
 *
 * @param enable True to enable, false to disable
 */
static inline void apm_ll_lp_apm_enable_ctrl_filter_all(bool enable)
{
    REG_WRITE(LP_APM_REGION_FILTER_EN_REG, enable ? LP_APM_REGION_FILTER_EN_V : 0);
}

#ifdef __cplusplus
}
#endif
