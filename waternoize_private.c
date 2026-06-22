// author:alex.terranova.2026
#include <alsa/asoundlib.h>
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <time.h>
#define SPH_WINDOW_SIZE 5
#define METALLIC_FIX_STAGES 4
#define BUFFER_LEN 1024
#define TWO_PI 6.283185307179586
#define MAX_BUBBLES 128
#define PI 3.14159265358979323846
#define GRID_SIZE 32
#define MAX_POLYPHONY 4
#define SLOW_LFO_SPEED 0.0012f
#define SAMPLE_RATE 44100
#define PCM_DEVICE "default"
#define BUFFER_FRAMES 4096
#define NUM_DROPLETS 128
#define REVERB_DELAY_SAMPLES 6000
#define SAMPLE_RATE 44100
#define STREAM_BUFFER_SIZE 2048 // loop1 wavemesh
static int blur_initializedx = 0;
// Physical Sound Mesh Dimensions (Balanced for real-time DSP stability)
#define MESH_ROWS 32
#define MESH_COLS 32

// Comfort volume scale (0.0f = muted, 1.0f = full loudness)
#define VOLUME_SCALE 5.00f

#define BUFFER_SIZE 512
#define SAMPLE_RATE 44100
#define CHANNELS 2
#define BUFFER_SIZEX 512  // midtones loop3
#define BUFFER_SIZEXZ 512 // crystalline loop4
#define MAX_BUBBLES 128
#define MAX_BUBBLESZ 128
static const float C_SPEED = 0.15f;
static const float DAMPING = 0.992f;

static float grid1[GRID_SIZE][GRID_SIZE];
static float grid2[GRID_SIZE][GRID_SIZE];
static float (*current_grid)[GRID_SIZE] = NULL;
static float (*previous_grid)[GRID_SIZE] = NULL;

// --- 4-STAGE STEREO DIFFUSER CASCADE MEMORY ---
// Prime numbers ensure frequencies don't phase-cancel cleanly, creating a rich
// blur
#define STAGE1_L 149
#define STAGE1_R 173
#define STAGE2_L 331
#define STAGE2_R 379
#define STAGE3_L 563
#define STAGE3_R 617
#define STAGE4_L 827
#define STAGE4_R 911

static float buf_1l[STAGE1_L], buf_1r[STAGE1_R];
static float buf_2l[STAGE2_L], buf_2r[STAGE2_R];
static float buf_3l[STAGE3_L], buf_3r[STAGE3_R];
static float buf_4l[STAGE4_L], buf_4r[STAGE4_R];

static int idx_1l = 0, idx_1r = 0;
static int idx_2l = 0, idx_2r = 0;
static int idx_3l = 0, idx_3r = 0;
static int idx_4l = 0, idx_4r = 0;

static const float FLUID_SPEEDZ = 15.0f;

typedef struct {
  int active;
  float phase;
  float amp;
  float decay;
  float pan;
  float base_radius;
  float age;
} CrystalBubblez;

static CrystalBubblez pools[MAX_BUBBLES];
static int active_indices[MAX_BUBBLES];
static int active_count = 0;
static snd_pcm_t *pcmz;

float minnaert_freqz(float r) {
  if (r < 0.0001f)
    return 20000.0f;
  return 3.0f / r;
}

void trigger_crystalx(float energy, float pan) {
  if (active_count >= MAX_BUBBLES)
    return;
  for (int i = 0; i < MAX_BUBBLES; i++) {
    if (!pools[i].active) {
      pools[i].active = 1;
      pools[i].phase = 0.0f;
      pools[i].age = 0.0f;
      pools[i].pan = pan;

      float r0 = 0.0006f + (((float)rand() / (float)RAND_MAX) * 0.015f);
      pools[i].base_radius =
          0.0002f + (((float)rand() / (float)RAND_MAX) * 0.0038f);
      pools[i].amp = energy * 0.06f;
      // Faster decay for "popping" sound
      pools[i].decay = 25.0f + (((float)rand() / (float)RAND_MAX) * 40.0f);

      active_indices[active_count] = i;
      active_count++;
      break;
    }
  }
}

// Highly intensive smudging function running 4 cascaded phase diffusers per
// channel
void apply_extreme_water_blur(float *l, float *r) {
  // Diffusion coefficient: 0.61f hits the sweet spot for heavy smearing without
  // ringing metallic resonances
  float k = 0.61f;
  float in_l = *l;
  float in_r = *r;
  float out, old;

  // --- LEFT CHANNEL CASCADE ---
  // Stage 1
  old = buf_1l[idx_1l];
  out = -k * in_l + old;
  buf_1l[idx_1l] = in_l + k * out;
  idx_1l = (idx_1l + 1) % STAGE1_L;
  in_l = out;
  // Stage 2
  old = buf_2l[idx_2l];
  out = -k * in_l + old;
  buf_2l[idx_2l] = in_l + k * out;
  idx_2l = (idx_2l + 1) % STAGE2_L;
  in_l = out;
  // Stage 3
  old = buf_3l[idx_3l];
  out = -k * in_l + old;
  buf_3l[idx_3l] = in_l + k * out;
  idx_3l = (idx_3l + 1) % STAGE3_L;
  in_l = out;
  // Stage 4
  old = buf_4l[idx_4l];
  out = -k * in_l + old;
  buf_4l[idx_4l] = in_l + k * out;
  idx_4l = (idx_4l + 1) % STAGE4_L;
  *l = out;

  // --- RIGHT CHANNEL CASCADE ---
  // Stage 1
  old = buf_1r[idx_1r];
  out = -k * in_r + old;
  buf_1r[idx_1r] = in_r + k * out;
  idx_1r = (idx_1r + 1) % STAGE1_R;
  in_r = out;
  // Stage 2
  old = buf_2r[idx_2r];
  out = -k * in_r + old;
  buf_2r[idx_2r] = in_r + k * out;
  idx_2r = (idx_2r + 1) % STAGE2_R;
  in_r = out;
  // Stage 3
  old = buf_3r[idx_3r];
  out = -k * in_r + old;
  buf_3r[idx_3r] = in_r + k * out;
  idx_3r = (idx_3r + 1) % STAGE3_R;
  in_r = out;
  // Stage 4
  old = buf_4r[idx_4r];
  out = -k * in_r + old;
  buf_4r[idx_4r] = in_r + k * out;
  idx_4r = (idx_4r + 1) % STAGE4_R;
  *r = out;
}

void render_samplesz(float *l, float *r) {
  *l = 0.0f;
  *r = 0.0f;

  for (int a = 0; a < active_count; a++) {
    int i = active_indices[a];

    pools[i].age += (1.0f / (float)SAMPLE_RATE) * FLUID_SPEEDZ;
    float current_radius = pools[i].base_radius - (pools[i].age * 0.0085f);
    float current_freq = minnaert_freqz(current_radius);
    pools[i].amp *= expf(-pools[i].decay * (1.0f / (float)SAMPLE_RATE));

    if (pools[i].amp < 0.0001f || current_freq > 19000.0f ||
        current_radius <= 0.0001f) {
      pools[i].active = 0;
      active_indices[a] = active_indices[active_count - 1];
      active_count--;
      a--;
      continue;
    }

    float delta = (2.0f * M_PI * current_freq) / (float)SAMPLE_RATE;
    pools[i].phase += delta;
    if (pools[i].phase > 2.0f * M_PI)
      pools[i].phase -= 2.0f * M_PI;

    float sample = sinf(pools[i].phase) * pools[i].amp;

    if (current_freq > 17500.0f) {
      float click_fade = (19000.0f - current_freq) / 1500.0f;
      sample *= (click_fade < 0.0f ? 0.0f : click_fade);
    }

    *l += sample * (1.0f - pools[i].pan);
    *r += sample * pools[i].pan;
  }

  if (active_count > 1) {
    float dynamic_scaler = 1.8f / sqrtf((float)active_count);
    if (dynamic_scaler < 1.0f) {
      *l *= dynamic_scaler;
      *r *= dynamic_scaler;
    }
  }
  *l *= 0.5f; // Scale left channel to 50% volume
  *r *= 0.5f; // Scale right channel to 50%
  *l = tanhf(*l);
  *r = tanhf(*r);
}

