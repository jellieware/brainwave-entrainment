//author:alex terranova(2026)

#include <alsa/asoundlib.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
// --- HIGH-PASS FILTER VARIABLES (REMOVES BASS RUMBLE) ---
float hp_prev_in_l = 0.0f;
float hp_prev_out_l = 0.0f;
float hp_prev_in_r = 0.0f;
float hp_prev_out_r = 0.0f;

// The filter coefficient:
// 0.985f sets a tight low-cut roughly around 120Hz-150Hz.
// This completely carves out the deep sub-bass mud without losing clarity.
float hp_alpha = 0.982f;
// --------------------------------------------------------

// --- FLUID LIQUID INTEGRATORS (DIRECT AUDIO DAMPENING) ---
float fluid_history_l = 0.0f;
float fluid_history_r = 0.0f;

// Lower values make the water smoother and more fluid.
// Higher values let more of the individual bubble details through.
float fluid_smudge_factor = 0.012f;
// ---------------------------------------------------------
float out_l;
float out_r;
#define AUDIO_P_COUNT 256
int sample_counter = 0;
float cached_hydro_sample = 0.0f;
typedef struct {
  float pos; // Physical 1D coordinate displacement line
  float vel; // Acoustic particle velocity
  float prs; // Acoustic pressure deviation (Pascals)
} HydroAudioNode;

HydroAudioNode h_nodes[AUDIO_P_COUNT];
float h_time = 0.0f;

void init_hydro_audio() {
  for (int i = 0; i < AUDIO_P_COUNT; i++) {
    h_nodes[i].pos =
        (float)i * 0.002f; // Tightly packed physical spacing (2mm intervals)
    h_nodes[i].vel = h_nodes[i].prs = 0.0f;
  }
}
// Binaural Beat Carrier Configurations
// Left ear plays a solid base tone; Right ear plays the shifted target tone.
// 102.5 Hz minus 100.0 Hz creates a 2.5 Hz Delta Beat illusion in the brain.
double carrier_phase_left = 0.0;
double carrier_phase_right = 0.0;
double carrier_freq_left = 40.0;  // Base carrier pitch in Left Ear
double carrier_freq_right = 42.5; // Delta shifted pitch in Right Ear
double binaural_volume = 0.05;    // Mix level (0.05 = 5%). Keep it low to blend
// Gain & Headroom Weights (Boosts volume without clipping)
// Gain & Headroom Weights (Boosts volume without clipping)
// Gain & Mix Control (Boosts volume without clipping)
#define PI 3.14159265358979323846
#define GRID_SIZE 64
#define MAX_POLYPHONY 4
#define SLOW_LFO_SPEED 0.0012f

// Gain & Headroom Weights
const float MASTER_VOLUME_GAIN = 4.0f;
const float BASTON_WEIGHT = 0.60f;
const float SCHOLASTIC_WEIGHT = 0.40f;

// Polyphonic Droplet Voice Matrix
typedef struct {
  float age;
  float duration;
  float base_freq;
  float amp_envelope;
  int active;
} DropletVoice;

static DropletVoice drops_L[MAX_POLYPHONY];
static DropletVoice drops_R[MAX_POLYPHONY];

// Explicit Global Canvas Track Variables (Clears your undeclared variable
// error)
static uint32_t noise_state = 123456789;
static double total_elapsed_time = 0.0;
float current_grid_density = 0.0001f;
float final_base_frequency = 150.0f;
float long_term_duration_mod = 1.0f;
float macro_flow_surge = 1.0f;

#define SAMPLE_RATE 44100
#define PI 3.14159265358979323846
#define PCM_DEVICE "default"
#define BUFFER_FRAMES 2048
#define NUM_DROPLETS 300          // Targeted dense overlap array pool size
#define REVERB_DELAY_SAMPLES 6000 // Echo size buffer (~136ms)
float reverb_buffer_l[REVERB_DELAY_SAMPLES] = {0.0f};
float reverb_buffer_r[REVERB_DELAY_SAMPLES] = {0.0f};
int reverb_idx = 0;

