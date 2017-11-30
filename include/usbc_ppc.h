/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_USBC_PPC_H
#define __CROS_EC_USBC_PPC_H

#include "common.h"

/* Common APIs for USB Type-C Power Path Controllers (PPC) */

struct ppc_drv {
	/**
	 * Initialize the PPC.
	 *
	 * @param port: The Type-C port number.
	 * @return EC_SUCCESS when init was successful, error otherwise.
	 */
	int (*init)(int port);

	/**
	 * Is the port sourcing Vbus?
	 *
	 * @param port: The Type-C port number.
	 * @return 1 if sourcing Vbus, 0 if not.
	 */
	int (*is_sourcing_vbus)(int port);

	/**
	 * Turn on/off the charge path FET, such that current flows into the
	 * system.
	 *
	 * @param port: The Type-C port number.
	 * @param enable: 1: Turn on the FET, 0: turn off the FET.
	 * @return EC_SUCCESS on success, error otherwise.
	 */
	int (*vbus_sink_enable)(int port, int enable);

	/**
	 * Turn on/off the source path FET, such that current flows from the
	 * system.
	 *
	 * @param port: The Type-C port number.
	 * @param enable: 1: Turn on the FET, 0: turn off the FET.
	 * @return EC_SUCCESS on success, error otherwise.
	 */
	int (*vbus_source_enable)(int port, int enable);

#ifdef CONFIG_CMD_PPC_DUMP
	/**
	 * Perform a register dump of the PPC.
	 *
	 * @param port: The Type-C port number.
	 * @return EC_SUCCESS on success, error otherwise.
	 */
	int (*reg_dump)(int port);
#endif /* defined(CONFIG_CMD_PPC_DUMP) */
};

struct ppc_config_t {
	int i2c_port;
	int i2c_addr;
	const struct ppc_drv *drv;
};

extern const struct ppc_config_t ppc_chips[];
extern const unsigned int ppc_cnt;

/**
 * Is the port sourcing Vbus?
 *
 * @param port: The Type-C port number.
 * @return 1 if sourcing Vbus, 0 if not.
 */
int ppc_is_sourcing_vbus(int port);

/**
 * Turn on/off the charge path FET, such that current flows into the
 * system.
 *
 * @param port: The Type-C port number.
 * @param enable: 1: Turn on the FET, 0: turn off the FET.
 * @return EC_SUCCESS on success, error otherwise.
 */
int ppc_vbus_sink_enable(int port, int enable);

/**
 * Turn on/off the source path FET, such that current flows from the
 * system.
 *
 * @param port: The Type-C port number.
 * @param enable: 1: Turn on the FET, 0: turn off the FET.
 * @return EC_SUCCESS on success, error otherwise.
 */
int ppc_vbus_source_enable(int port, int enable);

#endif /* !defined(__CROS_EC_USBC_PPC_H) */