static const int BLUR_DELAYS[METALLIC_FIX_STAGES] = {1433, 1973, 2657, 3529};
typedef struct {
  float *buffer;
  int size;
  int idx;
  float lpf_state;
} OrganicBlurStage;
static OrganicBlurStage blur_L[METALLIC_FIX_STAGES];
static OrganicBlurStage blur_R[METALLIC_FIX_STAGES];
static float x1_L = 0.0f, x2_L = 0.0f, y1_L = 0.0f, y2_L = 0.0f;
static float x1_R = 0.0f, x2_R = 0.0f, y1_R = 0.0f, y2_R = 0.0f;
static int blur_initialized = 0;
void init_organic_blur_static_safe(void) {
  if (blur_initialized)
    return;
  for (int i = 0; i < METALLIC_FIX_STAGES; i++) {
    blur_L[i].size = BLUR_DELAYS[i];
    blur_L[i].buffer = (float *)malloc(BLUR_DELAYS[i] * sizeof(float));
    blur_R[i].size = BLUR_DELAYS[i];
    blur_R[i].buffer = (float *)malloc(BLUR_DELAYS[i] * sizeof(float));
    if (blur_L[i].buffer && blur_R[i].buffer) {
      memset(blur_L[i].buffer, 0, BLUR_DELAYS[i] * sizeof(float));
      memset(blur_R[i].buffer, 0, BLUR_DELAYS[i] * sizeof(float));
    }
    blur_L[i].idx = 0;
    blur_L[i].lpf_state = 0.0f;
    blur_R[i].idx = 0;
    blur_R[i].lpf_state = 0.0f;
  }
  x1_L = x2_L = y1_L = y2_L = 0.0f;
  x1_R = x2_R = y1_R = y2_R = 0.0f;
  blur_initialized = 1;
}
float process_organic_sample(float input, OrganicBlurStage *stages) {
  float sample = input;
  const float diffusion = 0.15f;
  const float damping = 0.25f;
  for (int i = 0; i < METALLIC_FIX_STAGES; i++) {
    if (!stages[i].buffer)
      return input;
    int idx = stages[i].idx;
    int size = stages[i].size;
    float delayed = stages[i].buffer[idx];
    stages[i].lpf_state += damping * (delayed - stages[i].lpf_state);
    delayed = stages[i].lpf_state;
    float output = -diffusion * sample + delayed;
    stages[i].buffer[idx] = sample + diffusion * delayed;
    stages[i].idx = (idx + 1) % size;
    sample = output;
  }
  return sample;
}
void blur_bubbles_engine(int16_t *buffer, int frames) {
  if (!blur_initialized) {
    init_organic_blur_static_safe();
  }
  const float ocean_wash_blend = 0.5f;
  const float stream_gain_limit = 5.0f;
  const float b0 = 0.046f, b1 = 0.0f, b2 = -0.046f;
  const float a1 = -1.890f, a2 = 0.907f;
  for (int f = 0; f < frames; f++) {
    float left_in = (float)buffer[f * 2];
    float right_in = (float)buffer[f * 2 + 1];
    float wet_l = process_organic_sample(left_in, blur_L);
    float wet_r = process_organic_sample(right_in, blur_R);
    float raw_mix_L =
        (left_in * (1.0f - ocean_wash_blend)) + (wet_l * ocean_wash_blend);
    float raw_mix_R =
        (right_in * (1.0f - ocean_wash_blend)) + (wet_r * ocean_wash_blend);
    float brook_L =
        b0 * raw_mix_L + b1 * x1_L + b2 * x2_L - a1 * y1_L - a2 * y2_L;
    x2_L = x1_L;
    x1_L = raw_mix_L;
    y2_L = y1_L;
    y1_L = brook_L;
    float brook_R =
        b0 * raw_mix_R + b1 * x1_R + b2 * x2_R - a1 * y1_R - a2 * y2_R;
    x2_R = x1_R;
    x1_R = raw_mix_R;
    y2_R = y1_R;
    y1_R = brook_R;
    float out_l = brook_L * stream_gain_limit;
    float out_r = brook_R * stream_gain_limit;
    if (out_l > 32767.0f)
      out_l = 32767.0f;
    if (out_l < -32768.0f)
      out_l = -32768.0f;
    if (out_r > 32767.0f)
      out_r = 32767.0f;
    if (out_r < -32768.0f)
      out_r = -32768.0f;
    buffer[f * 2] = (int16_t)out_l;
    buffer[f * 2 + 1] = (int16_t)out_r;
  }
}
// ============================================================================
// 🎛️ CORE SOUND ENGINE ENVIRONMENT VARIABLES
// ============================================================================
static const float FLUID_SPEED =
    1.0f; // Flow rate multiplier (0.1f = slow, 3.0f = rapid)

// Bubble Radius Bounds (in meters)
// - Sub-millimeter sizes (e.g., 0.0002f) produce high-pitched, crystalline
// chimes
// - Larger sizes (e.g., 0.005f) produce deep, hollow, dripping plop tones
static const float BUBBLE_SIZE_MIN = 0.001f; // Minimum bubble radius cutoff
static const float BUBBLE_SIZE_MAX = 0.029f; // Maximum bubble radius cutoff
// ============================================================================

typedef struct {
  int active;
  float phase, amp, decay, pan, base_radius, age;
} CrystalBubble;

static CrystalBubble pool[MAX_BUBBLES];
static snd_pcm_t *pcm;

float minnaert_freq(float r) {
  if (r <= 0.00001f)
    return 20000.0f;
  return (1.0f / (2.0f * (float)M_PI * r)) *
         sqrtf((3.0f * 1.4f * 101325.0f) / 1000.0f);
}

void trigger_crystal(float energy, float pan) {
  for (int i = 0; i < MAX_BUBBLES; i++) {
    if (!pool[i].active) {
      pool[i].active = 1;
      pool[i].phase = 0.0f;
      pool[i].pan = pan;
      pool[i].age = 0.0f;

      // Map the size parameters dynamically between the defined min and max
      // variables
      float size_range = BUBBLE_SIZE_MAX - BUBBLE_SIZE_MIN;
      pool[i].base_radius = BUBBLE_SIZE_MIN + ((1.0f - energy) * size_range);

      pool[i].amp = 0.20f * energy;
      pool[i].decay = 0.995f;
      break;
    }
  }
}

void render_samples(float *l, float *r) {
  *l = 0.0f;
  *r = 0.0f;
  float dt = 1.0f / SAMPLE_RATE;

  for (int i = 0; i < MAX_BUBBLES; i++) {
    if (pool[i].active) {
      float current_radius =
          pool[i].base_radius *
          (1.0f - 0.12f * logf(1.0f + (pool[i].age * FLUID_SPEED) * 40.0f));
      float current_freq = minnaert_freq(current_radius);

      float s = sinf(pool[i].phase) * pool[i].amp;
      *l += s * (1.0f - pool[i].pan);
      *r += s * pool[i].pan;

      pool[i].phase += 2.0f * (float)M_PI * current_freq * dt;
      pool[i].amp *= pool[i].decay;
      pool[i].age += dt;

      if (pool[i].amp < 0.0001f || current_freq > 19500.0f) {
        pool[i].active = 0;
      }
    }
  }

  if (*l > 0.95f)
    *l = 0.95f;
  else if (*l < -0.95f)
    *l = -0.95f;
  if (*r > 0.95f)
    *r = 0.95f;
  else if (*r < -0.95f)
    *r = -0.95f;
}

typedef struct {
  int keep_running;
} EngineControl;

// Ultra-fast Xorshift random generator to seed physical wave friction points
unsigned int xor_seed = 8675309U;
inline float fast_white_noise() {
  xor_seed ^= xor_seed << 13;
  xor_seed ^= xor_seed >> 17;
  xor_seed ^= xor_seed << 5;
  return ((float)xor_seed / (float)4294967295U) * 2.0f - 1.0f;
}

