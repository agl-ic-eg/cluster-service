/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	socketcan-data.h
 * @brief	socket can data handling
 */
#ifndef SOCKETCAN_DATA_H
#define SOCKETCAN_DATA_H
//-----------------------------------------------------------------------------
#include <stdint.h>

typedef int (*can_handler_func_t)(uint32_t can_id, uint8_t *payload, size_t data_length);

struct s_socketcan_data_handling {
	uint32_t can_id;
	can_handler_func_t handler;
};
typedef struct s_socketcan_data_handling socketcan_data_handling_t;


size_t socketcan_data_get_element_num(void);
socketcan_data_handling_t *socketcan_data_get_table(size_t index);
//-----------------------------------------------------------------------------
#endif //#ifndef SOCKETCAN_DATA_H
