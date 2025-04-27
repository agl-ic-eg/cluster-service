/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	cluster-service.h
 * @brief	cluster-service header
 */
#ifndef DATA_POOL_STATIC_CONFIGURATOR_H
#define DATA_POOL_STATIC_CONFIGURATOR_H
//-----------------------------------------------------------------------------
#include <stdint.h>


uint64_t get_data_pool_notification_interval(void);
uint64_t get_data_pool_fixed_interval_smoothing_sp_analog_val(void);
uint64_t get_data_pool_fixed_interval_smoothing_ta_analog_val(void);
int get_data_pool_service_session_limit(void);
int get_data_pool_service_socket_name(char *str, int len);
int get_data_pool_service_socket_name_type(void);

//-----------------------------------------------------------------------------
#endif //#ifndef DATA_POOL_STATIC_CONFIGURATOR_H