static void *run_waveguide_mesh_engine(void *arg) {
  EngineControl *ctrl = (EngineControl *)arg;
  snd_pcm_t *handle;
  snd_pcm_hw_params_t *params;
  int dir;
  snd_pcm_uframes_t frames = STREAM_BUFFER_SIZE;
  short audio_buffer[STREAM_BUFFER_SIZE];

  if (snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0) {
    fprintf(stderr, "ALSA error: Cannot open sound device.\n");
    ctrl->keep_running = 0;
    return NULL;
  }

  snd_pcm_hw_params_alloca(&params);
  snd_pcm_hw_params_any(handle, params);
  snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
  snd_pcm_hw_params_set_format(
      handle, params, SND_PCM_FORMAT_S16_LE); // Strict native 16-Bit S16_LE
  snd_pcm_hw_params_set_channels(handle, params, 1);

  unsigned int val = SAMPLE_RATE;
  snd_pcm_hw_params_set_rate_near(handle, params, &val, &dir);

  // Deep buffer margins prevent emulation thread starvation clicks
  unsigned int periods = 6;
  snd_pcm_hw_params_set_periods_near(handle, params, &periods, &dir);
  snd_pcm_uframes_t target_buf = STREAM_BUFFER_SIZE * 6;
  snd_pcm_hw_params_set_buffer_size_near(handle, params, &target_buf);
  snd_pcm_hw_params_set_period_size_near(handle, params, &frames, &dir);

  if (snd_pcm_hw_params(handle, params) < 0) {
    fprintf(stderr, "ALSA hardware parameters rejected.\n");
    ctrl->keep_running = 0;
    snd_pcm_close(handle);
    return NULL;
  }

  // 2D Digital Waveguide Mesh State Buffers
  float mesh_pos[MESH_ROWS][MESH_COLS] = {{0.0f}};
  float mesh_vel[MESH_ROWS][MESH_COLS] = {{0.0f}};

  // Spatial-Temporal Audio Blur Trackers
  float low_pass_blur = 0.0f;
  const float blur_alpha =
      0.05f; // Lower alpha creates an increasingly blurred, liquid tone

  while (ctrl->keep_running) {
    for (int i = 0; i < STREAM_BUFFER_SIZE; i++) {

      // 1. Stochastic Energy Injection: Push random friction impulses into the
      // mesh core This constantly triggers the physical waveguide junctions
      mesh_pos[MESH_ROWS / 2][MESH_COLS / 2] += fast_white_noise() * 0.12f;

      // 2. Compute 2D Digital Scattering Wave Equations across neighbors
      // Wave propagation speed constant (Courant stability criterion: c <= 0.5
      // for 2D grids)
      const float wave_speed_sq = 0.22f;
      const float viscosity_friction = 0.985f; // Absorbs harsh high transients

      for (int r = 1; r < MESH_ROWS - 1; r++) {
        for (int c = 1; c < MESH_COLS - 1; c++) {
          // Standard discrete 2D spatial acceleration matrix
          float spatial_acceleration =
              (mesh_pos[r + 1][c] + mesh_pos[r - 1][c] + mesh_pos[r][c + 1] +
               mesh_pos[r][c - 1] - 4.0f * mesh_pos[r][c]);

          float acceleration = spatial_acceleration * wave_speed_sq;

          // Integrate velocity updates and apply physical mesh friction
          mesh_vel[r][c] = (mesh_vel[r][c] + acceleration) * viscosity_friction;
        }
      }

      // 3. Update spatial mesh grid position boundaries
      for (int r = 1; r < MESH_ROWS - 1; r++) {
        for (int c = 1; c < MESH_COLS - 1; c++) {
          mesh_pos[r][c] += mesh_vel[r][c];
        }
      }

      // 4. Acoustic Spatial Selection: Sample multiple distinct areas of the
      // mesh grid This replicates an acoustic microphone tracking a large,
      // moving physical space
      float mesh_output_mix = (mesh_pos[MESH_ROWS / 4][MESH_COLS / 4] +
                               mesh_pos[3 * MESH_ROWS / 4][3 * MESH_COLS / 4] +
                               mesh_pos[MESH_ROWS / 2][MESH_COLS / 3]);

      // 5. Apply Exponential Moving Average (EMA) to blur the grid signals into
      // fluid motion
      low_pass_blur = (blur_alpha * mesh_output_mix) +
                      ((1.0f - blur_alpha) * low_pass_blur);

      // Safe normalize transformation
      float final_signal = low_pass_blur * 0.15f * VOLUME_SCALE;
      if (final_signal > 1.0f)
        final_signal = 1.0f;
      if (final_signal < -1.0f)
        final_signal = -1.0f;

      // Direct bit encoding to Native ALSA S16_LE
      audio_buffer[i] = (short)(final_signal * 30000.0f);
    }

    // Write the calculated physical audio frame straight to the device
    snd_pcm_sframes_t rc = snd_pcm_writei(handle, audio_buffer, frames);
    if (rc == -EPIPE) {
      snd_pcm_prepare(handle);
    } else if (rc < 0) {
      snd_pcm_recover(handle, rc, 0);
    }
  }

  snd_pcm_drain(handle);
  snd_pcm_close(handle);
  return NULL;
}

void play_waveguide_mesh_sound() {
  pthread_t thread;
  EngineControl ctrl = {.keep_running = 1};

  printf("Streaming via libasound... Press [ENTER] to stop.\n\n");

  if (pthread_create(&thread, NULL, run_waveguide_mesh_engine, &ctrl) != 0) {
    fprintf(stderr, "Thread initialization error.\n");
    return;
  }

  getchar();
  exit(1);
  ctrl.keep_running = 0;
  pthread_join(thread, NULL);
  printf("Sound mesh engine cleanly shut down.\n");
}

float sph_cubic_spline_kernel(float distance, float h) {
  float q = fabsf(distance) / h;
  if (q >= 1.0f)
    return 0.0f;
  float sigma = 2.0f / (3.0f * h);
  if (q < 0.5f) {
    return sigma * (1.0f - 6.0f * q * q + 6.0f * q * q * q);
  } else {
    return sigma * (2.0f * powf(1.0f - q, 3.0f));
  }
}

void smooth_bubbles_sph(int16_t *buffer, int frames, float smoothing_radius) {
  int total_samples = frames * 2;
  float *temp_buffer = (float *)malloc(total_samples * sizeof(float));
  if (temp_buffer == NULL)
    return;
  for (int i = 0; i < total_samples; i++) {
    temp_buffer[i] = (float)buffer[i];
  }
  for (int ch = 0; ch < 2; ch++) {
    for (int f = 0; f < frames; f++) {
      int current_idx = f * 2 + ch;
      float weighted_sum = 0.0f;
      float total_weight = 0.0f;
      for (int neighbor = -SPH_WINDOW_SIZE; neighbor <= SPH_WINDOW_SIZE;
           neighbor++) {
        int target_frame = f + neighbor;
        if (target_frame >= 0 && target_frame < frames) {
          int neighbor_idx = target_frame * 2 + ch;
          float distance = (float)neighbor;
          float weight = sph_cubic_spline_kernel(distance, smoothing_radius);
          weighted_sum += temp_buffer[neighbor_idx] * weight;
          total_weight += weight;
        }
      }
      if (total_weight > 0.0001f) {
        float smoothed_sample = weighted_sum / total_weight;
        float dither = (((float)rand() / (float)RAND_MAX) - 0.5f);
        smoothed_sample += dither;
        if (smoothed_sample > 32767.0f)
          smoothed_sample = 32767.0f;
        if (smoothed_sample < -32768.0f)
          smoothed_sample = -32768.0f;
        buffer[current_idx] = (int16_t)smoothed_sample;
      }
    }
  }
  free(temp_buffer);
}
double carrier_phase_left = 0.0;
double carrier_phase_right = 0.0;
double carrier_freq_left = 40.0;
double carrier_freq_right = 42.5;
double binaural_volume = 0.05;

