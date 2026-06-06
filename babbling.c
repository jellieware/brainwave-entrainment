#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <alsa/asoundlib.h>

#define SAMPLE_RATE 44100
#define BUFFER_SIZE 16384
#define MAX_BUBBLES 300

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct {
    double b0, b1, b2, b3, b4, b5, b6;
} PinkNoise;

typedef struct {
    double y1;
} LowPassFilter;

typedef struct {
    int active;
    double age;               
    double start_frequency;   
    double frequency_factor;  
    double amplitude;         
    double initial_amplitude; 
    double decay;             
} Bubble;

double generate_pink_noise(PinkNoise *p) {
    double white = ((double)rand() / RAND_MAX) * 2.0 - 1.0;
    p->b0 = 0.99886 * p->b0 + white * 0.0555179;
    p->b1 = 0.99332 * p->b1 + white * 0.0750759;
    p->b2 = 0.96900 * p->b2 + white * 0.1538520;
    p->b3 = 0.86650 * p->b3 + white * 0.3104856;
    p->b4 = 0.55000 * p->b4 + white * 0.5329522;
    p->b5 = -0.7616 * p->b5 - white * 0.0168980;
    double pink = p->b0 + p->b1 + p->b2 + p->b3 + p->b4 + p->b5 + p->b6 + white * 0.5362;
    p->b6 = white * 0.115926;
    return pink * 0.11;
}

double apply_low_pass(LowPassFilter *lp, double input, double cutoff) {
    lp->y1 = lp->y1 + cutoff * (input - lp->y1);
    return lp->y1;
}

// Transparent Soft Limiter to allow massive volume without digital clipping square-waves
double soft_limit(double x) {
    if (x > 0.8) {
        return 0.8 + (x - 0.8) / (1.0 + (x - 0.8) * (x - 0.8));
    } else if (x < -0.8) {
        return -0.8 + (x + 0.8) / (1.0 + (x + 0.8) * (x + 0.8));
    }
    return x;
}

int main() {
    int rc;
    snd_pcm_t *handle;
    snd_pcm_hw_params_t *params;
    unsigned int val = SAMPLE_RATE;
    int dir;
    snd_pcm_uframes_t frames = BUFFER_SIZE / 2; 
    short *buffer = malloc(BUFFER_SIZE * sizeof(short));

    rc = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (rc < 0) return 1;
    
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(handle, params);
    snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(handle, params, 2); 
    snd_pcm_hw_params_set_rate_near(handle, params, &val, &dir);
    snd_pcm_hw_params_set_period_size_near(handle, params, &frames, &dir);
    rc = snd_pcm_hw_params(handle, params);
    if (rc < 0) return 1;

    PinkNoise pink = {0};
    LowPassFilter lp = {0};
    Bubble bubbles[MAX_BUBBLES] = {0};

    const double spawn_chance = 0.005 * RAND_MAX;

    printf("Streaming: Synthetic Babbling Brook...\n");

    while (1) {
        for (int i = 0; i < BUFFER_SIZE; i += 2) {
            double noise = apply_low_pass(&lp, generate_pink_noise(&pink), 0.025) * 0.12;
            
            // 1. Stochastic Bubble Spawning
            if (rand() < spawn_chance) { 
                for (int b = 0; b < MAX_BUBBLES; b++) {
                    if (!bubbles[b].active) {
                        bubbles[b].active = 1;
                        bubbles[b].age = 0.0;
                        
                        bubbles[b].start_frequency = 50.0 + ((double)rand() / RAND_MAX) * 150.0;
                        bubbles[b].frequency_factor = 8.0 + ((double)rand() / RAND_MAX) * 12.0;
                        
                        bubbles[b].initial_amplitude = 0.01 + ((double)rand() / RAND_MAX) * 0.05;
                        bubbles[b].amplitude = 0.0; 
                        
                        // Maintained randomized decay setting centering around 16.0
                        bubbles[b].decay = 30.0 + ((double)rand() / RAND_MAX) * 6.0; 
                        
                        break;
                    }
                }
            }

            // 2. Audio Processing Engine
            double bubbles_mixed = 0.0;
            double dt = 1.0 / SAMPLE_RATE;

            for (int b = 0; b < MAX_BUBBLES; b++) {
                if (bubbles[b].active) {
                    bubbles[b].age += dt;
                    double t = bubbles[b].age;

                    double current_freq = bubbles[b].start_frequency * (1.0 + bubbles[b].frequency_factor * t);
                    
                    double attack_gain = 1.0;
                    if (t < 0.003) {
                        attack_gain = t / 0.003; 
                    }

                    double envelope = bubbles[b].initial_amplitude * exp(-bubbles[b].decay * t) * attack_gain;
                    
                    double phase = 2.0 * M_PI * current_freq * t;
                    bubbles_mixed += sin(phase) * envelope;

                    if (t > 0.5 || envelope < 0.00005) {
                        bubbles[b].active = 0;
                    }
                }
            }

            // 3. High-Volume Mastering Output Stage
            // Boosted the gain multiplier from 0.25 to 0.65 for increased loudness
            double mixed_signal = noise + bubbles_mixed;
            double master_out = soft_limit(mixed_signal * 3); 

            // Rigid boundary physical ceiling check
            if (master_out > 3.0) master_out = 3.0;
            if (master_out < -1.0) master_out = -1.0;

            short sample = (short)(master_out * 32767.0);
            buffer[i]     = sample; // Left 
            buffer[i + 1] = sample; // Right
        }

        rc = snd_pcm_writei(handle, buffer, frames);
        if (rc == -EPIPE) {
            snd_pcm_prepare(handle); 
        }
    }

    snd_pcm_drain(handle);
    snd_pcm_close(handle);
    free(buffer);
    return 0;
}
