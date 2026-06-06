#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <alsa/asoundlib.h>

#define SAMPLE_RATE 44100
#define NUM_DROPLETS 300
#define PI 3.14159265358979323846
#define ALSA_DEVICE "default"

// Physics Constants for Minnaert Formula
const double GAMMA = 1.4;       
const double P0 = 101325.0;     
const double RHO = 1000.0;      

// ============================================================================
// GLOBAL CONTROL VARIABLES
// ============================================================================
double DROPLET_SIZE_MIN = 0.001;  
double DROPLET_SIZE_MAX = 0.008;  

double FREQ_START_HZ    = 50.0;  
double FREQ_END_HZ      = 1200.0; 
double BUBBLE_RATE_HZ   = 45.0;   

// NEW VOLUME CONTROL: 1.0 is standard nominal level. 
// Increase to 2.5, 5.0, or higher to drive the engine into a heavy, thick texture.
double VOLUME           = 15.0;    
// ============================================================================

typedef struct {
    int active;
    double current_phase;       
    double age;                 
    double duration;            
    double amplitude;           
    double start_freq;          
    double end_freq;            
} Droplet;

Droplet droplets[NUM_DROPLETS];

double get_minnaert_freq(double radius) {
    if (radius <= 0.0001) radius = 0.0001; 
    return (1.0 / radius) * sqrt((3.0 * GAMMA * P0) / RHO) / (2.0 * PI);
}

void trigger_droplet() {
    for (int i = 0; i < NUM_DROPLETS; i++) {
        if (!droplets[i].active) {
            droplets[i].active = 1;
            droplets[i].age = 0.0;
            
            double size_range = DROPLET_SIZE_MAX - DROPLET_SIZE_MIN;
            double randomized_radius = DROPLET_SIZE_MIN + (((double)rand() / RAND_MAX) * size_range);
            
            double size_factor = DROPLET_SIZE_MIN / randomized_radius; 
            
            double micro_drift = (((double)rand() / RAND_MAX) - 0.5) * 40.0;
            droplets[i].start_freq = FREQ_START_HZ * size_factor + micro_drift;
            droplets[i].end_freq   = FREQ_END_HZ * size_factor;
            
            if (droplets[i].end_freq <= droplets[i].start_freq) {
                droplets[i].end_freq = droplets[i].start_freq * 1.5;
            }

            droplets[i].duration = 0.04 + (randomized_radius * 25.0) + (((double)rand() / RAND_MAX) * 0.04);
            droplets[i].current_phase = 0.0; 
            
            droplets[i].amplitude = 0.005 + ((double)rand() / RAND_MAX) * 0.015; 
            return;
        }
    }
}

int main() {
    snd_pcm_t *pcm_handle;
    snd_pcm_hw_params_t *params;
    unsigned int rate = SAMPLE_RATE;
    int dir = 0;

    if (snd_pcm_open(&pcm_handle, ALSA_DEVICE, SND_PCM_STREAM_PLAYBACK, 0) < 0) {
        fprintf(stderr, "Error: Cannot open ALSA PCM device.\n");
        return -1;
    }

    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(pcm_handle, params);
    snd_pcm_hw_params_set_access(pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(pcm_handle, params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(pcm_handle, params, 1);
    snd_pcm_hw_params_set_rate_near(pcm_handle, params, &rate, &dir);

    snd_pcm_uframes_t buffer_size = 2048; 
    snd_pcm_uframes_t period_size = 512;  

    snd_pcm_hw_params_set_buffer_size_near(pcm_handle, params, &buffer_size);
    snd_pcm_hw_params_set_period_size_near(pcm_handle, params, &period_size, &dir);

    if (snd_pcm_hw_params(pcm_handle, params) < 0) {
        fprintf(stderr, "Error: Cannot set hardware parameters.\n");
        return -1;
    }

    srand(1337);
    for (int i = 0; i < NUM_DROPLETS; i++) {
        droplets[i].active = 0;
    }

    short *buffer = (short *)malloc(period_size * sizeof(short));
    if (buffer == NULL) {
        fprintf(stderr, "Memory allocation failed.\n");
        return -1;
    }

    printf("Water Engine Running. Master volume multiplier active.\n");

    double dt = 1.0 / SAMPLE_RATE;
    double sample_countdown = 0.0;

    while (1) {
        for (snd_pcm_uframes_t sample = 0; sample < period_size; sample++) {
            
            sample_countdown -= 1.0;
            if (sample_countdown <= 0.0) {
                trigger_droplet();
                sample_countdown = (double)SAMPLE_RATE / BUBBLE_RATE_HZ;
                sample_countdown += (((double)rand() / RAND_MAX) - 0.5) * (sample_countdown * 0.1);
            }

            double mixed_sample = 0.0;

            for (int i = 0; i < NUM_DROPLETS; i++) {
                if (!droplets[i].active) continue;

                droplets[i].age += dt;
                if (droplets[i].age >= droplets[i].duration) {
                    droplets[i].active = 0;
                    continue;
                }

                double lifecycle_progress = droplets[i].age / droplets[i].duration;
                double envelope = sin(lifecycle_progress * PI);
                
                double current_freq = droplets[i].start_freq + 
                    (droplets[i].end_freq - droplets[i].start_freq) * lifecycle_progress;

                droplets[i].current_phase += 2.0 * PI * current_freq * dt;

                mixed_sample += sin(droplets[i].current_phase) * envelope * droplets[i].amplitude;
            }

            // Apply Master Volume Multiplier
            mixed_sample *= VOLUME;

            // Soft saturation limiter: tanh smoothly compresses loud signals 
            // instead of hard-clipping them at 1.0. This keeps over-driven states musical.
            mixed_sample = tanh(mixed_sample);

            buffer[sample] = (short)(mixed_sample * 32767.0);
        }

        snd_pcm_sframes_t frames = snd_pcm_writei(pcm_handle, buffer, period_size);
        if (frames < 0) {
            snd_pcm_prepare(pcm_handle); 
        }
    }

    free(buffer);
    snd_pcm_drain(pcm_handle);
    snd_pcm_close(pcm_handle);
    return 0;
}
