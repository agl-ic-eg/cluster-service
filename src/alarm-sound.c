#include <alsa/asoundlib.h>
#include <incbin.h>
#include <alarm-sound.h>
#include <pthread.h>
#include <time.h>


typedef struct s_sound_table {
	unsigned char *data_top;
	size_t	samples;
} sound_table_t;

static void *alarm_sound_worker_thread(void* arg);

static sound_table_t g_alarm_sound_table[3];

int create_alarm_sound_worker(alarm_sound_worker_t **worker)
{
	alarm_sound_worker_t *p = NULL;
	int ret = -1;

	g_alarm_sound_table[0].data_top = (unsigned char*)&incbin_alarm0_start[0];
	g_alarm_sound_table[0].samples = (size_t)(&incbin_alarm0_end[0] - &incbin_alarm0_start[0])/4;
	g_alarm_sound_table[1].data_top = (unsigned char*)&incbin_alarm1_start[0];
	g_alarm_sound_table[1].samples = (size_t)(&incbin_alarm1_end[0] - &incbin_alarm1_start[0])/4;
	g_alarm_sound_table[2].data_top = (unsigned char*)&incbin_alarm2_start[2];
	g_alarm_sound_table[2].samples = (size_t)(&incbin_alarm2_end[0] - &incbin_alarm2_start[0])/4;

	p = (alarm_sound_worker_t*)malloc(sizeof(alarm_sound_worker_t));
	if (p == NULL) {
		return -1;
	}

	memset(p, 0, sizeof(alarm_sound_worker_t));

	ret = pthread_create(&p->worker_thread, NULL, alarm_sound_worker_thread, p);
	if (ret < 0) {
		free(p);
		return -1;
	}

	(*worker) = p;

	return 0;
}

int set_command_alarm_sound_worker(alarm_sound_worker_t *worker, int command)
{
	if (worker == NULL)
		return -1;

	worker->command = command;

	return 0;
}


int release_alarm_sound_worker(alarm_sound_worker_t *worker)
{
	if (worker == NULL)
		return -1;

	worker->command = ALARM_SOUND_WORKER_END;

	return 0;
}


static char *device = "default";			/* playback device */

static __attribute__((aligned(16))) short g_dummy_buffer[2*48000/2/10] = {0};	//100ms silent buffer

static void *alarm_sound_worker_thread(void* arg)
{
	int ret = -1;
	unsigned int i;
	snd_pcm_t *handle = NULL;
	snd_pcm_sframes_t frames;
	alarm_sound_worker_t *worker = (alarm_sound_worker_t *)arg;
	int index = -1;

	do {
		ret = snd_pcm_open(&handle, device, SND_PCM_STREAM_PLAYBACK, 0);
		if (!(ret < 0))
			break;

		if (worker->command == ALARM_SOUND_WORKER_END) {
			goto return_func;
		}

		{
			// 10ms wait
			struct timespec wait_time = {.tv_sec = 0, .tv_nsec = 10 * 1000 * 1000};
			(void)clock_nanosleep(CLOCK_MONOTONIC, 0, &wait_time, NULL);
		}
	} while(1);

	ret = snd_pcm_set_params(handle, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED, 2, 48000, 1, 50000 );
	if (ret < 0) {
		fprintf(stderr, "Playback open error: %s\n", snd_strerror(ret));
		goto return_func;
	}

    while(1) {
		if (worker->command == ALARM_SOUND_WORKER_PLAY0) {
			index = 0;
		} else if (worker->command == ALARM_SOUND_WORKER_PLAY1) {
			index = 1;
		} else if (worker->command == ALARM_SOUND_WORKER_PLAY2) {
			index = 2;
		} else if (worker->command == ALARM_SOUND_WORKER_END) {
			break;
		} else {
			index = -1;
		}

		if (index >= 0 && index < 3) {
			frames = snd_pcm_writei(handle, g_alarm_sound_table[index].data_top, g_alarm_sound_table[index].samples );
			if (frames < 0)
				frames = snd_pcm_recover(handle, frames, 0);
			if (frames < 0) {
				fprintf(stderr, "snd_pcm_writei failed: %s\n", snd_strerror(frames));
				goto return_func;
			}
			if (frames > 0 && frames < g_alarm_sound_table[index].samples ) {
				fprintf(stderr, "Short write (expected %li, wrote %li)\n", g_alarm_sound_table[index].samples, frames);
			}
		} else {
			frames = snd_pcm_writei(handle, &g_dummy_buffer[0], sizeof(g_dummy_buffer)/4 );
			if (frames < 0)
				frames = snd_pcm_recover(handle, frames, 0);
			if (frames < 0) {
				fprintf(stderr, "snd_pcm_writei failed: %s\n", snd_strerror(frames));
				goto return_func;
			}
			if (frames > 0 && frames < sizeof(g_dummy_buffer)/4) {
				printf("Short write (expected %li, wrote %li)\n", sizeof(g_dummy_buffer)/4  , frames);
			}
		}
	}

	(void) snd_pcm_drain(handle);

return_func:
	if (handle != NULL)
		snd_pcm_close(handle);

	return 0;
}
