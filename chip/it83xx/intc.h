/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* INTC control module for IT83xx. */

#ifndef __CROS_EC_INTC_H
#define __CROS_EC_INTC_H

int intc_get_ec_int(void);
void pm1_ibf_interrupt(void);
void pm2_ibf_interrupt(void);
void pm3_ibf_interrupt(void);
void pm4_ibf_interrupt(void);
void pm5_ibf_interrupt(void);
void lpcrst_interrupt(enum gpio_signal signal);
void peci_interrupt(void);
void adc_interrupt(void);
void i2c_interrupt(int port);
void clock_sleep_mode_wakeup_isr(void);
int clock_ec_wake_from_sleep(void);
void __enter_hibernate(uint32_t seconds, uint32_t microseconds);
void espi_reset_pin_asserted_interrupt(enum gpio_signal signal);
void espi_fw_reset_module(void);
void espi_interrupt(void);
void espi_vw_interrupt(void);
void espi_init(void);

#if defined(CONFIG_HOSTCMD_X86) && defined(HAS_TASK_KEYPROTO)
void lpc_kbc_ibf_interrupt(void);
void lpc_kbc_obe_interrupt(void);
#endif

#endif /* __CROS_EC_INTC_H */
