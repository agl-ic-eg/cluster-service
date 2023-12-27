/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	cluster-service.c
 * @brief	main source file for cluster-service
 */

#include "cluster-service-util.h"
#include "data-pool-service.h"

#include "demo-data-generator.h"

#include "alarm-sound.h"
#include "socketcan-receiver.h"

#include <stdlib.h>
#include <systemd/sd-daemon.h>
#include <systemd/sd-event.h>

#include <stdio.h>

int main(int argc, char *argv[])
{
	sd_event *event = NULL;
	data_pool_service_handle handle = NULL;
	socketcan_client_handle_sdevent can_handle = NULL;
	alarm_sound_worker_t *worker;
	int ret = -1;

	ret = sd_event_default(&event);
	if (ret < 0)
		goto finish;

	ret = signal_setup(event);
	if (ret < 0)
		goto finish;

	// Enable automatic service watchdog support
	ret = sd_event_set_watchdog(event, 1);
	if (ret < 0)
		goto finish;

	(void) demo_data_generator_setup(event);
	(void) socketcan_client_setup_sdevent(event, &can_handle);

	ret = data_pool_service_setup(event, &handle);

	(void) sd_notify(
		1,
		"READY=1\n"
		"STATUS=Daemon startup completed, processing events.");
	ret = sd_event_loop(event);

finish:
	(void) data_pool_service_cleanup(handle);
	(void) socketcan_client_cleanup_sdevent(can_handle);
	(void) demo_data_generator_cleanup();
	event = sd_event_unref(event);

	return ret < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
