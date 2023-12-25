#ifndef ALARM_SOUND_WORKER_H
#define ALARM_SOUND_WORKER_H
//----------------------------------------------------------------------------
#include <pthread.h>


typedef struct s_alarm_sound_worker {
	pthread_t worker_thread;
	int command;
} alarm_sound_worker_t;

#define ALARM_SOUND_WORKER_PLAY0	(0x10)
#define ALARM_SOUND_WORKER_PLAY1	(0x11)
#define ALARM_SOUND_WORKER_PLAY2	(0x12)
#define ALARM_SOUND_WORKER_STOP		(0x20)
#define ALARM_SOUND_WORKER_END		(0xa0)


int create_alarm_sound_worker(alarm_sound_worker_t **worker);
int release_alarm_sound_worker(alarm_sound_worker_t *worker);
int set_command_alarm_sound_worker(alarm_sound_worker_t *worker, int command);
//----------------------------------------------------------------------------
#endif	//#ifndef ALARM_SOUND_WORKER_H#define ALARM_SOUND_WORKER_PLAY0	(0x10)
