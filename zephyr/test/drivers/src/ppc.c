/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <device.h>
#include <devicetree/gpio.h>
#include <drivers/gpio/gpio_emul.h>
#include <zephyr.h>
#include <ztest.h>
#include <ztest_assert.h>

#include "emul/emul_syv682x.h"

#include "stubs.h"
#include "syv682x.h"
#include "timer.h"
#include "usbc_ppc.h"

#define SYV682X_ORD DT_DEP_ORD(DT_NODELABEL(syv682x_emul))
#define GPIO_USB_C1_FRS_EN_PATH DT_PATH(named_gpios, usb_c1_frs_en)
#define GPIO_USB_C1_FRS_EN_PORT DT_GPIO_PIN(GPIO_USB_C1_FRS_EN_PATH, gpios)

static const int syv682x_port = 1;

static void test_ppc_syv682x_vbus_enable(void)
{
	struct i2c_emul *emul = syv682x_emul_get(SYV682X_ORD);
	uint8_t reg;

	zassert_ok(syv682x_emul_get_reg(emul, SYV682X_CONTROL_1_REG, &reg),
			"Reading CONTROL_1 failed");
	zassert_equal(reg & SYV682X_CONTROL_1_PWR_ENB,
			SYV682X_CONTROL_1_PWR_ENB, "VBUS sourcing disabled");
	zassert_false(ppc_is_sourcing_vbus(syv682x_port),
			"PPC sourcing VBUS at beginning of test");

	zassert_ok(ppc_vbus_source_enable(syv682x_port, true),
			"VBUS enable failed");
	zassert_ok(syv682x_emul_get_reg(emul, SYV682X_CONTROL_1_REG, &reg),
			"Reading CONTROL_1 failed");
	zassert_equal(reg & SYV682X_CONTROL_1_PWR_ENB, 0,
			"VBUS sourcing disabled");
	zassert_true(ppc_is_sourcing_vbus(syv682x_port),
			"PPC is not sourcing VBUS after VBUS enabled");
}