const float MASTER_VOLUME_GAIN = 4.0f;
const float BASTON_WEIGHT = 0.60f;
const float SCHOLASTIC_WEIGHT = 0.40f;
typedef struct {
  float age;
  float duration;
  float base_freq;
  float amp_envelope;
  int active;
} DropletVoice;
static DropletVoice drops_L[MAX_POLYPHONY];
static DropletVoice drops_R[MAX_POLYPHONY];
static uint32_t noise_state = 123456789;
static double total_elapsed_time = 0.0;
static float current_grid_density = 0.0001f;
float reverb_buffer_l[REVERB_DELAY_SAMPLES] = {0.0f};
float reverb_buffer_r[REVERB_DELAY_SAMPLES] = {0.0f};
int reverb_idx = 0;
const float feedback = 0.1f;
const float dry_mix = 0.65f;
const float wet_mix = 0.45f;
const double MASTER_VOLUME = 45;
double BUBBLE_RATE_HZ = 1.0;
double DROPLET_SIZE_MIN = 0.000010;
double DROPLET_SIZE_MAX = 0.00025;
typedef struct {
  int active;
  double current_phase;
  double age;
  double duration;
  double amplitude;
  double sweep_start;
  double sweep_end;
  double sweep_range;
  double sweep_factor;
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
      droplets[i].duration = rand_double(0.06, 0.1);
      droplets[i].amplitude =
          rand_double(0.3, 0.7) * (1.0 / (double)NUM_DROPLETS);
      droplets[i].sweep_start =
          rand_double(50, 150.0) * size_factor + micro_drift;
      droplets[i].sweep_end =
          droplets[i].sweep_start + rand_double(2000, 16500.0) * size_factor;
      droplets[i].sweep_range = droplets[i].sweep_end - droplets[i].sweep_start;
      double pan = rand_double(0.0, 1.0);
      droplets[i].pan_left = sqrt(1.0 - pan);
      droplets[i].pan_right = sqrt(pan);
      break;
    }
  }
}

typedef struct {
  int active;
  float frequency;
  float phase;
  float amplitude;
  float decay;
  float pan;
} Bubblezzz;

static Bubblezzz bubble_poolz[MAX_BUBBLES];

void init_gridzzz(void) {
  for (int i = 0; i < GRID_SIZE; i++) {
    for (int j = 0; j < GRID_SIZE; j++) {
      grid1[i][j] = 0.0f;
      grid2[i][j] = 0.0f;
    }
  }
  current_grid = &grid1;
  previous_grid = &grid2;

  for (int i = 0; i < MAX_BUBBLES; i++) {
    bubble_poolz[i].active = 0;
  }
}

void trigger_minnaert_bubblezzz(float grid_energy, int x, int y) {
  for (int i = 0; i < MAX_BUBBLES; i++) {
    if (!bubble_poolz[i].active) {
      float radius = 0.0005f + ((1.0f - grid_energy) * 0.003f);
      bubble_poolz[i].active = 1;
      bubble_poolz[i].frequency = 3.26f / radius;
      bubble_poolz[i].phase = 0.0f;
      bubble_poolz[i].amplitude = grid_energy * 0.15f;
      bubble_poolz[i].decay =
          expf(-0.04f * bubble_poolz[i].frequency / SAMPLE_RATE);
      bubble_poolz[i].pan = (float)x / (float)GRID_SIZE;
      break;
    }
  }
}

void render_next_samplezzz(float *out_left, float *out_right) {
  if ((rand() % 1000) < 15) {
    int cx = 5 + rand() % (GRID_SIZE - 10);
    int cy = 5 + rand() % (GRID_SIZE - 10);
    current_grid[cx][cy] += 0.4f + ((float)rand() / (float)RAND_MAX) * 0.6f;
  }

  for (int i = 1; i < GRID_SIZE - 1; i++) {
    for (int j = 1; j < GRID_SIZE - 1; j++) {
      float neighbors = previous_grid[i - 1][j] + previous_grid[i + 1][j] +
                        previous_grid[i][j - 1] + previous_grid[i][j + 1];
      float next_val = (neighbors * C_SPEED) +
                       (previous_grid[i][j] * (2.0f - 4.0f * C_SPEED)) -
                       current_grid[i][j];
      next_val *= DAMPING;

      float velocity = fabsf(next_val - current_grid[i][j]);
      if (velocity > 0.15f && (rand() % 100) < 5) {
        trigger_minnaert_bubblezzz(velocity > 1.0f ? 1.0f : velocity, i, j);
      }
      current_grid[i][j] = next_val;
    }
  }

  float (*tmp)[GRID_SIZE] = current_grid;
  current_grid = previous_grid;
  previous_grid = tmp;

  *out_left = 0.0f;
  *out_right = 0.0f;
  for (int i = 0; i < MAX_BUBBLES; i++) {
    if (bubble_poolz[i].active) {
      float wave = sinf(bubble_poolz[i].phase) * bubble_poolz[i].amplitude;
      *out_left += wave * (1.0f - bubble_poolz[i].pan);
      *out_right += wave * bubble_poolz[i].pan;
      bubble_poolz[i].phase +=
          2.0f * (float)M_PI * bubble_poolz[i].frequency / SAMPLE_RATE;
      bubble_poolz[i].amplitude *= bubble_poolz[i].decay;
      if (bubble_poolz[i].amplitude < 0.001f)
        bubble_poolz[i].active = 0;
    }
  }
}

