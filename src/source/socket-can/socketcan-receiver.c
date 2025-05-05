/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	data-pool-service.c
 * @brief	data service provider
 */

#include "data-pool.h"
#include "socketcan-receiver.h"
#include "socketcan-data.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <linux/can.h>
#include <linux/can/raw.h>

#include <stdio.h>

/** data pool client handles */
struct s_socketcan_client_sdevent {
	sd_event *parent_eventloop;	  /**< UNIX Domain socket event source for data pool service */
	sd_event_source *socket_evsource; /**< UNIX Domain socket event source for data pool service */
};
typedef struct s_socketcan_client_sdevent *socketcan_client_handle_sdevent;


static void do_can(struct can_frame *can_frame_data, socketcan_client_handle_sdevent scp)
{
	size_t element_num = 0;

	element_num = socketcan_data_get_element_num();

	for (size_t i=0; i < element_num; i++) {
		socketcan_data_handling_t *operation = socketcan_data_get_table(i);
		if (operation != NULL) {
			if (operation->can_id == (uint32_t)can_frame_data->can_id) {
				operation->handler((uint32_t)can_frame_data->can_id,
				can_frame_data->data,
				(size_t)can_frame_data->can_dlc);
			}
		}
	}
/*
	int telltale_on = 0;

	switch(can_frame_data->can_id) {
	case 0x168 :
		{
			if ((can_frame_data->data[0] & 0x10U) == 0) {
				// off
				data_pool_set_low_battery(IC_HMI_OFF);
			} else {
				// on
				data_pool_set_low_battery(IC_HMI_ON);
				(void) set_command_alarm_sound_worker(scp->alarm_sound, ALARM_SOUND_WORKER_PLAY0);
				telltale_on = 1;
			}
			break;
		}
	case 0x484 :
		{
			if ((can_frame_data->data[1] & 0x08U) == 0) {
				// off
				data_pool_set_engine(IC_HMI_OFF);
			} else {
				// on
				data_pool_set_engine(IC_HMI_ON);
				(void) set_command_alarm_sound_worker(scp->alarm_sound, ALARM_SOUND_WORKER_PLAY1);
				telltale_on = 1;
			}
			break;
		}
	case 0x232 :
		{
			if ((can_frame_data->data[0] & 0x40U) == 0) {
				// off
				data_pool_set_srs_airbag(IC_HMI_OFF);
			} else {
				// on
				data_pool_set_srs_airbag(IC_HMI_ON);
				(void) set_command_alarm_sound_worker(scp->alarm_sound, ALARM_SOUND_WORKER_PLAY2);
				telltale_on = 1;
			}
			break;
		}
	default:
		break;
	}

	if (telltale_on == 0) {
		(void) set_command_alarm_sound_worker(scp->alarm_sound, ALARM_SOUND_WORKER_STOP);
	}

	fprintf(stderr, "0x%03X [%d] \n", can_frame_data->can_id, can_frame_data->can_dlc);

	return;
*/
}


static int socketcan_receive(int fd, socketcan_client_handle_sdevent scp)
{
	struct can_frame can_frame_data;
	ssize_t ret = -1;

	(void) memset(&can_frame_data, 0, sizeof(can_frame_data));

	ret = read(fd, &can_frame_data, sizeof(can_frame_data));
	if (ret > 0) {
		do_can(&can_frame_data, scp);
	}

	return 0;
}
/**
 * Event handler for server session socket
 *
 * @param [in]	event		Socket event source object
 * @param [in]	fd			File descriptor for socket session
 * @param [in]	revents		Active event (epooll)socketcan_receive
 * @param [in]	userdata	Pointer to data_pool_service_handle
 * @return int	 0 success
 *				-1 internal error
 */
static int socketcan_sessions_handler(sd_event_source *event, int fd, uint32_t revents, void *userdata)
{
	socketcan_client_handle_sdevent scp = (socketcan_client_handle_sdevent) userdata;
	int ret = -1;

	if ((revents & (EPOLLHUP | EPOLLERR)) != 0) {
		// Disconnect session

		if (scp != NULL) {
			scp->socket_evsource = sd_event_source_disable_unref(scp->socket_evsource);
		} else {
			// Arg error or end of list or loop limit,
			// This event is not include session list. Fail safe it unref.
			sd_event_source_disable_unref(event);
		}
	} else if ((revents & EPOLLIN) != 0) {
		// Receive
		ret = socketcan_receive(fd, scp);
	}

	return ret;
}

/**
 * Function for data pool passenger setup
 *
 * @param [in]	event	sd event loop handle
 * @param [out]	handle	Return pointer for data pool service handle.
 * @return int	 0 success
 *				-1 internal error
 *				-2 argument error
 */
int socketcan_client_setup_sdevent(sd_event *event, socketcan_client_handle_sdevent *handle, const char *canif)
{
	sd_event_source *socket_source = NULL;
	struct sockaddr_can can_addr;
	struct ifreq can_ifr;
	struct s_socketcan_client_sdevent *scp = NULL;
	size_t canif_name_len = 0;
	int fd = -1;
	int ret = -1;
	int result = -1;

	if ((event == NULL) || (handle == NULL) || (canif == NULL)) {
		return -2;
	}

	scp = (struct s_socketcan_client_sdevent *) malloc(sizeof(struct s_socketcan_client_sdevent));
	if (scp == NULL) {
		result = -1;
		goto err_return;
	}

	memset(scp, 0, sizeof(*scp));

	scp->parent_eventloop = event;

	// Create client socket
	fd = socket(PF_CAN, (SOCK_RAW | SOCK_NONBLOCK), CAN_RAW);
	if (fd < 0) {
		result = -1;
		goto err_return;
	}

	memset(&can_ifr, 0, sizeof(can_ifr));
	canif_name_len = strnlen(canif, sizeof(can_ifr.ifr_name));
	(void) memcpy(can_ifr.ifr_name, canif, canif_name_len);
	ret = ioctl(fd, SIOCGIFINDEX, &can_ifr);
	if (ret < 0) {
		result = -1;
		goto err_return;
	}

	memset(&can_addr, 0, sizeof(can_addr));

	can_addr.can_family = AF_CAN;
	can_addr.can_ifindex = can_ifr.ifr_ifindex;

	ret = bind(fd, (struct sockaddr *)&can_addr, sizeof(can_addr));
	if (ret < 0) {
		result = -1;
		goto err_return;
	} // TODO EALREADY and EINTR

	ret = sd_event_add_io(event, &socket_source, fd, EPOLLIN, socketcan_sessions_handler, scp);
	if (ret < 0) {
		result = -1;
		goto err_return;
	}

	// Set automatically fd close at delete object.
	ret = sd_event_source_set_io_fd_own(socket_source, 1);
	if (ret < 0) {
		result = -1;
		goto err_return;
	}

	// After the automaticall fd close setting shall not close fd in error path
	fd = -1;

	scp->socket_evsource = socket_source;

	(*handle) = scp;

	return 0;

err_return:
	socket_source = sd_event_source_disable_unref(socket_source);
	free(scp);
	if (fd != -1)
		close(fd);

	return result;
}

/**
 * Function for data pool passenger cleanup
 *
 * @param [in]	handle	Return pointer to data pool service handle.
 * @return int	 0 success
 */
int socketcan_client_cleanup_sdevent(socketcan_client_handle_sdevent handle)
{
	struct s_socketcan_client_sdevent *scp = handle;

	// NULL through
	if (handle == NULL)
		return 0;

	(void) sd_event_source_disable_unref(scp->socket_evsource);

	free(scp);

	return 0;
}