static void test_ppc_syv682x_interrupt(void)
{
	struct i2c_emul *emul = syv682x_emul_get(SYV682X_ORD);
	uint8_t reg;

	/* An OC event less than 100 ms should not cause VBUS to turn off. */
	syv682x_emul_set_condition(emul, SYV682X_STATUS_OC_5V,
			SYV682X_CONTROL_4_NONE);
	msleep(50);
	zassert_true(ppc_is_sourcing_vbus(syv682x_port),
			"PPC is not sourcing VBUS after 50 ms OC");
	/* But one greater than 100 ms should. */
	msleep(60);
	zassert_false(ppc_is_sourcing_vbus(syv682x_port),
			"PPC is sourcing VBUS after 100 ms OC");

	syv682x_emul_set_condition(emul, SYV682X_STATUS_NONE,
			SYV682X_CONTROL_4_NONE);
	/*
	 * TODO(b/190519131): Organize the tests to be more hermetic and avoid
	 * the following issue: The driver triggers overcurrent protection. If
	 * overcurrent protection is triggered 3 times, the TC won't turn the
	 * port back on without a detach. This could frustrate efforts to test
	 * the TC.
	 */

	/*
	 * A TSD event should cause the driver to disable source and sink paths.
	 * (The device will have already physically disabled them.) The state of
	 * the sink path is not part of the driver's API.
	 */
	zassert_ok(ppc_vbus_source_enable(syv682x_port, true),
			"Source enable failed");
	syv682x_emul_set_condition(emul, SYV682X_STATUS_TSD,
			SYV682X_CONTROL_4_NONE);
	msleep(1);
	zassert_false(ppc_is_sourcing_vbus(syv682x_port),
			"PPC is sourcing power after TSD");
	syv682x_emul_set_condition(emul, SYV682X_STATUS_NONE,
			SYV682X_CONTROL_4_NONE);

	/* An OVP event should cause the driver to disable the source path. */
	zassert_ok(ppc_vbus_source_enable(syv682x_port, true),
			"Source enable failed");
	syv682x_emul_set_condition(emul, SYV682X_STATUS_OVP,
			SYV682X_CONTROL_4_NONE);
	msleep(1);
	zassert_false(ppc_is_sourcing_vbus(syv682x_port),
			"PPC is sourcing power after OVP");
	syv682x_emul_set_condition(emul, SYV682X_STATUS_NONE,
			SYV682X_CONTROL_4_NONE);

	/*
	 * A high-voltage OC while sinking should cause the driver to try to
	 * re-enable the sink path until the OC count limit is reached, at which
	 * point the driver should leave it disabled.
	 */
	zassert_ok(ppc_vbus_sink_enable(syv682x_port, true),
			"Sink enable failed");
	syv682x_emul_set_condition(emul, SYV682X_STATUS_OC_HV,
			SYV682X_CONTROL_4_NONE);
	msleep(1);
	zassert_ok(syv682x_emul_get_reg(emul, SYV682X_CONTROL_1_REG, &reg),
			"Reading CONTROL_1 failed");
	zassert_equal(reg & SYV682X_CONTROL_1_PWR_ENB, 0,
			"Power path disabled after HV_OC handled");
	syv682x_emul_set_condition(emul, SYV682X_STATUS_OC_HV,
			SYV682X_CONTROL_4_NONE);
	msleep(1);
	zassert_ok(syv682x_emul_get_reg(emul, SYV682X_CONTROL_1_REG, &reg),
			"Reading CONTROL_1 failed");
	zassert_equal(reg & SYV682X_CONTROL_1_PWR_ENB, 0,
			"Power path disabled after HV_OC handled");
	syv682x_emul_set_condition(emul, SYV682X_STATUS_OC_HV,
			SYV682X_CONTROL_4_NONE);
	msleep(1);
	zassert_ok(syv682x_emul_get_reg(emul, SYV682X_CONTROL_1_REG, &reg),
			"Reading CONTROL_1 failed");
	zassert_equal(reg & SYV682X_CONTROL_1_PWR_ENB,
			SYV682X_CONTROL_1_PWR_ENB,
			"Power path enabled after HV_OC handled 3 times");
	syv682x_emul_set_condition(emul, SYV682X_STATUS_NONE,
			SYV682X_CONTROL_4_NONE);

	/*
	 * A VCONN OC event less than 100 ms should not cause the driver to turn
	 * VCONN off.
	 */
	ppc_set_vconn(syv682x_port, true);
	syv682x_emul_set_condition(emul, SYV682X_STATUS_NONE,
			SYV682X_CONTROL_4_VCONN_OCP);
	msleep(1);
	zassert_ok(syv682x_emul_get_reg(emul, SYV682X_CONTROL_4_REG, &reg),
			"Reading CONTROL_4 failed");
	zassert_true(reg &
			(SYV682X_CONTROL_4_VCONN1 | SYV682X_CONTROL_4_VCONN2),
			"VCONN disabled after initial VCONN OC");
	msleep(50);
	zassert_ok(syv682x_emul_get_reg(emul, SYV682X_CONTROL_4_REG, &reg),
			"Reading CONTROL_4 failed");
	zassert_true(reg &
			(SYV682X_CONTROL_4_VCONN1 | SYV682X_CONTROL_4_VCONN2),
			"VCONN disabled after short VCONN OC");
	/*
	 * But if the event keeps going for over 100 ms continuously, the driver
	 * should turn VCONN off.
	 */
	msleep(60);
	zassert_ok(syv682x_emul_get_reg(emul, SYV682X_CONTROL_4_REG, &reg),
			"Reading CONTROL_4 failed");
	zassert_false(reg &
			(SYV682X_CONTROL_4_VCONN1 | SYV682X_CONTROL_4_VCONN2),
			"VCONN enabled after long VCONN OC");
	syv682x_emul_set_condition(emul, SYV682X_STATUS_NONE,
			SYV682X_CONTROL_4_NONE);

	/*
	 * A VCONN over-voltage (VBAT_OVP) event will cause the device to
	 * disconnect CC and VCONN. The driver should then reinitialize the
	 * device, which will enable both CC lines but leave VCONN disabled. The
	 * driver should then run generic CC over-voltage handling.
	 */
	ppc_set_vconn(syv682x_port, true);
	syv682x_emul_set_condition(emul, SYV682X_STATUS_NONE,
			SYV682X_CONTROL_4_VBAT_OVP);
	msleep(1);
	zassert_ok(syv682x_emul_get_reg(emul, SYV682X_CONTROL_4_REG, &reg),
			"Reading CONTROL_4 failed");
	zassert_true(reg & SYV682X_CONTROL_4_CC1_BPS,
			"CC1 disabled after handling VBAT_OVP");
	zassert_true(reg & SYV682X_CONTROL_4_CC2_BPS,
			"CC2 disabled after handling VBAT_OVP");
	zassert_false(reg &
			(SYV682X_CONTROL_4_VCONN1 | SYV682X_CONTROL_4_VCONN2),
			"VCONN enabled after handling VBAT_OVP");
	/*
	 * TODO(b/190519131): The PD stack should generate a Reset in response
	 * to a CC over-voltage event. There is currently no easy way to test
	 * that a Hard Reset occurred.
	 */
	syv682x_emul_set_condition(emul, SYV682X_STATUS_NONE,
			SYV682X_CONTROL_4_NONE);
}

