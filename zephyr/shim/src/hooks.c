/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <kernel.h>
#include <zephyr.h>

#include "common.h"
#include "console.h"
#include "ec_tasks.h"
#include "hooks.h"
#include "task.h"
#include "timer.h"

int hook_call_deferred(const struct deferred_data *data, int us)
{
	struct k_delayed_work *work = data->delayed_work;
	int rv = 0;

	if (us == -1) {
		k_delayed_work_cancel(work);
	} else if (us >= 0) {
		rv = k_delayed_work_submit(work, K_USEC(us));
		if (rv < 0)
			cprints(CC_HOOK,
				"Warning: deferred call not submitted, "
				"deferred_data=0x%pP, err=%d",
				data, rv);
	} else {
		return EC_ERROR_PARAM2;
	}

	return rv;
}

static struct zephyr_shim_hook_list *hook_registry[HOOK_TYPE_COUNT];

void zephyr_shim_setup_hook(enum hook_type type, void (*routine)(void),
			    int priority, struct zephyr_shim_hook_list *entry)
{
	struct zephyr_shim_hook_list **loc = &hook_registry[type];

	/* Find the correct place to put the entry in the registry. */
	while (*loc && (*loc)->priority < priority)
		loc = &((*loc)->next);

	/* Setup the entry. */
	entry->routine = routine;
	entry->priority = priority;
	entry->next = *loc;

	/* Insert the entry. */
	*loc = entry;
}

void hook_notify(enum hook_type type)
{
	struct zephyr_shim_hook_list *p;

	for (p = hook_registry[type]; p; p = p->next)
		p->routine();
}

static void check_hook_task_priority(k_tid_t thread)
{
	if (k_thread_priority_get(thread) != LOWEST_THREAD_PRIORITY)
		cprintf(CC_HOOK,
			"ERROR: %s has priority %d but must be priority %d\n",
			k_thread_name_get(thread),
			k_thread_priority_get(thread), LOWEST_THREAD_PRIORITY);
}

void hook_task(void *u)
{
	/* Periodic hooks will be called first time through the loop */
	static uint64_t last_second = -SECOND;
	static uint64_t last_tick = -HOOK_TICK_INTERVAL;

	/*
	 * Verify deferred routines are run at the lowest priority.
	 */
	check_hook_task_priority(&k_sys_work_q.thread);
	check_hook_task_priority(k_current_get());

	while (1) {
		uint64_t t = get_time().val;
		int next = 0;

		if (t - last_tick >= HOOK_TICK_INTERVAL) {
			hook_notify(HOOK_TICK);
			last_tick = t;
		}

		if (t - last_second >= SECOND) {
			hook_notify(HOOK_SECOND);
			last_second = t;
		}

		/* Calculate when next tick needs to occur */
		t = get_time().val;
		if (last_tick + HOOK_TICK_INTERVAL > t)
			next = last_tick + HOOK_TICK_INTERVAL - t;

		/*
		 * Sleep until next tick, unless we've already exceeded
		 * HOOK_TICK_INTERVAL.
		 */
		if (next > 0)
			task_wait_event(next);
	}
}
