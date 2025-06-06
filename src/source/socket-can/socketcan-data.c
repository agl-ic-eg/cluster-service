#include <stdio.h>
#include "socketcan-data.h"
#include "data-pool.h"

static int can_handler_1001(uint32_t can_id, uint8_t *payload, size_t data_length)
{
	// SG_ PT_VehicleSpeed : 7|15@0+ (0.015625,0) [0|511.984375] "km/h" Vector_XXX
	{
		uint32_t tmp = 0;
		tmp = (((uint32_t)payload[0] & 0xffU) * 128) + (((uint32_t)payload[1] & 0xfeU) / 2);

		// 1 / (2^6) = 0.015625
		uint32_t val = (tmp * 100U) / 64U;
		if (tmp >= 0x7530U) {
			val = 0x7530U;
		}

		data_pool_set_speed_analog_val(val);
	}

	return 0;
}

static int can_handler_985(uint32_t can_id, uint8_t *payload, size_t data_length)
{
	// SG_ PT_FuelLevelLow : 55|1@1+ (1,0) [0|1] "" VCAR_SIGNAL
	{
		uint8_t tmp = 0;
		tmp = (payload[6] & 0x80U);
		if (tmp != 0) {
			data_pool_set_fuel(IC_HMI_ON);
		} else {
			data_pool_set_fuel(IC_HMI_OFF);
		}
	}

	// SG_ PT_EngineSpeed : 23|16@0+ (0.25,0) [0|0] "rpm" VCAR_SIGNAL
	{
		uint32_t tmp = 0;
		tmp = (((uint32_t)payload[2] & 0xffU) * 256U) + ((uint32_t)payload[3] & 0xffU);

		// 1 / (2^2) = 0.25
		uint32_t val = tmp / 4U;
		if (tmp >= 0x4E20U) {
			val = 0x4E20U;
		}

		data_pool_set_tacho_analog_val(val);
	}

	// SG_ PT_FuelLevelPct : 8|8@1+ (0.392157,0) [0|0] "%" VCAR_SIGNAL
	{
		/* nop */
	}

	return 0;
}

static int can_handler_180(uint32_t can_id, uint8_t *payload, size_t data_length)
{
	//SG_ EV_ShiftPosition : 12|3@0+ (1,0) [0|7] "" EVPWR
	//VAL_ 180 Gear 7 "GEAR_SNA" 5 "GEAR_B" 4 "GEAR_D" 3 "GEAR_N" 2 "GEAR_R" 1 "GEAR_P" 0 "GEAR_INVALID" ;
	{
		uint8_t tmp = 0;
		tmp = (payload[1] >> 2) & 0x07U;

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

	//SG_ EV_MotorFaultLamp : 1|1@0+ (1,0) [0|1] "" EVPWR
	{
		uint8_t tmp = 0;
		tmp = (payload[0] & 0x02U);
		if (tmp != 0) {
			data_pool_set_engine(IC_HMI_ON);
		} else {
			data_pool_set_engine(IC_HMI_OFF);
		}
	}

	return 0;
}

static int can_handler_184(uint32_t can_id, uint8_t *payload, size_t data_length)
{
	//  SG_ EspOffLamp : 31|1@0+ (1,0) [0|1] ""  ESP
	{
		uint8_t tmp = 0;
		tmp = (payload[3] & 0x80U);
		if (tmp != 0) {
			data_pool_set_esp_off(IC_HMI_ON);
		} else {
			data_pool_set_esp_off(IC_HMI_OFF);
		}
	}

	// SG_ AbsFaultLamp : 27|1@0+ (1,0) [0|1] ""  ESP
	{
		uint8_t tmp = 0;
		tmp = (payload[3] & 0x08U);
		if (tmp != 0) {
			data_pool_set_abs(IC_HMI_ON);
		} else {
			data_pool_set_abs(IC_HMI_OFF);
		}
	}

 	// SG_ EspActLamp : 6|1@0+ (1,0) [0|1] ""  ESP
	{
		uint8_t tmp = 0;
		tmp = (payload[0] & 0x40U);
		if (tmp != 0) {
			data_pool_set_esp_act(IC_HMI_ON);
		} else {
			data_pool_set_esp_act(IC_HMI_OFF);
		}
	}

	return 0;
}

static int can_handler_192(uint32_t can_id, uint8_t *payload, size_t data_length)
{
	//SG_ EpsFaultLamp : 6|1@1+ (1,0) [0|1] ""  EPS
	{
		uint8_t tmp = payload[0] & 0x40U;

		if (tmp != 0) {
			data_pool_set_eps(IC_HMI_ON);
		} else {
			data_pool_set_eps(IC_HMI_OFF);
		}
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
		} else {
			data_pool_set_front_left_door(IC_HMI_OFF);
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

static int can_handler_143(uint32_t can_id, uint8_t *payload, size_t data_length)
{
	//SG_ ImmobilizerFaultLamp : 4|1@1+ (1,0) [0|0] ""  BCM
	{
		uint8_t tmp = payload[0] & 0x10U;

		if (tmp != 0) {
			data_pool_set_immobi(IC_HMI_ON);
		} else {
			data_pool_set_immobi(IC_HMI_OFF);
		}
	}

	return 0;
}

static int can_handler_986(uint32_t can_id, uint8_t *payload, size_t data_length)
{
	//SG_ PT_HazardOn : 0|1@1+ (1,0) [0|1] "" Vector_XXX
	{
		/* nop */
	}

	//SG_ PT_LeftTurnOn : 1|1@1+ (1,0) [0|1] "" Vector_XXX
	{
		uint8_t tmp = payload[0] & 0x02U;

		if (tmp != 0) {
			data_pool_set_turn_l(IC_HMI_ON);
		} else {
			data_pool_set_turn_l(IC_HMI_OFF);
		}
	}

	//SG_ PT_RightTurnOn : 2|1@1+ (1,0) [0|1] "" Vector_XXX
	{
		uint8_t tmp = payload[0] & 0x04U;

		if (tmp != 0) {
			data_pool_set_turn_r(IC_HMI_ON);
		} else {
			data_pool_set_turn_r(IC_HMI_OFF);
		}
	}

	return 0;
}

static socketcan_data_handling_t socketcan_data_handling_table[] = {
	{
		.can_id = 1001,
		.handler = can_handler_1001,
	},
	{
		.can_id = 985,
		.handler = can_handler_985,
	},
	{
		.can_id = 986,
		.handler = can_handler_986,
	},
	{
		.can_id = 180,
		.handler = can_handler_180,
	},
	{
		.can_id = 184,
		.handler = can_handler_184,
	},
	{
		.can_id = 192,
		.handler = can_handler_192,
	},
	{
		.can_id = 142,
		.handler = can_handler_142,
	},
	{
		.can_id = 143,
		.handler = can_handler_143,
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