static void test_ppc_syv682x_frs(void)
{
	struct i2c_emul *emul = syv682x_emul_get(SYV682X_ORD);
	const struct device *gpio_dev =
		DEVICE_DT_GET(DT_GPIO_CTLR(GPIO_USB_C1_FRS_EN_PATH, gpios));
	uint8_t reg;

	/*
	 * Enabling FRS should enable only the appropriate CC line based on
	 * polarity. Disabling FRS should enable both CC lines.
	 */
	ppc_vbus_sink_enable(syv682x_port, true);
	zassert_false(ppc_is_sourcing_vbus(syv682x_port),
			"PPC is sourcing VBUS after sink enabled");
	ppc_set_polarity(syv682x_port, 0 /* CC1 */);
	ppc_set_frs_enable(syv682x_port, true);
	zassert_equal(gpio_emul_output_get(gpio_dev, GPIO_USB_C1_FRS_EN_PORT),
			1, "FRS enabled, but FRS GPIO not asserted");
	zassert_ok(syv682x_emul_get_reg(emul, SYV682X_CONTROL_4_REG, &reg),
			"Reading CONTROL_4 failed");
	zassert_equal(reg &
			(SYV682X_CONTROL_4_CC1_BPS | SYV682X_CONTROL_4_CC2_BPS),
			SYV682X_CONTROL_4_CC1_BPS,
			"FRS enabled with CC1 polarity, but CONTROL_4 is 0x%x",
			reg);
	ppc_set_frs_enable(syv682x_port, false);
	zassert_equal(gpio_emul_output_get(gpio_dev, GPIO_USB_C1_FRS_EN_PORT),
			0, "FRS disabled, but FRS GPIO not deasserted");
	zassert_ok(syv682x_emul_get_reg(emul, SYV682X_CONTROL_4_REG, &reg),
			"Reading CONTROL_4 failed");
	zassert_equal(reg &
			(SYV682X_CONTROL_4_CC1_BPS | SYV682X_CONTROL_4_CC2_BPS),
			SYV682X_CONTROL_4_CC1_BPS | SYV682X_CONTROL_4_CC2_BPS,
			"FRS enabled with CC1 polarity, but CONTROL_4 is 0x%x",
			reg);

	ppc_set_polarity(syv682x_port, 1 /* CC2 */);
	ppc_set_frs_enable(syv682x_port, true);
	zassert_ok(syv682x_emul_get_reg(emul, SYV682X_CONTROL_4_REG, &reg),
			"Reading CONTROL_4 failed");
	zassert_equal(reg &
			(SYV682X_CONTROL_4_CC1_BPS | SYV682X_CONTROL_4_CC2_BPS),
			SYV682X_CONTROL_4_CC2_BPS,
			"FRS enabled with CC2 polarity, but CONTROL_4 is 0x%x",
			reg);

	/*
	 * An FRS event when the PPC is Sink should cause the PPC to switch from
	 * Sink to Source.
	 */
	syv682x_emul_set_condition(emul, SYV682X_STATUS_FRS,
			SYV682X_CONTROL_4_NONE);
	msleep(1);
	zassert_true(ppc_is_sourcing_vbus(syv682x_port),
			"PPC is not sourcing VBUS after FRS signal handled");
	syv682x_emul_set_condition(emul, SYV682X_STATUS_NONE,
			SYV682X_CONTROL_4_NONE);
}