// Unified Sound Engine: Pass a float from 0.0 to 1.0 to set master volume level
void run_soundscape_enginezzz(snd_pcm_t *pcm_handle, float volume) {
  short buffer[BUFFER_SIZE * CHANNELS];
  int rc;

  printf("");

  while (1) {
    for (int i = 0; i < BUFFER_SIZE; i++) {
      float sample_l = 0.0f;
      float sample_r = 0.0f;

      render_next_samplezzz(&sample_l, &sample_r);

      // Apply the volume attenuation scaling variable
      sample_l *= volume;
      sample_r *= volume;

      if (sample_l > 1.0f)
        sample_l = 1.0f;
      if (sample_l < -1.0f)
        sample_l = -1.0f;
      if (sample_r > 1.0f)
        sample_r = 1.0f;
      if (sample_r < -1.0f)
        sample_r = -1.0f;

      buffer[i * 2] = (short)(sample_l * 32767.0f);
      buffer[i * 2 + 1] = (short)(sample_r * 32767.0f);
    }

    rc = snd_pcm_writei(pcm_handle, buffer, BUFFER_SIZE);
    if (rc == -EPIPE) {
      snd_pcm_prepare(pcm_handle);
    } else if (rc < 0) {
      fprintf(stderr, "Error writing to PCM device: %s\n", snd_strerror(rc));
    }
  }
}
void *loop_x(void *arg) {
  snd_pcm_t *pcm_handle;
  snd_pcm_hw_params_t *params;
  int rc;

  init_gridzzz();

  rc = snd_pcm_open(&pcm_handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
  if (rc < 0) {
    fprintf(stderr, "Cannot open PCM device: %s\n", snd_strerror(rc));
    return 0;
  }

  snd_pcm_hw_params_alloca(&params);
  snd_pcm_hw_params_any(pcm_handle, params);
  snd_pcm_hw_params_set_access(pcm_handle, params,
                               SND_PCM_ACCESS_RW_INTERLEAVED);
  snd_pcm_hw_params_set_format(pcm_handle, params, SND_PCM_FORMAT_S16_LE);
  snd_pcm_hw_params_set_channels(pcm_handle, params, CHANNELS);
  unsigned int val = SAMPLE_RATE;
  snd_pcm_hw_params_set_rate_near(pcm_handle, params, &val, 0);

  rc = snd_pcm_hw_params(pcm_handle, params);
  if (rc < 0) {
    fprintf(stderr, "Cannot set HW parameters: %s\n", snd_strerror(rc));
    return 0;
  }

  // --- EXACTLY ONE FUNCTION CALL IN MAIN WITH THE VOLUME ARGUEMENT (0.25f =
  // 25% Volume) ---
  run_soundscape_enginezzz(pcm_handle, 0.25f);

  snd_pcm_drain(pcm_handle);
  snd_pcm_close(pcm_handle);
  return 0;
}

#define SAMPLE_RATE 44100
#define PERIOD_SIZE                                                            \
  512 // Increased slightly to provide structural wake-up cushion
#define MAX_ACTIVE_BUBBLES 256


#include <math.h>
#include <stdlib.h>

#define SPH_AUDIO_PARTICLES 32
#define SPH_AUDIO_H 0.35f
#define SPH_AUDIO_RHO0 800.0f
#define SPH_AUDIO_K 1200.0f
#define SPH_AUDIO_DT 0.0002f 

// Physics tracking arrays
float s_pos[SPH_AUDIO_PARTICLES];
float s_vel[SPH_AUDIO_PARTICLES];
float s_rho[SPH_AUDIO_PARTICLES];
float s_pres[SPH_AUDIO_PARTICLES];
float s_mass = 1.0f;

// Audio tracking arrays (completely separated from physics rates)
float s_phase[SPH_AUDIO_PARTICLES];
float s_fixed_freq[SPH_AUDIO_PARTICLES]; // Locked frequency per bubble life to prevent pitch skyrocketing
float s_amp[SPH_AUDIO_PARTICLES];
float s_decay_rate[SPH_AUDIO_PARTICLES]; 

int s_audio_initialized = 0;

void init_sph_audio_engine() {
    for (int i = 0; i < SPH_AUDIO_PARTICLES; i++) {
        s_pos[i] = ((float)rand() / RAND_MAX) * 2.0f;
        s_vel[i] = (((float)rand() / RAND_MAX) * 2.0f - 1.0f) * 0.5f;
        s_rho[i] = SPH_AUDIO_RHO0;
        s_pres[i] = 0.0f;
        s_phase[i] = 0.0f;
        s_fixed_freq[i] = 200.0f;
        s_amp[i] = 0.0f;    
        s_decay_rate[i] = 0.995f; 
    }
    s_audio_initialized = 1;
}

void render_sph_smooth_to_buffer(float *buffer, unsigned int num_samples, float master_volume, float external_energy) {
    if (!s_audio_initialized) {
        init_sph_audio_engine();
    }

    float sample_rate = 44100.0f; 
    float inv_sample_rate = 1.0f / sample_rate;

    for (unsigned int s = 0; s < num_samples; s++) {
        
        // --- 1. Step SPH Physics Fields (Kept completely independent) ---
        if (s % 32 == 0) {
            // Update Densities
            for (int i = 0; i < SPH_AUDIO_PARTICLES; i++) {
                float density_sum = 0.0f;
                for (int j = 0; j < SPH_AUDIO_PARTICLES; j++) {
                    float dist = fabsf(s_pos[i] - s_pos[j]);
                    if (dist < SPH_AUDIO_H) {
                        float q = dist / SPH_AUDIO_H;
                        density_sum += s_mass * (2.0f / (3.0f * SPH_AUDIO_H)) * (1.0f - 1.5f * q * q + 0.75f * q * q * q);
                    }
                }
                s_rho[i] = (density_sum < 10.0f) ? 10.0f : (density_sum > 1200.0f ? 1200.0f : density_sum);
                s_pres[i] = SPH_AUDIO_K * (s_rho[i] - SPH_AUDIO_RHO0);
            }

            // Update Forces and Motion
            for (int i = 0; i < SPH_AUDIO_PARTICLES; i++) {
                float pressure_force = 0.0f;
                for (int j = 0; j < SPH_AUDIO_PARTICLES; j++) {
                    if (i == j) continue;
                    float dist = s_pos[i] - s_pos[j];
                    if (fabsf(dist) < SPH_AUDIO_H && fabsf(dist) > 1e-5f) {
                        float sign = (dist > 0.0f) ? 1.0f : -1.0f;
                        float grad_w = sign * (-12.0f / (SPH_AUDIO_H * SPH_AUDIO_H * SPH_AUDIO_H)) * powf(SPH_AUDIO_H - fabsf(dist), 2.0f);
                        pressure_force -= s_mass * ((s_pres[i] / (s_rho[i] * s_rho[i])) + (s_pres[j] / (s_rho[j] * s_rho[j]))) * grad_w;
                    }
                }
                
                float random_excitation = (((float)rand() / RAND_MAX) * 2.0f - 1.0f) * external_energy * 2.5f;
                float accel = (pressure_force / s_rho[i]) + random_excitation;
                
                s_vel[i] += accel * SPH_AUDIO_DT;
                s_vel[i] *= 0.90f; // High damping prevents rapid continuous re-triggering screech
                s_pos[i] += s_vel[i] * SPH_AUDIO_DT;

                int hit_boundary = 0;
                if (s_pos[i] < 0.0f) { s_pos[i] = 0.0f; s_vel[i] *= -0.1f; hit_boundary = 1; }
                if (s_pos[i] > 2.0f) { s_pos[i] = 2.0f; s_vel[i] *= -0.1f; hit_boundary = 1; }

                float speed = fabsf(s_vel[i]);
                
                // Trigger a clean bubble voice ONLY if the slot is completely dead/silent
                if ((hit_boundary || speed > 1.4f) && (s_amp[i] <= 0.00001f) && (rand() % 100 < 4)) {
                    float density_ratio = s_rho[i] / SPH_AUDIO_RHO0;
                    
                    // Simple, clean, mathematically locked frequency boundaries (120Hz to 380Hz)
                    s_fixed_freq[i] = 120.0f + (150.0f * density_ratio) + (((float)rand() / RAND_MAX) * 50.0f);
                    if (s_fixed_freq[i] > 380.0f) s_fixed_freq[i] = 380.0f;
                    if (s_fixed_freq[i] < 100.0f) s_fixed_freq[i] = 100.0f;

                    // Set micro-volume to keep accumulation silent and clean
                    s_amp[i] = 0.004f * master_volume; 
                    s_phase[i] = 0.0f; 
                    s_decay_rate[i] = 0.994f; // Clean, natural decay timeline
                }
            }
        }

        // --- 2. Pure, Static-Pitch Sample Rendering ---
        float mixed_sample = 0.0f;

        for (int i = 0; i < SPH_AUDIO_PARTICLES; i++) {
            if (s_amp[i] > 0.00001f) {
                // Pitch is completely stable—impossible to screech
                s_phase[i] += 2.0f * M_PI * s_fixed_freq[i] * inv_sample_rate;
                if (s_phase[i] >= 2.0f * M_PI) s_phase[i] -= 2.0f * M_PI;

                mixed_sample += sinf(s_phase[i]) * s_amp[i];

                // Smooth exponential dampening applied perfectly per-sample
                s_amp[i] *= s_decay_rate[i]; 
            }
        }

        // --- 3. Safety Clipping Limiter ---
        float final_out = buffer[s] + mixed_sample;
        if (final_out > 0.95f)  final_out = 0.95f;
        if (final_out < -0.95f) final_out = -0.95f;
        
        buffer[s] = final_out;
    }
}





typedef struct {
  int active;
  float current_time;
  float minnaert_freq;
  float decay_rate;
  float pitch_drift;
  float amplitude;
  int sample_delay;
} Bubbleyyy;

Bubbleyyy bubble_poolyyy[MAX_ACTIVE_BUBBLES];
float global_time = 0.0f;

float generate_roaring_fluid_noise(float t) {
  float white = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
  float wave_grid = (sinf(2.0f * M_PI * 7.0f * t) * 0.45f +
                     sinf(2.0f * M_PI * 19.0f * t) * 0.30f +
                     sinf(2.0f * M_PI * 43.0f * t) * 0.15f +
                     sinf(2.0f * M_PI * 110.0f * t) * 0.10f);
  return (wave_grid * 0.7f + white * 0.3f) * 0.12f;
}

void spawn_bounded_bubble(float volume, float speed, float min_radius,
                          float max_radius) {
  for (int i = 0; i < MAX_ACTIVE_BUBBLES; i++) {
    if (!bubble_poolyyy[i].active) {
      const float gamma = 1.4f;
      const float p_A = 101325.0f;
      const float rho = 1000.0f;

      float radius_range = max_radius - min_radius;
      float target_radius =
          min_radius + (((float)rand() / RAND_MAX) * radius_range);
      float speed_compression = 1.0f / (1.0f + (speed * 0.15f));
      float a = target_radius * speed_compression;
      if (a < 0.0001f)
        a = 0.0001f;

      //   float freq = (1.0f / (2.0f * M_PI * a)) * sqrtf((3.0f * gamma * p_A)
      //   / rho);
      //    bubble_poolyyy[i].minnaert_freq = freq + (((float)rand() / RAND_MAX)
      //    * 300.0f - 150.0f);

      float freq =
          (1.0f / (2.0f * M_PI * a)) * sqrtf((3.0f * gamma * p_A) / rho);
      bubble_poolyyy[i].minnaert_freq =
          freq + (((float)rand() / RAND_MAX) * 200.0f - 150.0f);

      // 1. Raise the physical frequency floor to step out of subwoofer range
      if (bubble_poolyyy[i].minnaert_freq < 150.0f)
        bubble_poolyyy[i].minnaert_freq = 150.0f;

      //  if (bubble_poolyyy[i].minnaert_freq < 50.0f)
      //  bubble_poolyyy[i].minnaert_freq = 50.0f;

      bubble_poolyyy[i].active = 1;
      bubble_poolyyy[i].current_time = 0.0f;

      float size_factor = a / 0.01f;
      bubble_poolyyy[i].decay_rate =
          (30.0f + ((float)rand() / RAND_MAX) * 60.0f) *
          (1.0f / (size_factor + 0.5f));
      bubble_poolyyy[i].pitch_drift =
          (0.6f + ((float)rand() / RAND_MAX) * 1.2f) *
          (1.0f / (size_factor + 0.2f));

      bubble_poolyyy[i].sample_delay = rand() % PERIOD_SIZE;
      // Change line 59 inside spawn_bounded_bubble()
      bubble_poolyyy[i].amplitude =
          (volume * 0.02f) * (0.2f + ((float)rand() / RAND_MAX) * 0.5f);
      float calculated_amp =
          (volume * 0.05f) * (0.2f + ((float)rand() / RAND_MAX) * 0.8f);

      // 3. Smoothly fade the volume down for any bubble whose frequency sits
      // below 350 Hz This leaves the signature smudge texture intact but strips
      // away the low-end pressure waves.
      if (bubble_poolyyy[i].minnaert_freq < 350.0f) {
        float attenuation = (bubble_poolyyy[i].minnaert_freq - 150.0f) / 200.0f;
        if (attenuation < 0.05f)
          attenuation = 0.05f; // Keep a tiny touch of texture impact
        calculated_amp *= attenuation;
      }

      bubble_poolyyy[i].amplitude = calculated_amp;
      break;
    }
  }
}

void generate_audio_frame(float *buffer, int num_samples, float volume,
                          int num_splashes, float speed, float min_r,
                          float max_r) {
  for (int i = 0; i < num_samples; i++)
    buffer[i] = 0.0f;
  float dt = 1.0f / 44100.0f;

  // Cascaded filtering structures
  static float last_bubble_sample[MAX_ACTIVE_BUBBLES] = {0.0f};
  static float last_bubble_sample_2[MAX_ACTIVE_BUBBLES] = {0.0f};

  int spawn_count = 3 + (num_splashes / 5);
  for (int b = 0; b < spawn_count; b++) {
    spawn_bounded_bubble(volume, speed, min_r, max_r);
  }

  for (int b = 0; b < MAX_ACTIVE_BUBBLES; b++) {
    if (!bubble_poolyyy[b].active)
      continue;

    for (int i = 0; i < num_samples; i++) {
      float t = bubble_poolyyy[b].current_time;
      float envelope = expf(-bubble_poolyyy[b].decay_rate * t);
      float current_freq = bubble_poolyyy[b].minnaert_freq *
                           (1.0f + bubble_poolyyy[b].pitch_drift * t);

      // 1. Calculate raw bubble waveform
      float raw_sample = sinf(2.0f * M_PI * current_freq * t) * envelope *
                         bubble_poolyyy[b].amplitude;

      // 2. Continuous smudge filtering layer
      float alpha = 0.05f;

      float stage1 =
          (alpha * raw_sample) + ((1.0f - alpha) * last_bubble_sample[b]);
      last_bubble_sample[b] = stage1;

      float smudged_sample =
          (alpha * stage1) + ((1.0f - alpha) * last_bubble_sample_2[b]);
      last_bubble_sample_2[b] = smudged_sample;

      // 3. Accumulate to the target streaming buffer
      float bubble_gain = 5.5f;

      // 3. Accumulate to the target streaming buffer with gain applied
      buffer[i] += smudged_sample * bubble_gain;

      bubble_poolyyy[b].current_time += dt;

      // FIXED: Removed the break statement that was corrupting frame bounds
      if (envelope < 0.001f) {
        bubble_poolyyy[b].active = 0;
        last_bubble_sample[b] = 0.0f;
        last_bubble_sample_2[b] = 0.0f;
      }
    }
    // At the bottom of your generate_audio_frame() function:
// This renders to the buffer cleanly without breaking or changing anything else


  }

  for (int i = 0; i < num_samples; i++) {
    // buffer[i] += generate_roaring_fluid_noise(global_time) * volume;
    global_time += 1.0f / SAMPLE_RATE;

    if (buffer[i] > 1.0f)
      buffer[i] = 1.0f;
    else if (buffer[i] < -1.0f)
      buffer[i] = -1.0f;
    else
      buffer[i] = buffer[i] - (1.0f / 3.0f) * powf(buffer[i], 3.0f);
  }
  render_sph_smooth_to_buffer(buffer, num_samples, volume, speed);
}

// ====================================================================
// PROCEDURAL COMPATIBLE INTERFACE: loop_one
// ====================================================================
void *loop_one(void *arg) {
  srand(time(NULL));
  int err;
  snd_pcm_t *handle;
  float buffer[PERIOD_SIZE];

  for (int i = 0; i < MAX_ACTIVE_BUBBLES; i++)
    bubble_poolyyy[i].active = 0;

  float min_splash_radius = 0.0019f;
  float max_splash_radius = 0.090f;
  float volume_var = 0.75f;
  int number_splashes_var = 90;
  float speed_var = 3.5f;

  printf("");

  // 1. Force open ALSA in STANDARD BLOCKING MODE (0).
  // Non-blocking mode lets the CPU idle, which triggers Android's freeze
  // mechanism. Standard blocking mode anchors this process directly to the
  // kernel's hardware DMA clock.
  if ((err = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0)) <
      0) {
    fprintf(stderr, "Playback device initialization failure: %s\n",
            snd_strerror(err));
    return 0;
  }

  // 2. CRITICAL CHANGE: Allocate a massive 1-second ring buffer cushion
  // (1,000,000 microseconds). When the app goes to the background, Android can
  // freeze the process for up to 800ms. A 1-second buffer ensures the soundcard
  // keeps playing pre-rendered audio during a freeze.
  if ((err = snd_pcm_set_params(handle, SND_PCM_FORMAT_FLOAT,
                                SND_PCM_ACCESS_RW_INTERLEAVED, 1, SAMPLE_RATE,
                                1, 1000000)) < 0) {
    fprintf(stderr, "ALSA setup configuration failure: %s\n",
            snd_strerror(err));
    snd_pcm_close(handle);
    return 0;
  }

  // 3. Completely Infinite Loop
  while (1) {
    // Calculate the fluid simulation frame
    generate_audio_frame(buffer, PERIOD_SIZE, volume_var, number_splashes_var,
                         speed_var, min_splash_radius, max_splash_radius);

    // This call blocks until the hardware demands data.
    // The hardware interrupt forces the Android kernel to wake our thread,
    // overriding the freeze.
    
    snd_pcm_sframes_t written = snd_pcm_writei(handle, buffer, PERIOD_SIZE);

    if (written < 0) {
      // Instantly catch underruns (EPIPE) if a background freeze exceeds 1
      // second
      written = snd_pcm_recover(handle, written, 0);
      if (written < 0) {
        // If a recovery fails, force prepare the stream to keep the audio
        // flowing infinitely
        snd_pcm_prepare(handle);
      }
    }
  }

  snd_pcm_drain(handle);
  snd_pcm_close(handle);
  return 0;
}

