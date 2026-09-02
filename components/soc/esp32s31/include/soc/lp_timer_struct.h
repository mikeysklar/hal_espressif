/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 *  SPDX-License-Identifier: Apache-2.0 OR MIT
 */

/*
 * NOT an original hal_espressif file. See lp_timer_reg.h in this directory for why.
 * Aliases the type/global, does not redefine the struct layout.
 */

#pragma once

#include "soc/rtc_timer_struct.h"

typedef rtc_timer_dev_t lp_timer_dev_t;

