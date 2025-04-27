#ifndef ALARM_SOUND_CONFIG_H
#define ALARM_SOUND_CONFIG_H
//----------------------------------------------------------------------------
#include <stddef.h>


//----------------------------------------------------------------------------
typedef int (*alarm_sound_judge_func_t)(void);
struct s_alarm_sound_service_config {
	unsigned int priority;
	unsigned int sound_index;
	alarm_sound_judge_func_t func; 
};

void get_alarm_sound_service_config(const struct s_alarm_sound_service_config ** config, size_t *num);
//----------------------------------------------------------------------------
#endif	//#ifndef ALARM_SOUND_CONFIG_H
