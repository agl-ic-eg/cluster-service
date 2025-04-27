#include <stdio.h>
#include "socketcan-data.h"
#include "data-pool.h"

static int can_handler_180(uint32_t can_id, uint8_t *payload, size_t data_length)
{
	//SG_ MotorRPM : 32|16@1- (0.5,0) [-16000|16000] "RPM"  EVPWR
	{
		int32_t tmp = 0;
		tmp = (((int32_t)payload[5] & 0x7fU) * 256) + ((int32_t)payload[4]);
		if ((payload[5] & 0x80U) != 0) {
			tmp = tmp * -1;
		}

		uint32_t val = 0;
		if (tmp < 0) {
			val = 0U;
		} else if ((tmp / 2) > 20000) {
			val = 20000U;
		} else {
			val = (uint32_t)(tmp / 2);
		}
		data_pool_set_tacho_analog_val(val);
	}	
	
	//SG_ Gear : 12|3@1+ (1,0) [0|7] "" EVPWR
	//VAL_ 180 Gear 7 "GEAR_SNA" 5 "GEAR_B" 4 "GEAR_D" 3 "GEAR_N" 2 "GEAR_R" 1 "GEAR_P" 0 "GEAR_INVALID" ;
	{
		uint8_t tmp = 0;
		tmp = (payload[1] >> 4) & 0x07U;

		IC_HMI_GEAR_AT_VAL gear_val = IC_HMI_AT_UNUSED;
		if (tmp == 1) {
			gear_val = IC_HMI_AT_PARKING;
		} else if (tmp == 2) {
			gear_val = IC_HMI_AT_REVERSE;
		} else if (tmp == 3) {
			gear_val = IC_HMI_AT_NEUTRAL;
		} else if (tmp == 4) {
			gear_val = IC_HMI_AT_DRIVE;
		} else if (tmp == 5) {
			gear_val = IC_HMI_AT_BRAKE;
		} else {
			gear_val = IC_HMI_AT_UNUSED;
		}
		data_pool_set_gear_at_val(gear_val);
	}
	
	//SG_ BrakePedal : 15|1@1+ (1,0) [0|0] "" EVPWR
	
	//SG_ VehicleSpeed : 16|12@1+ (0.05,0) [0|204.75] "km/h" EVPWR
	{
		int32_t tmp = 0;
		tmp = (((int32_t)payload[3] & 0x0fU) * 256) + ((int32_t)payload[2]);

		uint32_t val = 0;
		if (tmp < 0) {
			val = 0U;
		} else if ((tmp / 2) > 30000) {
			val = 30000;
		} else {
			val = (uint32_t)(tmp * 5);
		}
		data_pool_set_speed_analog_val(val);
	}	

	return 0;
}

static int can_handler_142(uint32_t can_id, uint8_t *payload, size_t data_length)
{
	//SG_ FrontLeftDoor : 4|1@1+ (1,0) [0|0] ""  BCM
	{
		uint8_t tmp = payload[0] & 0x10U;

		if (tmp != 0) {
			data_pool_set_front_left_door(IC_HMI_ON);
			//fprintf(stderr, "data_pool_set_front_left_door(IC_HMI_ON)\n");
		} else {
			data_pool_set_front_left_door(IC_HMI_OFF);
			//fprintf(stderr, "data_pool_set_front_left_door(IC_HMI_OFF)\n");
		}
	}
	
	//SG_ FrontRightDoor : 5|1@1+ (1,0) [0|0] ""  BCM
	{
		uint8_t tmp = payload[0] & 0x20U;

		if (tmp != 0) {
			data_pool_set_front_right_door(IC_HMI_ON);
		} else {
			data_pool_set_front_right_door(IC_HMI_OFF);
		}
	}

	//SG_ RearRightDoor : 6|1@1+ (1,0) [0|0] ""  BCM
	{
		uint8_t tmp = payload[0] & 0x40U;

		if (tmp != 0) {
			data_pool_set_rear_right_door(IC_HMI_ON);
		} else {
			data_pool_set_rear_right_door(IC_HMI_OFF);
		}
	}

	//SG_ RearLeftDoor : 7|1@1+ (1,0) [0|0] ""  BCM
	{
		uint8_t tmp = payload[0] & 0x80U;

		if (tmp != 0) {
			data_pool_set_rear_left_door(IC_HMI_ON);
		} else {
			data_pool_set_rear_left_door(IC_HMI_OFF);
		}
	}

	//SG_ LeftSeatBelt : 12|1@1+ (1,0) [0|0] ""  BCM
	{
		uint8_t tmp = payload[1] & 0x10U;

		if (tmp != 0) {
			data_pool_set_front_left_seatbelt(IC_HMI_ON);
		} else {
			data_pool_set_front_left_seatbelt(IC_HMI_OFF);
		}
	}
	//SG_ RightSeatBelt : 13|1@1+ (1,0) [0|0] ""  BCM
	{
		uint8_t tmp = payload[1] & 0x20U;

		if (tmp != 0) {
			data_pool_set_front_right_seatbelt(IC_HMI_ON);
		} else {
			data_pool_set_front_right_seatbelt(IC_HMI_OFF);
		}
	}

	return 0;
}
/*
static int can_handler_180(uint32_t can_id, uint8_t *payload, size_t data_length)
{
	fprintf(stderr, "can id: 0x%03X [%d]", can_id, can_id);

	for(size_t i = 0; i < data_length; i++) {
		fprintf(stderr, " 0x%02X", payload[i]);
	}

	fprintf(stderr, "\n");

	return 0;
}
*/

static socketcan_data_handling_t socketcan_data_handling_table[] = {
	{
		.can_id = 180,
		.handler = can_handler_180,
	},
	{
		.can_id = 142,
		.handler = can_handler_142,
	},
};

size_t socketcan_data_get_element_num(void)
{
	return (sizeof(socketcan_data_handling_table) / sizeof(socketcan_data_handling_t));
}

socketcan_data_handling_t *socketcan_data_get_table(size_t index)
{
	socketcan_data_handling_t *ptr = NULL;

	if (index < socketcan_data_get_element_num()) {
		ptr = &socketcan_data_handling_table[index];
	}

	return ptr;
}