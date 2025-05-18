/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	data-source.h
 * @brief	data source header
 */
#ifndef DATA_SOURCE_H
#define DATA_SOURCE_H
//-----------------------------------------------------------------------------
#include <stdint.h>
#include <systemd/sd-event.h>

struct s_data_source_config {
	int demo;
	char *canif;
};

/** data source handle */
struct s_data_source_handle;
typedef struct s_data_source_handle *data_source_handle_t;

int data_source_setup(sd_event *event, data_source_handle_t *handle, struct s_data_source_config *config);
int data_source_cleanup(data_source_handle_t handle);

//-----------------------------------------------------------------------------
#endif //#ifndef DATA_SOURCE_H