static void test_ppc_syv682x_source_current_limit(void)
{
	struct i2c_emul *emul = syv682x_emul_get(SYV682X_ORD);
	uint8_t reg;
	int ilim_val;

	zassert_ok(ppc_set_vbus_source_current_limit(syv682x_port,
				TYPEC_RP_USB),
			"Could not set source current limit");
	zassert_ok(syv682x_emul_get_reg(emul, SYV682X_CONTROL_1_REG, &reg),
			"Reading CONTROL_1 failed");
	ilim_val = (reg & SYV682X_5V_ILIM_MASK) >> SYV682X_5V_ILIM_BIT_SHIFT;
	zassert_equal(reg & SYV682X_5V_ILIM_MASK, SYV682X_5V_ILIM_1_25,
			"Set USB Rp value, but 5V_ILIM is %d", ilim_val);

	zassert_ok(ppc_set_vbus_source_current_limit(syv682x_port,
				TYPEC_RP_1A5),
			"Could not set source current limit");
	zassert_ok(syv682x_emul_get_reg(emul, SYV682X_CONTROL_1_REG, &reg),
			"Reading CONTROL_1 failed");
	ilim_val = (reg & SYV682X_5V_ILIM_MASK) >> SYV682X_5V_ILIM_BIT_SHIFT;
	zassert_equal(ilim_val, SYV682X_5V_ILIM_1_75,
			"Set 1.5A Rp value, but 5V_ILIM is %d", ilim_val);

	zassert_ok(ppc_set_vbus_source_current_limit(syv682x_port,
				TYPEC_RP_3A0),
			"Could not set source current limit");
	zassert_ok(syv682x_emul_get_reg(emul, SYV682X_CONTROL_1_REG, &reg),
			"Reading CONTROL_1 failed");
	ilim_val = (reg & SYV682X_5V_ILIM_MASK) >> SYV682X_5V_ILIM_BIT_SHIFT;
	zassert_equal(ilim_val, SYV682X_5V_ILIM_3_30,
			"Set 3.0A Rp value, but 5V_ILIM is %d", ilim_val);
}

static void test_ppc_syv682x_write_busy(void)
{
	struct i2c_emul *emul = syv682x_emul_get(SYV682X_ORD);

	/*
	 * Writes should fail while the BUSY bit is set, except that writes to
	 * CONTROL_4 should succeed on the SYV682C. 100 reads is intentionally
	 * many more than the driver is expected to make before reaching its
	 * timeout. It is not a goal of this test to verify the frequency of
	 * polling or the exact value of the timeout.
	 */
	syv682x_emul_set_busy_reads(emul, 100);
	zassert_equal(ppc_set_vbus_source_current_limit(syv682x_port,
				TYPEC_RP_USB),
			EC_ERROR_TIMEOUT, "SYV682 busy, but write completed");
	zassert_ok(ppc_set_frs_enable(syv682x_port, false),
			"Could not set CONTROL_4 while busy on SYV682C");

	/*
	 * If the busy bit clears before the driver reaches its timeout, the
	 * write should succeed.
	 */
	syv682x_emul_set_busy_reads(emul, 1);
	zassert_equal(ppc_set_vbus_source_current_limit(syv682x_port,
				TYPEC_RP_USB), 0,
			"SYV682 not busy, but write failed");

	syv682x_emul_set_busy_reads(emul, 0);
}

static void test_ppc_syv682x_dev_is_connected(void)
{
	struct i2c_emul *emul = syv682x_emul_get(SYV682X_ORD);
	uint8_t reg;

	zassert_ok(ppc_dev_is_connected(syv682x_port, PPC_DEV_SRC),
			"Could not connect device as source");
	zassert_ok(syv682x_emul_get_reg(emul, SYV682X_CONTROL_2_REG, &reg),
			"Reading CONTROL_2 failed");
	zassert_false(reg & SYV682X_CONTROL_2_FDSG,
			"Connected as source, but force discharge enabled");

	zassert_ok(ppc_dev_is_connected(syv682x_port, PPC_DEV_DISCONNECTED),
			"Could not disconnect device");
	zassert_ok(syv682x_emul_get_reg(emul, SYV682X_CONTROL_2_REG, &reg),
			"Reading CONTROL_2 failed");
	zassert_true(reg & SYV682X_CONTROL_2_FDSG,
			"Disconnected, but force discharge disabled");

	zassert_ok(ppc_dev_is_connected(syv682x_port, PPC_DEV_SNK),
			"Could not connect device as source");
}

static void test_ppc_syv682x(void)
{
	zassert_ok(ppc_init(syv682x_port), "PPC init failed");

	test_ppc_syv682x_vbus_enable();
	test_ppc_syv682x_interrupt();
	test_ppc_syv682x_frs();
	test_ppc_syv682x_source_current_limit();
	test_ppc_syv682x_write_busy();
	test_ppc_syv682x_dev_is_connected();
}

void test_suite_ppc(void)
{
	ztest_test_suite(ppc,
			 ztest_user_unit_test(test_ppc_syv682x));
	ztest_run_test_suite(ppc);
}
