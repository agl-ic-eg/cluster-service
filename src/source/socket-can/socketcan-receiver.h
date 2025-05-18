/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	socketcan-receiver.h
 * @brief	socket can receiver utility header
 */
#ifndef SOCKETCAN_RECEIVER_H
#define SOCKETCAN_RECEIVER_H
//-----------------------------------------------------------------------------
#include <stdint.h>
#include <systemd/sd-event.h>

#ifdef __cplusplus
extern "C" {
#endif
/** data pool service handles */
struct s_socketcan_client_sdevent;
typedef struct s_socketcan_client_sdevent *socketcan_client_handle_sdevent;

int socketcan_client_setup_sdevent(sd_event *event, socketcan_client_handle_sdevent *handle, const char *canif);
int socketcan_client_cleanup_sdevent(socketcan_client_handle_sdevent handle);
#ifdef __cplusplus
}
#endif
//-----------------------------------------------------------------------------
#endif //#ifndef SOCKETCAN_RECEIVER_H
