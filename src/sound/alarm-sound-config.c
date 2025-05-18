#include "alarm-sound-config.h"
#include <data-pool.h>

static int high_alarm(void)
{
	int result = 0;	// Don't need alarm sound.
	
	{	// Door and Seat belt alarm
		int32_t door = -1;
		int32_t seatbelt = -1;
		uint32_t speed_value = 0;
		const uint32_t speed_threshold = 1000;	//10km/h

		door = data_pool_get_door();
		seatbelt = data_pool_get_seatbelt();
		speed_value = data_pool_get_speed_analog_val();

		if (speed_value >= speed_threshold) {
			if ((seatbelt == IC_HMI_ON) || (door == IC_HMI_ON)) {
				result = 1;	// Need alarm sound.
			}
		}
	}

	{	// Engine, Brake, Airbag alarm
		int32_t engine = -1;
		int32_t brake = -1;
		int32_t airbag = -1;

		engine = data_pool_get_engine();
		brake = data_pool_get_brake();
		airbag = data_pool_get_srs_airbag();
		if ((engine == IC_HMI_ON) || (brake == IC_HMI_ON) 
			|| (airbag == IC_HMI_ON)) {
			result = 1;	// Need alarm sound.
		}
	}

	return result;
}

static int mid_alarm(void)
{
	int result = 0;	// Don't need alarm sound.
	
	{	// Engine, Brake, Airbag alarm
		int32_t abs = -1;
		int32_t eps = -1;
		int32_t esp = -1;

		abs = data_pool_get_abs();
		eps = data_pool_get_eps();
		esp = data_pool_get_esp_off();
		if ((abs == IC_HMI_ON) || (eps == IC_HMI_ON) 
			|| (esp == IC_HMI_ON)) {
			result = 1;	// Need alarm sound.
		}
	}

	return result;
}

static const struct s_alarm_sound_service_config alarm_sound_service_config[] = {
	[0] = {
		.priority = 100,
		.sound_index = 0,
		.func = high_alarm,
	},
	[1] = {
		.priority = 50,
		.sound_index = 1,
		.func = mid_alarm,
	},
};


void get_alarm_sound_service_config(const struct s_alarm_sound_service_config **config, size_t *num)
{
	(*num) = sizeof(alarm_sound_service_config) / sizeof(struct s_alarm_sound_service_config);
	(*config) = &alarm_sound_service_config[0];
}