// ====================================================================
// EXAMPLE MAIN WRAPPER
// Demonstrates how to spawn loop_one alongside other background tasks
// ====================================================================

void *loop_three(void *arg) {
  if (snd_pcm_open(&pcm, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0) {
    fprintf(stderr, "Error: Could not open ALSA device.\n");
    return 0;
  }
  snd_pcm_set_params(pcm, SND_PCM_FORMAT_FLOAT_LE,
                     SND_PCM_ACCESS_RW_INTERLEAVED, 2, SAMPLE_RATE, 1, 50000);

  float buf[BUFFER_SIZEX * 2];

  printf("💧 Infinite Liquid Space Active...\n");

  int spawn_threshold = (int)(15.0f / FLUID_SPEED);
  if (spawn_threshold < 2)
    spawn_threshold = 2;

  while (1) {
    if (rand() % spawn_threshold == 0) {
      float random_pan = (float)rand() / (float)RAND_MAX;
      float random_energy = 0.6f + (((float)rand() / (float)RAND_MAX) * 0.4f);
      trigger_crystal(random_energy, random_pan);
    }

    for (int i = 0; i < BUFFER_SIZEX; i++) {
      render_samples(&buf[i * 2], &buf[i * 2 + 1]);
    }

    if (snd_pcm_writei(pcm, buf, BUFFER_SIZEX) < 0) {
      snd_pcm_prepare(pcm);
    }
  }

  snd_pcm_close(pcm);
  return 0;
}

// Function for the second while loop
void *loop_two(void *arg) {

  srand((unsigned int)time(NULL));
  snd_pcm_t *pcm_handle;
  snd_pcm_hw_params_t *params;
  int err;
  if ((err = snd_pcm_open(&pcm_handle, PCM_DEVICE, SND_PCM_STREAM_PLAYBACK,
                          0)) < 0) {
    fprintf(stderr, "ERROR: Can't open \"%s\" PCM device: %s\n", PCM_DEVICE,
            snd_strerror(err));
    return 0;
  }
  snd_pcm_hw_params_alloca(&params);
  snd_pcm_hw_params_any(pcm_handle, params);
  snd_pcm_hw_params_set_access(pcm_handle, params,
                               SND_PCM_ACCESS_RW_INTERLEAVED);
  snd_pcm_hw_params_set_format(pcm_handle, params, SND_PCM_FORMAT_S16_LE);
  snd_pcm_hw_params_set_channels(pcm_handle, params, 2);
  unsigned int rate = SAMPLE_RATE;
  snd_pcm_hw_params_set_rate_near(pcm_handle, params, &rate, 0);
  if ((err = snd_pcm_hw_params(pcm_handle, params)) < 0) {
    fprintf(stderr, "ERROR: Can't set hardware parameters: %s\n",
            snd_strerror(err));
    snd_pcm_close(pcm_handle);
    return 0;
  }
  for (int i = 0; i < NUM_DROPLETS; i++) {
    droplets[i].active = 0;
  }
  int16_t buffer[BUFFER_FRAMES * 2];
  double speed_modifier = 0.6;
  double dt = (1.0 / SAMPLE_RATE / BUBBLE_RATE_HZ) * speed_modifier;
  printf("");
  while (1) {
    for (int f = 0; f < BUFFER_FRAMES; f++) {

      if (rand_double(0.0, 100.0) < (1.5 * speed_modifier)) {
        trigger_droplet();
      }
      double carrier_left = sin(carrier_phase_left) * binaural_volume;
      double carrier_right = sin(carrier_phase_right) * binaural_volume;
      carrier_phase_left += (2.0 * M_PI * carrier_freq_left) / SAMPLE_RATE;
      carrier_phase_right += (2.0 * M_PI * carrier_freq_right) / SAMPLE_RATE;
      if (carrier_phase_left > 2.0 * M_PI)
        carrier_phase_left -= 2.0 * M_PI;
      if (carrier_phase_right > 2.0 * M_PI)
        carrier_phase_right -= 2.0 * M_PI;
      double mixed_left = carrier_left;
      double mixed_right = carrier_right;
      for (int i = 0; i < NUM_DROPLETS; i++) {
        if (droplets[i].active) {
          if (droplets[i].age >= droplets[i].duration) {
            droplets[i].active = 0;
            continue;
          }
          double sweep_end = droplets[i].sweep_start * rand_double(3.0, 6.0);
          droplets[i].sweep_factor = sweep_end / droplets[i].sweep_start;
          double progress = droplets[i].age / droplets[i].duration;
          if (progress >= 1.0) {
            droplets[i].active = 0;
            continue;
          }
          double current_freq =
              droplets[i].sweep_start * pow(droplets[i].sweep_factor, progress);
          double envelope =
              droplets[i].amplitude * exp(-5.0 * progress) * sin(PI * progress);
          double sample_mono = envelope * sin(droplets[i].current_phase);
          droplets[i].current_phase += (2.0 * PI * current_freq) / SAMPLE_RATE;
          if (droplets[i].current_phase > 2.0 * PI) {
            droplets[i].current_phase -= 2.0 * PI;
          }
          droplets[i].current_phase += 2.0 * PI * current_freq * dt;
          if (droplets[i].current_phase > 2.0 * PI) {
            droplets[i].current_phase =
                fmod(droplets[i].current_phase, 2.0 * PI);
          }
          mixed_left += sample_mono * droplets[i].pan_left * MASTER_VOLUME;
          mixed_right += sample_mono * droplets[i].pan_right * MASTER_VOLUME;
          droplets[i].age += dt;
        }
      }

      float history_l = reverb_buffer_l[reverb_idx];
      float history_r = reverb_buffer_r[reverb_idx];
      reverb_buffer_l[reverb_idx] = mixed_left + (history_l * feedback);
      reverb_buffer_r[reverb_idx] = mixed_right + (history_r * feedback);
      float out_l = (mixed_left * dry_mix) + (history_l * wet_mix);
      float out_r = (mixed_right * dry_mix) + (history_r * wet_mix);
      reverb_idx = (reverb_idx + 1) % REVERB_DELAY_SAMPLES;
      if (out_l > 1.0f)
        out_l = 1.0f;
      if (out_l < -1.0f)
        out_l = -1.0f;
      if (out_r > 1.0f)
        out_r = 1.0f;
      if (out_r < -1.0f)
        out_r = -1.0f;
      total_elapsed_time += (1.0 / 44100.0);
      float grid_x = 0.5f + 0.25f * sin(total_elapsed_time * 0.12);
      float grid_y = 0.5f + 0.25f * cos(total_elapsed_time * 0.08);
      int cell_x = (int)(grid_x * (GRID_SIZE - 1));
      int cell_y = (int)(grid_y * (GRID_SIZE - 1));
      float slow_env_drift = sinf(total_elapsed_time * SLOW_LFO_SPEED);
      float dynamic_base_pitch =
          45.0f + ((float)cell_y / (float)GRID_SIZE) * 70.0f;
      float final_base_frequency =
          dynamic_base_pitch + (slow_env_drift * 15.0f);
      float long_term_duration_mod = 1.0f + (slow_env_drift * 0.35f);
      current_grid_density =
          0.00008f + ((float)cell_x / (float)GRID_SIZE) * 0.00018f;
      noise_state ^= (noise_state << 13);
      noise_state ^= (noise_state >> 17);
      noise_state ^= (noise_state << 5);
      float random_roll_L = (float)(noise_state & 0xFFFF) / 65535.0f;
      float random_roll_R = (float)((noise_state >> 16) & 0xFFFF) / 65535.0f;
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
      int32_t amplified_L =
          (int32_t)(((sin(out_l) * BASTON_WEIGHT) +
                     (scholastic_water_L * SCHOLASTIC_WEIGHT)) *
                    32767.0f * MASTER_VOLUME_GAIN);
      int32_t amplified_R =
          (int32_t)(((sin(out_r) * BASTON_WEIGHT) +
                     (scholastic_water_R * SCHOLASTIC_WEIGHT)) *
                    32767.0f * MASTER_VOLUME_GAIN);
      if (amplified_L > 32767)
        amplified_L = 32767;
      if (amplified_L < -32768)
        amplified_L = -32768;
      if (amplified_R > 32767)
        amplified_R = 32767;
      if (amplified_R < -32768)
        amplified_R = -32768;

      buffer[f * 2] = (int16_t)(amplified_L);
      buffer[f * 2 + 1] = (int16_t)(amplified_R);
    }

    smooth_bubbles_sph(buffer, BUFFER_FRAMES, 1.0f);
    // blur_bubbles_engine(buffer, BUFFER_FRAMES);
    snd_pcm_sframes_t written =
        snd_pcm_writei(pcm_handle, buffer, BUFFER_FRAMES);

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

typedef struct {
  int active;
  float frequency;
  float phase;
  float amplitude;
  float decay;
  float pan;
} Bubblezx;

static Bubblezx bubble_pool[MAX_BUBBLES];

void init_grid(void) {
  for (int i = 0; i < GRID_SIZE; i++) {
    for (int j = 0; j < GRID_SIZE; j++) {
      grid1[i][j] = 0.0f;
      grid2[i][j] = 0.0f;
    }
  }
  current_grid = &grid1;
  previous_grid = &grid2;

  for (int i = 0; i < MAX_BUBBLES; i++) {
    bubble_pool[i].active = 0;
  }
}

void trigger_minnaert_bubble(float grid_energy, int x, int y) {
  for (int i = 0; i < MAX_BUBBLES; i++) {
    if (!bubble_pool[i].active) {
      float radius = 0.0005f + ((1.0f - grid_energy) * 0.003f);
      bubble_pool[i].active = 1;
      bubble_pool[i].frequency = 3.26f / radius;
      bubble_pool[i].phase = 0.0f;
      bubble_pool[i].amplitude = grid_energy * 0.15f;
      bubble_pool[i].decay =
          expf(-0.04f * bubble_pool[i].frequency / SAMPLE_RATE);
      bubble_pool[i].pan = (float)x / (float)GRID_SIZE;
      break;
    }
  }
}

void render_next_sample(float *out_left, float *out_right) {
  if ((rand() % 1000) < 15) {
    int cx = 5 + rand() % (GRID_SIZE - 10);
    int cy = 5 + rand() % (GRID_SIZE - 10);
    current_grid[cx][cy] += 0.4f + ((float)rand() / (float)RAND_MAX) * 0.6f;
  }

  for (int i = 1; i < GRID_SIZE - 1; i++) {
    for (int j = 1; j < GRID_SIZE - 1; j++) {
      float neighbors = previous_grid[i - 1][j] + previous_grid[i + 1][j] +
                        previous_grid[i][j - 1] + previous_grid[i][j + 1];
      float next_val = (neighbors * C_SPEED) +
                       (previous_grid[i][j] * (2.0f - 4.0f * C_SPEED)) -
                       current_grid[i][j];
      next_val *= DAMPING;

      float velocity = fabsf(next_val - current_grid[i][j]);
      if (velocity > 0.15f && (rand() % 100) < 5) {
        trigger_minnaert_bubble(velocity > 1.0f ? 1.0f : velocity, i, j);
      }
      current_grid[i][j] = next_val;
    }
  }

  float (*tmp)[GRID_SIZE] = current_grid;
  current_grid = previous_grid;
  previous_grid = tmp;

  *out_left = 0.0f;
  *out_right = 0.0f;
  for (int i = 0; i < MAX_BUBBLES; i++) {
    if (bubble_pool[i].active) {
      float wave = sinf(bubble_pool[i].phase) * bubble_pool[i].amplitude;
      *out_left += wave * (1.0f - bubble_pool[i].pan);
      *out_right += wave * bubble_pool[i].pan;
      bubble_pool[i].phase +=
          2.0f * (float)M_PI * bubble_pool[i].frequency / SAMPLE_RATE;
      bubble_pool[i].amplitude *= bubble_pool[i].decay;
      if (bubble_pool[i].amplitude < 0.001f)
        bubble_pool[i].active = 0;
    }
  }
}

// Unified Sound Engine: Pass a float from 0.0 to 1.0 to set master volume level
void run_soundscape_engine(snd_pcm_t *pcm_handle, float volume) {
  short buffer[BUFFER_SIZEXZ * CHANNELS];
  int rc;

  printf("");

  while (1) {
    for (int i = 0; i < BUFFER_SIZEXZ; i++) {
      float sample_l = 0.0f;
      float sample_r = 0.0f;

      render_next_sample(&sample_l, &sample_r);

      // Apply the volume attenuation scaling variable
      sample_l *= volume;
      sample_r *= volume;

      if (sample_l > 1.0f)
        sample_l = 1.0f;
      if (sample_l < -1.0f)
        sample_l = -1.0f;
      if (sample_r > 1.0f)
        sample_r = 1.0f;
      if (sample_r < -1.0f)
        sample_r = -1.0f;

      buffer[i * 2] = (short)(sample_l * 32767.0f);
      buffer[i * 2 + 1] = (short)(sample_r * 32767.0f);
    }

    rc = snd_pcm_writei(pcm_handle, buffer, BUFFER_SIZEXZ);
    if (rc == -EPIPE) {
      snd_pcm_prepare(pcm_handle);
    } else if (rc < 0) {
      fprintf(stderr, "Error writing to PCM device: %s\n", snd_strerror(rc));
    }
  }
}
void *loop_four(void *arg) {

  srand(time(NULL));

  if (snd_pcm_open(&pcmz, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0) {
    fprintf(stderr, "Error opening ALSA PCM.\n");
    return 0;
  }

  snd_pcm_set_params(pcmz, SND_PCM_FORMAT_FLOAT_LE,
                     SND_PCM_ACCESS_RW_INTERLEAVED, CHANNELS, SAMPLE_RATE, 1,
                     80000);

  // Initialize all buffers explicitly to zero
  memset(pools, 0, sizeof(pools));
  memset(buf_1l, 0, sizeof(buf_1l));
  memset(buf_1r, 0, sizeof(buf_1r));
  memset(buf_2l, 0, sizeof(buf_2l));
  memset(buf_2r, 0, sizeof(buf_2r));
  memset(buf_3l, 0, sizeof(buf_3l));
  memset(buf_3r, 0, sizeof(buf_3r));
  memset(buf_4l, 0, sizeof(buf_4l));
  memset(buf_4r, 0, sizeof(buf_4r));

  float buf[BUFFER_SIZE * 2];

  while (1) {
    for (int i = 0; i < BUFFER_SIZE; i++) {
      // High probability spawning (16% chance per sample) to build extreme
      // water density
      if ((rand() % 100) < 35) {
        float random_pan = (float)rand() / (float)RAND_MAX;
        float random_energy = 0.3f + (((float)rand() / (float)RAND_MAX) * 0.7f);
        trigger_crystalx(random_energy, random_pan);
      }

      render_samplesz(&buf[i * 2], &buf[i * 2 + 1]);
      apply_extreme_water_blur(&buf[i * 2], &buf[i * 2 + 1]);
    }

    snd_pcm_sframes_t frames = snd_pcm_writei(pcmz, buf, BUFFER_SIZE);
    if (frames < 0) {
      snd_pcm_prepare(pcmz);
    }
  }

  snd_pcm_close(pcmz);
  return 0;
}
int main() {
  setpriority(PRIO_PROCESS, 0, -16);
  pthread_t thread2, thread1;
  pthread_create(&thread2, NULL, loop_two, NULL);
  // Create the threads and assign their functions
  //   pthread_create(&thread2, NULL, loop_two, NULL);
  //   pthread_create(&thread5, NULL, loop_x, NULL);
  pthread_create(&thread1, NULL, loop_one, NULL);
  //    pthread_create(&thread3, NULL, loop_three, NULL);
  //
  //

  pthread_join(thread2, NULL);
  // Wait for the threads to finish (they run forever in this example
  //   pthread_join(thread2, NULL); // bubbles
  //   pthread_join(thread5, NULL); // trickles
  pthread_join(thread1, NULL); // trickles
                               //  pthread_join(thread3, NULL); // water stream

  //
  return 0;
}