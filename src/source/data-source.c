/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	demo-data-generator.c
 * @brief	demo data generator for cluster service
 */
#include "data-source.h"
#ifdef DATA_SOURCE_FAKE
#include "fake-data/demo-data-generator.h"
#endif

#ifdef DATA_SOURCE_SOCKET_CAN
#include "socket-can/socketcan-receiver.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/** data source handle */
struct s_data_source_handle {
#ifdef DATA_SOURCE_SOCKET_CAN
	socketcan_client_handle_sdevent socketcan_handle;
#endif
	void *dummy;
};
typedef struct s_data_source_handle *data_source_handle_t;

int data_source_setup(sd_event *event, data_source_handle_t *handle, struct s_data_source_config *config)
{
	int result = -1;
	struct s_data_source_handle *dsh = NULL;

	if (handle == NULL) {
		result = -3;
		goto do_err_return;
	}

	dsh = (struct s_data_source_handle*)malloc(sizeof(struct s_data_source_handle));
	if (dsh == NULL) {
		result = -2;
		goto do_err_return;
	}

	dsh->dummy = NULL;
#ifdef DATA_SOURCE_FAKE
	if (config->demo == 1) {
		int ret = -1;
		ret = demo_data_generator_setup(event);
		if (ret < 0) {
			result = -1;
			goto do_err_return;
		}
		result = 0;
	}
#endif

#ifdef DATA_SOURCE_SOCKET_CAN
	if (config->canif != NULL) {
		int ret = -1;
		ret = socketcan_client_setup_sdevent(event, &dsh->socketcan_handle, config->canif);
		if (ret < 0) {
			result = -1;
			goto do_err_return;
		}
		result = 0;
	}
#endif

	(*handle) = (data_source_handle_t)dsh;
	return result;

do_err_return:
	(void) free(dsh);
	return result;
}

int data_source_cleanup(data_source_handle_t handle)
{
	int result = 0;
	struct s_data_source_handle *dsh = NULL;

	if (handle == NULL) {
		result = -3;
		goto do_err_return;
	}

	dsh = (struct s_data_source_handle*)handle;
#ifdef DATA_SOURCE_SOCKET_CAN
	{
		int ret = -1;
		ret = socketcan_client_cleanup_sdevent(dsh->socketcan_handle);
		if (ret < 0) {
			result = -1;
			goto do_err_return;
		}
	}
#endif

#ifdef DATA_SOURCE_FAKE
	{
		int ret = -1;
		ret = demo_data_generator_cleanup();
		if (ret < 0) {
			result = -1;
			goto do_err_return;
		}
	}
#endif

	(void) free(handle);
	return result;

do_err_return:
	return result;
}