// Acoustic space adjusters
const float feedback = 0.1f; // Decay size (0.1 = tiny room, 0.92 = huge cave)
const float dry_mix = 0.65f; // Volume of original untouched audio
const float wet_mix = 0.45f; // Volume of

// Global Master Loudness Control (0.0 to 1.0)
const double MASTER_VOLUME = 20;
double BUBBLE_RATE_HZ = 1.0;
double DROPLET_SIZE_MIN = 0.00005;
double DROPLET_SIZE_MAX = 0.0025;
typedef struct {
  int active;
  double current_phase;
  double age;
  double duration;
  double amplitude;

  // Explicit linear sweep variables
  double sweep_start;
  double sweep_end;
  double sweep_range;
  double sweep_factor;
  // Spatial positioning variables to distribute acoustic energy
  double pan_left;
  double pan_right;
} Droplet;

Droplet droplets[NUM_DROPLETS];

double rand_double(double min, double max) {
  return min + ((double)rand() / (double)RAND_MAX) * (max - min);
}

void trigger_droplet() {
  for (int i = 0; i < NUM_DROPLETS; i++) {
    if (!droplets[i].active) {
      droplets[i].active = 1;
      droplets[i].current_phase = 0.0;
      droplets[i].age = 0.0;

      double size_range = DROPLET_SIZE_MAX - DROPLET_SIZE_MIN;
      double randomized_radius =
          DROPLET_SIZE_MIN + (((double)rand() / RAND_MAX) * size_range);

      double size_factor = DROPLET_SIZE_MIN / randomized_radius;
      double micro_drift = (((double)rand() / RAND_MAX) - 0.5) * 40.0;
      // Varied short life cycles keeps the texture fluid rather than robotic
      droplets[i].duration = rand_double(0.06, 0.1);

      // Scale baseline per-droplet volume explicitly to prevent 100-wave
      // summation clipping
      droplets[i].amplitude =
          rand_double(0.3, 0.7) * (1.0 / (double)NUM_DROPLETS);

      // STRICT USER TARGET: Base pitch initializes between 50Hz and 150Hz
      droplets[i].sweep_start =
          rand_double(50, 1200.0) * size_factor + micro_drift;
      double sweep_end = droplets[i].sweep_start * rand_double(3.0, 6.0);
          droplets[i].sweep_factor = sweep_end / droplets[i].sweep_start;
      // Sweep target climbs swiftly away from bass rumble to create clean fluid
      // definition
      /*  droplets[i].sweep_end =
            droplets[i].sweep_start + rand_double(2000, 16500.0) * size_factor;
        droplets[i].sweep_range = droplets[i].sweep_end -
        droplets[i].sweep_start;
  */
      // Stereo Position assignment (0.0 = Far Left, 1.0 = Far Right)
      double pan = rand_double(0.0, 1.0);
      droplets[i].pan_left = sqrt(1.0 - pan); // Equal-power panning curve
      droplets[i].pan_right = sqrt(pan);
      break;
    }
  }
}
// Low-Pass Filter State (one for each stereo channel)
float lpf_state_l = 0.0f;
float lpf_state_r = 0.0f;

