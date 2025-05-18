/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	demo-data-generator.c
 * @brief	demo data generator for cluster service
 */

#include "demo-data-generator.h"
#include "data-pool.h"
//#include "alarm-sound.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct s_demo_data_timer {
	sd_event_source *timer_evsource; /**< Timer event source */
	uint64_t timerval;		 /**< Timer counter */
	uint64_t demo_count;		 /** demo event counter */
};

static struct s_demo_data_timer g_demo_timer;

const uint64_t g_demo_data_update_interval = 10 * 1000; // usec

#include "demo-data.c"

static int g_table_index = 0;
static int g_turn = 0;

#include "demo-data.c"

static int g_table_index = 0;
static int g_turn = 0;

static void do_demo(void)
{
	uint32_t speed_val = g_sim_data_tbl[g_table_index].speed;
	uint32_t rpm_val = g_sim_data_tbl[g_table_index].rpm;
	uint32_t gear_val = g_sim_data_tbl[g_table_index].gear;
	uint32_t turn_val = g_sim_data_tbl[g_table_index].turn;

	g_table_index = g_table_index + 1;
	if (g_table_index >= (sizeof(g_sim_data_tbl)/sizeof(g_sim_data_tbl[0]))) {
		g_table_index = 0;
		g_turn = (g_turn + 1) % 2;
	}

	data_pool_set_speed_analog_val(speed_val);
	data_pool_set_tacho_analog_val(rpm_val);
	if (gear_val == 0) {
		data_pool_set_gear_at_val(IC_HMI_AT_OFF);
	} else if (gear_val == 1) {
		data_pool_set_gear_at_val(IC_HMI_AT_PARKING);
	} else if (gear_val == 2) {
		data_pool_set_gear_at_val(IC_HMI_AT_REVERSE);
	} else if (gear_val == 3) {
		data_pool_set_gear_at_val(IC_HMI_AT_NEUTRAL);
	} else if (gear_val == 4) {
		data_pool_set_gear_at_val(IC_HMI_AT_DRIVE);
	} else if (gear_val == 5) {
		data_pool_set_gear_at_val(IC_HMI_AT_BRAKE);
	}


	if (g_turn == 0) {
		if (turn_val == 1)
			data_pool_set_turn_r(IC_HMI_ON);
		else
			data_pool_set_turn_r(IC_HMI_OFF);

		data_pool_set_turn_l(IC_HMI_OFF);
	} else {
		if (turn_val == 1)
			data_pool_set_turn_l(IC_HMI_ON);
		else
			data_pool_set_turn_l(IC_HMI_OFF);

		data_pool_set_turn_r(IC_HMI_OFF);
	}

	// trip
	{
		data_pool_set_trcom_trip_a_val(data_pool_get_trcom_trip_a_val() + 1);
	}

	// temp
	{
		if (((g_demo_timer.demo_count / 300) % 4) == 1) {
			if ((g_demo_timer.demo_count % 300) == 0) {
				int16_t temp_val = data_pool_get_o_temp_val();

				if (temp_val == 35)
					temp_val = 18;
				else
					temp_val = 35;

				data_pool_set_o_temp_val(temp_val);
			}
		}
	}

	// Telltale
	{
		if (((g_demo_timer.demo_count / 300) % 4) == 1) {
			if ((g_demo_timer.demo_count % 30) == 1) {
				if (data_pool_get_front_right_seatbelt() == IC_HMI_ON)
					data_pool_set_front_right_seatbelt(IC_HMI_OFF);
				else 
					data_pool_set_front_right_seatbelt(IC_HMI_ON);

				if (data_pool_get_front_left_seatbelt() == IC_HMI_ON)
					data_pool_set_front_left_seatbelt(IC_HMI_OFF);
				else
					data_pool_set_front_left_seatbelt(IC_HMI_ON);
			}
		}

		if (((g_demo_timer.demo_count / 500) % 5) == 0) {
			data_pool_set_brake(IC_HMI_ON);
			data_pool_set_front_right_door(IC_HMI_ON);
			data_pool_set_front_left_door(IC_HMI_ON);
			data_pool_set_rear_right_door(IC_HMI_ON);
			data_pool_set_rear_left_door(IC_HMI_ON);
			data_pool_set_trunk_door(IC_HMI_ON);
			data_pool_set_hood_door(IC_HMI_ON);
		} else {
			data_pool_set_brake(IC_HMI_OFF);
			data_pool_set_front_right_door(IC_HMI_OFF);
			data_pool_set_front_left_door(IC_HMI_OFF);
			data_pool_set_rear_right_door(IC_HMI_OFF);
			data_pool_set_rear_left_door(IC_HMI_OFF);
			data_pool_set_trunk_door(IC_HMI_OFF);
			data_pool_set_hood_door(IC_HMI_OFF);
		}

		if (((g_demo_timer.demo_count / 500) % 5) == 1) {
			data_pool_set_eps(IC_HMI_ON);
			data_pool_set_srs_airbag(IC_HMI_ON);
			data_pool_set_abs(IC_HMI_ON);
			data_pool_set_low_battery(IC_HMI_ON);
			data_pool_set_oil_press(IC_HMI_ON);
			data_pool_set_engine(IC_HMI_ON);
			data_pool_set_fuel(IC_HMI_ON);
			data_pool_set_immobi(IC_HMI_ON);
		} else {
			data_pool_set_eps(IC_HMI_OFF);
			data_pool_set_srs_airbag(IC_HMI_OFF);
			data_pool_set_abs(IC_HMI_OFF);
			data_pool_set_low_battery(IC_HMI_OFF);
			data_pool_set_oil_press(IC_HMI_OFF);
			data_pool_set_engine(IC_HMI_OFF);
			data_pool_set_fuel(IC_HMI_OFF);
			data_pool_set_immobi(IC_HMI_OFF);
		}

		if (((g_demo_timer.demo_count / 500) % 5) == 2) {
			data_pool_set_tm_fail(IC_HMI_ON);
			data_pool_set_esp_act(IC_HMI_ON);
			data_pool_set_esp_off(IC_HMI_ON);
			data_pool_set_adapting_lighting(IC_HMI_ON);
			data_pool_set_auto_stop(IC_HMI_ON);
			data_pool_set_auto_stop_fail(IC_HMI_ON);
			data_pool_set_parking_lights(IC_HMI_ON);
		} else {
			data_pool_set_tm_fail(IC_HMI_OFF);
			data_pool_set_esp_act(IC_HMI_OFF);
			data_pool_set_esp_off(IC_HMI_OFF);
			data_pool_set_adapting_lighting(IC_HMI_OFF);
			data_pool_set_auto_stop(IC_HMI_OFF);
			data_pool_set_auto_stop_fail(IC_HMI_OFF);
			data_pool_set_parking_lights(IC_HMI_OFF);
		}

		if (((g_demo_timer.demo_count / 500) % 5) == 3) {
			data_pool_set_front_fog(IC_HMI_ON);
			data_pool_set_exterior_light_fault(IC_HMI_ON);
			data_pool_set_acc_fail(IC_HMI_ON);
			data_pool_set_ldw_off(IC_HMI_ON);
			data_pool_set_hill_descent(IC_HMI_ON);
			data_pool_set_auto_hi_beam_green(IC_HMI_ON);
			data_pool_set_auto_hi_beam_amber(IC_HMI_ON);
			data_pool_set_sports_mode(IC_HMI_ON);
			data_pool_set_ldw_operate(IC_HMI_ON);
			data_pool_set_general_warn(IC_HMI_ON);
			data_pool_set_driving_power_mode(IC_HMI_ON);
			data_pool_set_hot_temp(IC_HMI_ON);
			data_pool_set_low_temp(IC_HMI_ON);

			if (((g_demo_timer.demo_count / 250) % 5) == 0)
				data_pool_set_high_beam(IC_HMI_ON);
			else
				data_pool_set_high_beam(IC_HMI_OFF);

		} else {
			data_pool_set_front_fog(IC_HMI_OFF);
			data_pool_set_exterior_light_fault(IC_HMI_OFF);
			data_pool_set_acc_fail(IC_HMI_OFF);
			data_pool_set_ldw_off(IC_HMI_OFF);
			data_pool_set_hill_descent(IC_HMI_OFF);
			data_pool_set_auto_hi_beam_green(IC_HMI_OFF);
			data_pool_set_auto_hi_beam_amber(IC_HMI_OFF);
			data_pool_set_sports_mode(IC_HMI_OFF);
			data_pool_set_ldw_operate(IC_HMI_OFF);
			data_pool_set_general_warn(IC_HMI_OFF);
			data_pool_set_driving_power_mode(IC_HMI_OFF);
			data_pool_set_hot_temp(IC_HMI_OFF);
			data_pool_set_low_temp(IC_HMI_OFF);
		}
	}

	g_demo_timer.demo_count++;
}
/**
 * TImer handler for data pool passenger
 *
 * @param [in]	es	sd event source
 * @param [in]	usec	callback time (MONOTONIC time)
 * @param [in]	userdata	Pointer to g_demo_timer
 * @return int	 0 success
 *				-1 internal error (timer stop)
 */
