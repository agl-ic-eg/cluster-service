/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	cluster-service.c
 * @brief	main source file for cluster-service
 */

#include "cluster-service-util.h"
#include "data-pool-service.h"
#include "sound/alarm-sound.h"
#include "source/data-source.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <systemd/sd-daemon.h>
#include <systemd/sd-event.h>


typedef struct s_cluster_service_options {
	int sound;
	struct s_data_source_config data_source_config;
} cluster_service_options_t;
cluster_service_options_t g_options;

static struct option long_options[] = {
	{"help", no_argument, 0, 1},
	{"sound", no_argument, NULL, 10},
#ifdef DATA_SOURCE_FAKE
	{"demo", no_argument, NULL, 20},
#endif	//#ifdef DATA_SOURCE_FAKE
#ifdef DATA_SOURCE_SOCKET_CAN
	{"can", required_argument, NULL, 30},
#endif	//#ifdef DATA_SOURCE_SOCKET_CAN
	{0, 0, 0, 0},
};

static void usage(void)
{
	(void) fprintf(stdout,
		"usage: [options] \n\n"
	    " --help       Print help strings.\n"
	    " --sound      Enable alarm sound.\n"
#ifdef DATA_SOURCE_FAKE
		" --demo       Enable demo signals.\n"
#endif //#ifdef DATA_SOURCE_FAKE
#ifdef DATA_SOURCE_SOCKET_CAN
		" --can=NAME   Enable socket can feature (NAME=can if name).\n"
#endif //#ifdef DATA_SOURCE_SOCKET_CAN
	);
}

static int get_command_line_options(int argc, char *argv[])
{
	int ret = -1;
	int result = -1;

	do {
		ret = getopt_long(argc, argv, "", long_options, NULL);
		if (ret == 1) {
			usage();
			result = -1;
			break;
		} else if (ret == 10) {
			g_options.sound = 1;
			result = 0;
#ifdef DATA_SOURCE_FAKE
		} else if (ret == 20) {
			g_options.data_source_config.demo = 0;
			result = 0;
#endif //#ifdef DATA_SOURCE_FAKE
#ifdef DATA_SOURCE_SOCKET_CAN
		} else if (ret == 30) {
			size_t name_length = 0;
			name_length = strlen(optarg);
			if (name_length > 1) {
				g_options.data_source_config.canif = (char*)malloc(name_length + 1);
				(void) strcpy(g_options.data_source_config.canif, optarg);
			}
			result = 0;
#endif //#ifdef DATA_SOURCE_SOCKET_CAN
		} else {
			break;
		}
	} while(1);

	return result;
}

int main(int argc, char *argv[])
{
	sd_event *event = NULL;
	data_pool_service_handle handle = NULL;
	data_source_handle_t data_source_handle = NULL;
	alarm_sound_service_handle alarm_sound_handle = NULL;
	int ret = -1;

	ret = get_command_line_options(argc, argv);
	if (ret < 0) {
		goto finish;
	}

	ret = sd_event_default(&event);
	if (ret < 0) {
		goto finish;
	}

	ret = signal_setup(event);
	if (ret < 0) {
		goto finish;
	}

	// Enable automatic service watchdog support
	ret = sd_event_set_watchdog(event, 1);
	if (ret < 0) {
		goto finish;
	}

	ret = data_source_setup(event, &data_source_handle, &g_options.data_source_config);
	if (ret < 0) {
		goto finish;
	}

	ret = data_pool_service_setup(event, &handle);
	if (ret < 0) {
		goto finish;
	}

	if (g_options.sound != 0) {
		ret = alarm_sound_service_setup(&alarm_sound_handle);
		if (ret < 0) {
			goto finish;
		}	
	}

	(void) sd_notify(
		1,
		"READY=1\n"
		"STATUS=Daemon startup completed, processing events.");

	ret = sd_event_loop(event);

finish:
	(void) alarm_sound_service_cleanup(alarm_sound_handle);
	(void) data_pool_service_cleanup(handle);
	(void) data_source_cleanup(data_source_handle);
	event = sd_event_unref(event);

	return ret < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