// Smoothing factor alpha (Value between 0.0 and 1.0)
// Closer to 0.0 = Lower cutoff frequency (muffled/deeper)
// Closer to 1.0 = Higher cutoff frequency (sharper/brighter)
const float LPF_ALPHA = 0.040f;
float step_sph_hydro_sample(float carrier_freq) {
  const float DT =
      1.0f / 44100.0f; // Exact time step for 44.1kHz audio sampling rate
  h_time += DT;

  // Node 0 acts as your underwater transducer sonar/hydrophone projection
  // source
  h_nodes[0].prs = 0.8f * sinf(2.0f * 3.1415926f * carrier_freq * h_time);

  // 1. Density continuity calculation translated to underwater acoustic
  // pressure
  for (int i = 1; i < AUDIO_P_COUNT; i++) {
    float local_accumulation = 0.0f;
    for (int j = 0; j < AUDIO_P_COUNT; j++) {
      float dist = fabsf(h_nodes[j].pos - h_nodes[i].pos);
      if (dist < 0.01f) { // 1cm smoothing kernel radius
        local_accumulation += (1.0f - (dist / 0.01f));
      }
    }
    // Hydroacoustic calculation: p = c^2 * delta_rho (Water speed constant
    // ~1500m/s scaled)
    h_nodes[i].prs = 22500.0f * (local_accumulation - 1.0f);
  }

  // 2. Momentum transformation (Pressure gradient accelerating the acoustic
  // fluid)
  for (int i = 1; i < AUDIO_P_COUNT; i++) {
    float force_gradient = 0.0f;
    for (int j = 0; j < AUDIO_P_COUNT; j++) {
      if (i == j)
        continue;
      float dist = fabsf(h_nodes[j].pos - h_nodes[i].pos);
      if (dist < 0.01f && dist > 0.0001f) {
        // Directional push based on neighbor pressure differences
        float sign = (h_nodes[j].pos > h_nodes[i].pos) ? 1.0f : -1.0f;
        force_gradient += -0.5f * (h_nodes[i].prs + h_nodes[j].prs) * sign;
      }
    }
 //   h_nodes[i].vel += DT * force_gradient;
 //   h_nodes[i].pos += DT * h_nodes[i].vel;
 // }
     h_nodes[ i]. vel += DT * force_gradient;

    // --- ADD THIS LINE TO INCREASE VISCOSITY ---
    h_nodes[ i]. vel -= DT * 4.5f * h_nodes[ i]. vel; 

    h_nodes[ i]. pos += DT * h_nodes[ i]. vel;
}


  // Return the pressure wave sample captured at the far end node of the array
  return h_nodes[AUDIO_P_COUNT - 1].prs;
}
int main() {
  srand((unsigned int)time(NULL));

  snd_pcm_t *pcm_handle;
  snd_pcm_hw_params_t *params;
  int err;

  if ((err = snd_pcm_open(&pcm_handle, PCM_DEVICE, SND_PCM_STREAM_PLAYBACK,
                          0)) < 0) {
    fprintf(stderr, "ERROR: Can't open \"%s\" PCM device: %s\n", PCM_DEVICE,
            snd_strerror(err));
    return 1;
  }

  snd_pcm_hw_params_alloca(&params);
  snd_pcm_hw_params_any(pcm_handle, params);
  snd_pcm_hw_params_set_access(pcm_handle, params,
                               SND_PCM_ACCESS_RW_INTERLEAVED);
  snd_pcm_hw_params_set_format(pcm_handle, params, SND_PCM_FORMAT_S16_LE);
  snd_pcm_hw_params_set_channels(pcm_handle, params,
                                 2); // Explicit stereo stream config

  unsigned int rate = SAMPLE_RATE;
  snd_pcm_hw_params_set_rate_near(pcm_handle, params, &rate, 0);

  if ((err = snd_pcm_hw_params(pcm_handle, params)) < 0) {
    fprintf(stderr, "ERROR: Can't set hardware parameters: %s\n",
            snd_strerror(err));
    snd_pcm_close(pcm_handle);
    return 1;
  }

  for (int i = 0; i < NUM_DROPLETS; i++) {
    droplets[i].active = 0;
  }

  int16_t buffer[BUFFER_FRAMES * 2];
  double speed_modifier = 1; // 1.0 is normal, 0.5 is half speed (slower)
  double dt = (1.0 / SAMPLE_RATE / BUBBLE_RATE_HZ) * speed_modifier;

  // double dt = 1.0 / SAMPLE_RATE / BUBBLE_RATE_HZ;

  printf("Now Streaming: Babbling Brook...\n");

  while (1) {
    for (int f = 0; f < BUFFER_FRAMES; f++) {
      if (rand_double(0.0, 100.0) < (1.5 * speed_modifier)) {
        trigger_droplet();
        
      }
      
      // Highly aggressive spawn rate to keep the 100-channel allocation engine
      // saturated
      // if (rand_double (0.0, 100.0) < 1.5)
      //  {
      //     trigger_droplet ();
      // }
      total_elapsed_time += (1.0 / 44100.0);

      // Dillon Baston 64-Grid Coordinate Vector Mechanics
      float grid_x = 0.5f + 0.25f * sin(total_elapsed_time * 0.12);
      float grid_y = 0.5f + 0.25f * cos(total_elapsed_time * 0.08);

      int cell_x = (int)(grid_x * (GRID_SIZE - 1));
      int cell_y = (int)(grid_y * (GRID_SIZE - 1));

      // Secondary 10-Minute Ultra-Slow Environmental Drift

      // float dynamic_master_gain = MASTER_VOLUME_GAIN * macro_flow_surge;

      float slow_env_drift = sinf(total_elapsed_time * SLOW_LFO_SPEED);
      macro_flow_surge = 0.825f + (slow_env_drift * 0.175f);
      float dynamic_base_pitch =
          45.0f + ((float)cell_y / (float)GRID_SIZE) * 70.0f;
      float final_base_frequency = dynamic_base_pitch +
                                   (slow_env_drift * 15.0f) +
                                   MASTER_VOLUME * macro_flow_surge;
      float long_term_duration_mod = 1.0f + (slow_env_drift * 0.35f);

      // ASSIGN MODIFIER: Sets the global variable to manage large, heavy bubble
      // spawning
      current_grid_density =
          0.00008f + ((float)cell_x / (float)GRID_SIZE) * 0.00018f;

      // ============================================================================
      // POLYPHONIC MATRIX SOUND GENERATION ENGINE
      // ============================================================================
      // Fast Pseudo-Random Generator (Xorshift)
      noise_state ^= (noise_state << 13);
      noise_state ^= (noise_state >> 17);
      noise_state ^= (noise_state << 5);
      float random_roll_L = (float)(noise_state & 0xFFFF) / 65535.0f;
      float random_roll_R = (float)((noise_state >> 16) & 0xFFFF) / 65535.0f;

      // Asynchronously schedule new bubbles into empty voice slots using the
      // grid density
      if (random_roll_L < current_grid_density) {
        for (int v = 0; v < MAX_POLYPHONY; v++) {
          if (!drops_L[v].active) {
            drops_L[v].active = 1;
            drops_L[v].age = 0.0f;
            drops_L[v].duration = 3000.0f + (random_roll_L * 4000.0f);
            drops_L[v].base_freq =
                current_grid_density + (random_roll_L * 100.0f);
            drops_L[v].amp_envelope = 0.1f + (random_roll_L * 0.4f);
            break;
          }
        }
      }
      if (random_roll_R < current_grid_density) {
        for (int v = 0; v < MAX_POLYPHONY; v++) {
          if (!drops_R[v].active) {
            drops_R[v].active = 1;
            drops_R[v].age = 0.0f;
            drops_R[v].duration = 3000.0f + (random_roll_R * 4000.0f);
            drops_R[v].base_freq =
                current_grid_density + (random_roll_R * 100.0f);
            drops_R[v].amp_envelope = 0.1f + (random_roll_R * 0.4f);
            break;
          }
        }
      }

      // Sum overlapping polyphonic voices smoothly
      float scholastic_water_L = 0.0f;
      float scholastic_water_R = 0.0f;

      for (int v = 0; v < MAX_POLYPHONY; v++) {
        if (drops_L[v].active) {
          drops_L[v].age += 1.0f;
          float progress = drops_L[v].age / drops_L[v].duration;

          float window = sinf(3.14159265f * progress) * (1.0f - progress);
          float local_amp = drops_L[v].amp_envelope * window;

          float dynamic_freq =
              drops_L[v].base_freq * (1.0f + (progress * progress * 0.4f));
          scholastic_water_L +=
              sinf(drops_L[v].age *
                   (2.0f * 3.14159265f * dynamic_freq / 44100.0f)) *
              local_amp;

          if (drops_L[v].age >= drops_L[v].duration)
            drops_L[v].active = 0;
        }

        if (drops_R[v].active) {
          drops_R[v].age += 1.0f;
          float progress = drops_R[v].age / drops_R[v].duration;

          float window = sinf(3.14159265f * progress) * (1.0f - progress);
          float local_amp = drops_R[v].amp_envelope * window;

          float dynamic_freq =
              drops_R[v].base_freq * (1.0f + (progress * progress * 1.6f));
          scholastic_water_R +=
              sinf(drops_R[v].age *
                   (2.0f * 3.14159265f * dynamic_freq / 44100.0f)) *
              local_amp;

          if (drops_R[v].age >= drops_R[v].duration)
            drops_R[v].active = 0;
        }
      }

      // 3. Mix channels together inside a safe 32-bit workspace with Master
      // Gain applied
      int32_t amplified_L =
          (int32_t)(((sin(out_l) * BASTON_WEIGHT) +
                     (scholastic_water_L * SCHOLASTIC_WEIGHT)) *
                    32767.0f * MASTER_VOLUME_GAIN);
      int32_t amplified_R =
          (int32_t)(((sin(out_r) * BASTON_WEIGHT) +
                     (scholastic_water_R * SCHOLASTIC_WEIGHT)) *
                    32767.0f * MASTER_VOLUME_GAIN);

      // 4. Strict boundary ceiling clamp checks (Stops digital static
      // completely)
      if (amplified_L > 32767)
        amplified_L = 32767;
      if (amplified_L < -32768)
        amplified_L = -32768;
      if (amplified_R > 32767)
        amplified_R = 32767;
      if (amplified_R < -32768)
        amplified_R = -32768;

      //
      double carrier_left = sin(carrier_phase_left) * binaural_volume;
      double carrier_right = sin(carrier_phase_right) * binaural_volume;

      // Advance carrier phases for the binaural beat
      carrier_phase_left += (2.0 * M_PI * carrier_freq_left) / SAMPLE_RATE;
      carrier_phase_right += (2.0 * M_PI * carrier_freq_right) / SAMPLE_RATE;
      if (carrier_phase_left > 2.0 * M_PI)
        carrier_phase_left -= 2.0 * M_PI;
      if (carrier_phase_right > 2.0 * M_PI)
        carrier_phase_right -= 2.0 * M_PI;

      // Mix the binaural oscillators independently into your existing sound
      // Replace your old single mixed output with these two split channels:
      double mixed_left = carrier_left;
      double mixed_right = carrier_right;

      //   double mixed_left = 0.0;
      //  double mixed_right = 0.0;

      for (int i = 0; i < NUM_DROPLETS; i++) {
        if (droplets[i].active) {
          if (droplets[i].age >= droplets[i].duration) {
            droplets[i].active = 0;
            continue;
          }

          //    double progress = droplets[i].age / droplets[i].duration;

          // Smooth window curve clears out edge clicks and abrupt pops
          //     double envelope = sin(progress * PI);

          // Pure linear calculation prevents sub-bass lingering distortion
          //      double current_freq = droplets[i].sweep_start +
          //      (droplets[i].sweep_range * progress);

          
          double progress = droplets[i].age / droplets[i].duration;

          if (progress >= 1.0) {
            droplets[i].active = 0;
            continue;
          }

          // 1. EXPONENTIAL FREQUENCY SWEEP
          // Pitch accelerates upward drastically near the end
          double current_freq =
              droplets[i].sweep_start * pow(droplets[i].sweep_factor, progress);

          // 2. EXPONENTIAL VOLUME DECAY ENVELOPE
          // Sharp initial burst with a smooth, natural ringing tail
          double envelope =
              droplets[i].amplitude * exp(-5.0 * progress) * sin(PI * progress);

          // 2. Hollow watery envelope
          // double envelope = droplets[i].amplitude * sin(progress * PI) * (1.0
          // - progress);
          double sample_mono = envelope * sin(droplets[i].current_phase);

          // 3. Pressure wobble phase update
          double bubble_wobble =
              1.0 + 0.08 * sin(2.0 * PI * 80.0 * droplets[i].age);
          droplets[i].current_phase +=
              (2.0 * PI * current_freq * bubble_wobble) * dt;

          if (droplets[i].current_phase > 2.0 * PI) {
            droplets[i].current_phase =
                fmod(droplets[i].current_phase, 2.0 * PI);
          }

          // Generate sine wave sample
          //    double sample_mono = envelope * sin(droplets[i].current_phase);
          /*
                    // Update phase based on the accelerating frequency
                    droplets[i].current_phase += (2.0 * PI * current_freq) /
             SAMPLE_RATE; if (droplets[i].current_phase > 2.0 * PI) {
                      droplets[i].current_phase -= 2.0 * PI;
                    }

                    // Track strict phase continuity to ensure smooth
             transitions
                    //  droplets[ i]. current_phase += (2.0 * PI * current_freq)
             /
                    //  SAMPLE_RATE;
                    droplets[i].current_phase += 2.0 * PI * current_freq * dt;
                    if (droplets[i].current_phase > 2.0 * PI) {
                      droplets[i].current_phase =
                          fmod(droplets[i].current_phase, 2.0 * PI);
                    } */

          // Compute single mono raw sample contribution
          //    double sample_mono = sin(droplets[i].current_phase) * envelope *
          //    droplets[i].amplitude;

          // Route mono drop signal into wide stereo bus using assigned pan
          // coordinates
          mixed_left += sample_mono * droplets[i].pan_left * MASTER_VOLUME;
          mixed_right += sample_mono * droplets[i].pan_right * MASTER_VOLUME;

          droplets[i].age += dt;
        }
      }

      // Apply soft-saturation clipping protection along both distinct channels
      double sat_left = tanh(mixed_left) * MASTER_VOLUME * 0.03;
      double sat_right = tanh(mixed_right) * MASTER_VOLUME * 0.03;

      // 2. Pull older echo reflections out of memory arrays
      float history_l = reverb_buffer_l[reverb_idx];
      float history_r = reverb_buffer_r[reverb_idx];

      // 3. Store current audio back into the echo feedback loops
      reverb_buffer_l[reverb_idx] = mixed_left + (history_l * feedback);
      reverb_buffer_r[reverb_idx] = mixed_right + (history_r * feedback);

      // 4. Mix the dry audio stream with wet space reflections
      out_l = (mixed_left * dry_mix) + (history_l * wet_mix);
      out_r = (mixed_right * dry_mix) + (history_r * wet_mix);

      // 5. Advance the circular index counter forward
      reverb_idx = (reverb_idx + 1) % REVERB_DELAY_SAMPLES;

      // 6. Native hard clipping protection limiters
      if (out_l > 1.0f)
        out_l = 1.0f;
      if (out_l < -1.0f)
        out_l = -1.0f;
      if (out_r > 1.0f)
        out_r = 1.0f;
      if (out_r < -1.0f)
        out_r = -1.0f;

      // 7. Overwrite original array positions with reverberated sound

      // 2. Soft-clip the signals using a hyperbolic tangent function (tanh)
      // This naturally compresses the audio smoothly like a professional
      // limiter

      // Convert floating point coordinates to native PCM 16-bit sound card
      // boundaries
      // buffer[f * 2] = (int16_t) (sat_left * 32767.0);	// Left Channel
      // Index
      // buffer[f * 2 + 1] = (int16_t) (sat_right * 32767.0);	// Right Channel
      // Index
      // 1. Advance our master timeline clock single-sample by single-sample

      // 1. Progress the master timeline clock sample-by-sample

      lpf_state_l =
          lpf_state_l + LPF_ALPHA * ((float)amplified_L - lpf_state_l);
      lpf_state_r =
          lpf_state_r + LPF_ALPHA * ((float)amplified_R - lpf_state_r);

      // Convert back to integers
      amplified_L = (int32_t)lpf_state_l;
      amplified_R = (int32_t)lpf_state_r;
      // ==========================================

      float constant_background_roar =
          (((float)rand() / (float)RAND_MAX) * 2.0f) - 1.0f;

      // 5. Output clean raw interleaved binary streams to standard out
      //  int16_t out_sample;
      //    float constant_background_roar =
      //     (((float)rand() / (float)RAND_MAX) * 2.0f) - 1.0f;

      // Apply a rough bandpass filter to mask the noise or damp it heavily
      constant_background_roar *=
          0.03f; // Keep it low (3% volume) to act as a glue layer

      if (sample_counter++ >= 64) {
        cached_hydro_sample = step_sph_hydro_sample(150.0f); // Run SPH step
        sample_counter = 0;                                  // Reset counter
      }

   //   float hydro_sample = cached_hydro_sample;
float hydro_sample = cached_hydro_sample * 32767.0f * 0.5f; 
      // Hard limiter to safely prevent digital clipping distortion
      if (hydro_sample > 1.0f)
        hydro_sample = 1.0f;
      if (hydro_sample < -1.0f)
        hydro_sample = -1.0f;

      // 3. Write directly to your project's active output array type
      // If your code uses 16-bit integer streams (AUDIO_S16):
      //    audio_output_buffer[i] = (int16_t)(hydro_sample * 32767.0f);

      // --- INSERT THIS DIRECT BLOCK TO LIQUIDIZE THE SOUND ---

      // 1. Blend the current bubble sample into the historical audio track
      // This drags the individual sharp peaks horizontally, fusing them
      // together
      fluid_history_l = fluid_history_l +
                        fluid_smudge_factor * (amplified_L - fluid_history_l);
      fluid_history_r = fluid_history_r +
                        fluid_smudge_factor * (amplified_R - fluid_history_r);

      // 2. Assign the smoothly integrated liquid output back to your channels
      // We apply a small boost (1.8f) to perfectly restore any perceived volume
      // loss
      amplified_L = fluid_history_l * 1.8f;
      amplified_R = fluid_history_r * 1.8f;

      // -------------------------------------------------------

      // --- INSERT THIS HIGH-PASS BLOCK TO ELIMINATE THE BASS RUMBLE ---

      // 1. Left Channel High-Pass Filter Math
      float current_hp_l =
          hp_alpha * (hp_prev_out_l + amplified_L - hp_prev_in_l);
      hp_prev_in_l = amplified_L;
      hp_prev_out_l = current_hp_l;
      amplified_L = current_hp_l;

      // 2. Right Channel High-Pass Filter Math
      float current_hp_r =
          hp_alpha * (hp_prev_out_r + amplified_R - hp_prev_in_r);
      hp_prev_in_r = amplified_R;
      hp_prev_out_r = current_hp_r;
      amplified_R = current_hp_r;

      // -----------------------------------------------------------------

      buffer[f * 2] =
          (int16_t)(((amplified_L + constant_background_roar + hydro_sample)));
      buffer[f * 2 + 1] =
          (int16_t)(((amplified_R + constant_background_roar + hydro_sample)));
    }

    snd_pcm_sframes_t written =
        snd_pcm_writei(pcm_handle, buffer, BUFFER_FRAMES);
    if (written < 0) {
      written = snd_pcm_recover(pcm_handle, written, 0);
    }
    if (written < 0) {
      fprintf(stderr, "ERROR: Failed writing data to PCM device: %s\n",
              snd_strerror(written));
      break;
    }
  }

  snd_pcm_drain(pcm_handle);
  snd_pcm_close(pcm_handle);
  return 0;
}