static int timer_handler(sd_event_source *es, uint64_t usec, void *userdata)
{
	int ret = -1;

	g_demo_timer.timerval = g_demo_timer.timerval + g_demo_data_update_interval;
	ret = sd_event_source_set_time(es, g_demo_timer.timerval);
	if (ret < 0) {
		return -1;
	}

	do_demo();

	return 0;
}

/**
 * Setup function for demo data generator
 *
 * @param [in]	event	sd event loop handle
 * @return int	 0 success
 *				-1 internal error
 *				-2 argument error
 */
int demo_data_generator_setup(sd_event *event)
{
	sd_event_source *timer_source = NULL;
	uint64_t timerval = 0;
	int ret = -1;

	if (event == NULL)
		return -2;

	// Notification timer setup
	ret = sd_event_now(event, CLOCK_MONOTONIC, &timerval);
	if (ret < 0) {
		ret = -1;
		goto err_return;
	}
	g_demo_timer.timerval = timerval + 1 * 1000 * 1000;
	ret = sd_event_add_time(
		event,
		&timer_source,
		CLOCK_MONOTONIC,
		g_demo_timer.timerval, // triger time (usec)
		1 * 1000,	       // accuracy (1000usec)
		timer_handler,
		&g_demo_timer);
	if (ret < 0) {
		ret = -1;
		goto err_return;
	}

	g_demo_timer.timer_evsource = timer_source;

	ret = sd_event_source_set_enabled(timer_source, SD_EVENT_ON);
	if (ret < 0) {
		ret = -1;
		goto err_return;
	}

	return 0;

err_return:
	timer_source = sd_event_source_disable_unref(timer_source);

	return ret;
}

/**
 * Cleanup function for demo data generator
 *
 * @param [in]	handle	Return pointer to data pool service handle.
 * @return int	 0 success
 */
int demo_data_generator_cleanup(void)
{
	(void) sd_event_source_disable_unref(g_demo_timer.timer_evsource);

	return 0;
}
