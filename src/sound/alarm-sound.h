#ifndef ALARM_SOUND_SERVICE_H
#define ALARM_SOUND_SERVICE_H
//----------------------------------------------------------------------------

/** data pool service handles */
struct s_alarm_sound_service;
typedef struct s_alarm_sound_service *alarm_sound_service_handle;

int alarm_sound_service_setup(alarm_sound_service_handle *handle);
int alarm_sound_service_cleanup(alarm_sound_service_handle handle);
//----------------------------------------------------------------------------
#endif	//#ifndef ALARM_SOUND_SERVICE_H
