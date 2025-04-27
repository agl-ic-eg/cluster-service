#include <alsa/asoundlib.h>
#include "incbin.h"
#include "alarm-sound-config.h"
#include "alarm-sound.h"
#include <pthread.h>
#include <time.h>

#include <data-pool.h>

// Worker state
#define ALARM_SOUND_WORKER_CREATE		(0x10U)
#define ALARM_SOUND_WORKER_RUN			(0x80U)
#define ALARM_SOUND_WORKER_END			(0xa0U)

#define ALARM_SOUND_TABLES		(3)
typedef struct s_sound_table {
	unsigned char *data_top;
	size_t	samples;
} sound_table_t;
static sound_table_t g_alarm_sound_table[ALARM_SOUND_TABLES];

struct s_alarm_sound_service {
	uint32_t worker_state;
	pthread_t worker_thread;
};

static char *device = "default";			/* playback device */
static __attribute__((aligned(16))) short g_dummy_buffer[2*48000/2/10] = {0};	//100ms silent buffer
static void *alarm_sound_worker_thread(void* arg);

static void alarm_sound_table_setup(void)
{
	if (g_alarm_sound_table[0].data_top != NULL) {
		return;	//Already setup
	}

	g_alarm_sound_table[0].data_top = (unsigned char*)&incbin_alarm0_start[0];
	g_alarm_sound_table[0].samples = (size_t)(&incbin_alarm0_end[0] - &incbin_alarm0_start[0])/4;
	g_alarm_sound_table[1].data_top = (unsigned char*)&incbin_alarm1_start[0];
	g_alarm_sound_table[1].samples = (size_t)(&incbin_alarm1_end[0] - &incbin_alarm1_start[0])/4;
	g_alarm_sound_table[2].data_top = (unsigned char*)&incbin_alarm2_start[2];
	g_alarm_sound_table[2].samples = (size_t)(&incbin_alarm2_end[0] - &incbin_alarm2_start[0])/4;
}

static int alarm_sound_judge(void)
{
	int result = -1;
	unsigned int priority = 0;
	unsigned int sound_index = 0;
	const struct s_alarm_sound_service_config *config = NULL;
	size_t config_num = 0;

	get_alarm_sound_service_config(&config, &config_num);

	for(size_t i=0; i < config_num; i++) {
		int ret = config[i].func();
		if (ret == 1) {
			if (priority < config[i].priority) {
				priority = config[i].priority;
				sound_index = config[i].sound_index;
			}
		}
	}

	if (priority > 0) {
		if (sound_index < ALARM_SOUND_TABLES) {
			result = (int)sound_index;
		} else {
			result = 0;
		}
	}

	return result;
} 

static void alarm_wait(void)
{
	// 100ms wait
	struct timespec wait_time = {.tv_sec = 0, .tv_nsec = 100 * 1000 * 1000};
	(void)clock_nanosleep(CLOCK_MONOTONIC, 0, &wait_time, NULL);
}

static void *alarm_sound_worker_thread(void* arg)
{
	int ret = -1;
	snd_pcm_t *pcm_handle = NULL;
	alarm_sound_service_handle handle = (alarm_sound_service_handle)arg;

	do {
		ret = snd_pcm_open(&pcm_handle, device, SND_PCM_STREAM_PLAYBACK, 0);
		if (ret >= 0) {
			break;
		}

		if (handle->worker_state == ALARM_SOUND_WORKER_END) {
			goto return_func;
		}

		alarm_wait();
	} while(1);

	ret = snd_pcm_set_params(pcm_handle, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED, 2, 48000, 1, 50000 );
	if (ret < 0) {
		fprintf(stderr, "Playback open error: %s\n", snd_strerror(ret));
		goto return_func;
	}

	while(1) {
		snd_pcm_sframes_t frames;

		if (handle->worker_state == ALARM_SOUND_WORKER_END) {
			break;
		}

		int alarm_ops = alarm_sound_judge();

		if ((alarm_ops >= 0) && (alarm_ops < 3)) {
			frames = snd_pcm_writei(pcm_handle, g_alarm_sound_table[alarm_ops].data_top, g_alarm_sound_table[alarm_ops].samples );

			if (frames < 0) {
				int recover_ret = snd_pcm_recover(pcm_handle, frames, 0);
				if (recover_ret < 0) {
					alarm_wait();
				}	
			}
		} else {
			frames = snd_pcm_writei(pcm_handle, &g_dummy_buffer[0], sizeof(g_dummy_buffer)/4 );
			if (frames < 0) {
				int recover_ret = snd_pcm_recover(pcm_handle, frames, 0);
				if (recover_ret < 0) {
					alarm_wait();
				}	
			}
		}
	}

	(void) snd_pcm_drain(pcm_handle);

return_func:
	if (pcm_handle != NULL)
		snd_pcm_close(pcm_handle);

	return 0;
}

int alarm_sound_service_setup(alarm_sound_service_handle *handle)
{
	struct s_alarm_sound_service *p = NULL;
	int result = -1;
	int ret = -1;

	alarm_sound_table_setup();

	p = (struct s_alarm_sound_service*)malloc(sizeof(struct s_alarm_sound_service));
	if (p == NULL) {
		result = -1;
		goto error_return;
	}

	memset(p, 0, sizeof(struct s_alarm_sound_service));

	ret = pthread_create(&p->worker_thread, NULL, alarm_sound_worker_thread, p);
	if (ret < 0) {
		result = -2;
		goto error_return;
	}

	p->worker_state = ALARM_SOUND_WORKER_CREATE;

	(*handle) = (alarm_sound_service_handle)p;

	return 0;

error_return:
	(void) free(p);

	return result;
}

int alarm_sound_service_cleanup(alarm_sound_service_handle handle)
{
	int result = 0;
	void *val = NULL;

	if (handle == NULL) {
		result = -1;
		goto error_return;
	}
	if (handle->worker_state != ALARM_SOUND_WORKER_END ) {
		handle->worker_state = ALARM_SOUND_WORKER_END;
		pthread_join(handle->worker_thread, &val);	
	}

	(void) free(handle);

error_return:

	return result;
}
