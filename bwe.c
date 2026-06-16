#include <alsa/asoundlib.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#define SPH_WINDOW_SIZE 5
#define PINK_MAX_ROWS 12
#define BROOK_POLYPHONY 16

typedef struct {
  float age;
  float duration;
  float base_freq;
  float amp_envelope;
  int active;
} BrookVoice;

static BrookVoice g_brook_pool[BROOK_POLYPHONY];

void process_van_den_doel_brook(float *out_left_channel,
                                float *out_right_channel) {
  const float dt_sample = 1.0f / 44100.0f;
  float accum_L = 0.0f;
  float accum_R = 0.0f;

  // 1. Independent Spawning Grid Matrix
  // A tight threshold limits the frequency of tiny splash occurrences
  if (((float)rand() / (float)RAND_MAX) < 0.015f) {
    for (int v = 0; v < BROOK_POLYPHONY; v++) {
      if (!g_brook_pool[v].active) {
        g_brook_pool[v].active = 1;
        g_brook_pool[v].age = 0.0f;

        // Short lifetime (40ms to 90ms) ensures a crisp, clicking snap
        g_brook_pool[v].duration =
            0.04f + ((float)rand() / (float)RAND_MAX) * 0.05f;

        // Higher pitches (350Hz - 1200Hz) represent small, hollow water
        // cavities
        g_brook_pool[v].base_freq =
            350.0f + ((float)rand() / (float)RAND_MAX) * 850.0f;

        // Low amplitude allocation blends it softly beneath the heavy mix
        g_brook_pool[v].amp_envelope =
            0.05f + ((float)rand() / (float)RAND_MAX) * 0.08f;
        break;
      }
    }
  }

  // 2. Van Den Doel Synthesis Engine (Explicitly matching lines 648-705)
  for (int v = 0; v < BROOK_POLYPHONY; v++) {
    if (g_brook_pool[v].active) {
      float progress = g_brook_pool[v].age / g_brook_pool[v].duration;

      // Core physical parameter: A delicate upward frequency rise (2% to 15%)
      float dynamic_freq =
          g_brook_pool[v].base_freq * (1.0f + 0.12f * progress);

      // Sine window multiplied by a sharp exponential dampening factor (-35.0f)
      float envelope_window =
          sinf(3.14159265f * progress) * expf(-35.0f * progress);

      // Calculate raw oscillator sound wave value
      float sample_mono =
          sinf(g_brook_pool[v].age * (2.0f * 3.14159265f * dynamic_freq)) *
          (g_brook_pool[v].amp_envelope * envelope_window);

      // Spatial stereo splitting via panning assignment
      if (v % 2 == 0) {
        accum_L += sample_mono * 0.75f;
        accum_R += sample_mono * 0.35f;
      } else {
        accum_L += sample_mono * 0.35f;
        accum_R += sample_mono * 0.75f;
      }

      // Progress the internal clock using single-sample increments
      g_brook_pool[v].age += dt_sample;

      // Terminate voice execution safely once lifecycle ends
      if (progress >= 1.0f) {
        g_brook_pool[v].active = 0;
      }
    }
  }

  // Export variables out to your master summing pipeline
  *out_left_channel = accum_L;
  *out_right_channel = accum_R;
}

typedef struct {
  int rows[PINK_MAX_ROWS];
  int running_sum;
  int index;
} PinkNoiseGenerator;

// --- VOSS-MCCARTNEY PINK NOISE GENERATOR VARIABLES ---
#define PINK_ROWS 12
uint32_t pink_indices = 0;
float pink_rows[PINK_ROWS] = {0.0f};
float pink_running_sum = 0.0f;

// Optimized random seed structure matching your existing xorshift logic
static uint32_t pink_noise_state = 987654321;

float generate_pink_noise() {
  // 1. Fast xorshift random roll mapped smoothly between -1.0f and 1.0f
  pink_noise_state ^= (pink_noise_state << 13);
  pink_noise_state ^= (pink_noise_state >> 17);
  pink_noise_state ^= (pink_noise_state << 5);
  float raw_white =
      ((float)(pink_noise_state & 0xFFFF) / 65535.0f) * 2.0f - 1.0f;

  // 2. Count trailing zeros to determine which filter row updates
  pink_indices++;
  int trailing_zeros = __builtin_ctz(pink_indices);
  if (trailing_zeros >= PINK_ROWS) {
    trailing_zeros = PINK_ROWS - 1;
    pink_indices = 0; // Reset index counter safely
  }

  // 3. Subtract the old row value from sum, inject new random variance, update
  // row
  pink_running_sum -= pink_rows[trailing_zeros];
  pink_rows[trailing_zeros] = raw_white;
  pink_running_sum += pink_rows[trailing_zeros];

  // 4. Combine the running grid array sum with an extra random flash to
  // eliminate structural patterns
  float finalized_pink =
      (pink_running_sum + raw_white) / (float)(PINK_ROWS + 1);

  return finalized_pink; // Balanced output scaled roughly from -1.0 to 1.0
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

  // 1. Process in floating-point space to eliminate intermediate quantization
  // noise
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
        // 2. Normalize by total weight to preserve original volume gain
        float smoothed_sample = weighted_sum / total_weight;

        // 3. Simple TPDF-style dither to eliminate remaining LSB truncation
        // static
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

void safe_volume_boost_alsa(int16_t *buffer, int frames, float volume_boost) {
  int total_samples = frames * 2; // Stereo interleaved channels (L, R, L, R)
  float max_peak = 0.0001f;

  // Allocate a temporary floating-point buffer to prevent premature integer
  // clipping
  float *float_workspace = (float *)malloc(total_samples * sizeof(float));
  if (float_workspace == NULL)
    return; // Fail-safe guard if out of memory

  // Step 1: Unpack integers to floats and apply the raw volume boost
  for (int i = 0; i < total_samples; i++) {
    // Convert to a standardized -1.0 to 1.0 decimal scale
    float normalized_sample = (float)buffer[i] / 32767.0f;

    // Multiply by your boost value (this can safely push past 1.0 temporarily)
    float_workspace[i] = normalized_sample * volume_boost;

    // Track the single highest volume peak created by the boost
    float abs_val = fabsf(float_workspace[i]);
    if (abs_val > max_peak) {
      max_peak = abs_val;
    }
  }

  // Step 2: Calculate a normalization scalar if the boost pushed the audio into
  // distortion
  float limiter_scalar = 1.0f;
  if (max_peak > 1.0f) {
    // If the boost caused clipping, compress it perfectly back down to a clean
    // 0dBFS ceiling
    limiter_scalar = 1.0f / max_peak;
  }

  // Step 3: Apply the limiter scalar, clamp limits, and pack cleanly back to
  // 16-bit integers
  for (int i = 0; i < total_samples; i++) {
    float finalized_sample = float_workspace[i] * limiter_scalar;

    // Hard boundary safety guards to completely eliminate integer wrap-around
    // noise
    if (finalized_sample > 1.0f)
      finalized_sample = 1.0f;
    if (finalized_sample < -1.0f)
      finalized_sample = -1.0f;

    // Scale cleanly back to ALSA hardware architecture limits
    buffer[i] = (int16_t)(finalized_sample * 32767.0f);
  }

  // Free the temporary workspace memory
  free(float_workspace);
}
// --- HIGH-PASS FILTER VARIABLES (REMOVES BASS RUMBLE) ---
float hp_prev_in_l = 0.0f;
float hp_prev_out_l = 0.0f;
float hp_prev_in_r = 0.0f;
float hp_prev_out_r = 0.0f;

// The filter coefficient:
// 0.985f sets a tight low-cut roughly around 120Hz-150Hz.
// This completely carves out the deep sub-bass mud without losing clarity.
// float hp_alpha = 0.982f;
float hp_alpha = 0.940f; // Drop from 0.982f to let deep sub-bass gargles pass
                         // through cleanly

// --------------------------------------------------------

// --- FLUID LIQUID INTEGRATORS (DIRECT AUDIO DAMPENING) ---
float fluid_history_l = 0.0f;
float fluid_history_r = 0.0f;

// Lower values make the water smoother and more fluid.
// Higher values let more of the individual bubble details through.
float fluid_smudge_factor = 0.04f;
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
#define MAX_POLYPHONY 16
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
#define BUFFER_FRAMES 8192
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
// const double MASTER_VOLUME = 80;
// double BUBBLE_RATE_HZ = 1.0;
// double DROPLET_SIZE_MIN = 0.0050;
// double DROPLET_SIZE_MAX = 0.0450;
const double MASTER_VOLUME = 80; // Safe pre-gain ceiling multiplier
double BUBBLE_RATE_HZ = 1.0;
double DROPLET_SIZE_MIN = 0.0550; // Significantly larger min bubble volume
double DROPLET_SIZE_MAX = 0.0850; // Massive max radius for deep throat gurgles

typedef struct {
  int active;
  double current_phase;
  double age;
  double duration;
  double amplitude;
  // Add these inside the typedef struct { ... } Droplet;
  double damping_factor; // Controls exponential decay thickness
  double pitch_slur;     // Controls how aggressively pitch curves up over time

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
      // Inside trigger_droplet(), before the panning calculations:
      droplets[i].damping_factor =
          rand_double(2.0, 4.0);                      // Higher = more viscous
      droplets[i].pitch_slur = rand_double(6.0, 8.0); // Higher = sharper chirp

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
          rand_double(50, 150.0) * size_factor + micro_drift;

      // Sweep target climbs swiftly away from bass rumble to create clean fluid
      // definition

      // Stereo Position assignment (0.0 = Far Left, 1.0 = Far Right)
      double pan = rand_double(0.0, 1.0);
      droplets[i].pan_left = sqrt(1.0 - pan); // Equal-power panning curve
      droplets[i].pan_right = sqrt(pan);
      break;
    }
  }
}
#define M_PI 3.14159265358979323846

void trigger_vandoel_medium_bubble() {
  for (int i = 0; i < NUM_DROPLETS; i++) {
    if (!droplets[i].active) {
      // 1. CHOOSE A "MEDIUM" RADIUS (in meters)
      // Medium bubbles are roughly 6mm to 9mm in diameter (0.003m to 0.0045m
      // radius)
      double radius = rand_double(0.003, 0.0045);

      // 2. VAN DEN DOEL / MINNAERT FREQUENCY CALCULATION
      // Standard Minnaert approximation: f = 3.0 / radius
      double minnaert_freq = 3.0 / radius;
      droplets[i].sweep_start = minnaert_freq;

      // 3. PITCH CHIRP (Slightly rising frequency as it breaks free)
      // Van den Doel uses a slope parameter (epsilon) between 4.0 and 8.0
      droplets[i].pitch_slur = rand_double(4.0, 6.0);

      // 4. VAN DEN DOEL DAMPING FACTOR
      // Damping combines thermal/viscous loss (0.013/r) and radiation loss
      // (0.022 * f)
      double vandoel_damping = (0.013 / radius) + (0.022 * minnaert_freq);

      // For a "SOFT" sound, we slightly scale up the damping to make it more
      // muffled
      droplets[i].damping_factor = vandoel_damping * 1.3;

      // 5. LIFESPAN & AMPLITUDE
      // Medium bubbles decay completely within 100ms to 180ms
      droplets[i].duration = rand_double(0.10, 0.18);
      droplets[i].amplitude =
          rand_double(0.15, 0.30) * (1.0 / (double)NUM_DROPLETS);

      // 6. INITIALIZE STATE & STEREO PAN
      droplets[i].active = 1;
      droplets[i].current_phase = 0.0;
      droplets[i].age = 0.0;

      double pan = rand_double(0.1, 0.9);
      droplets[i].pan_left = sqrt(1.0 - pan);
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
    h_nodes[i].vel += DT * force_gradient;
    h_nodes[i].pos += DT * h_nodes[i].vel;
  }

  // Return the pressure wave sample captured at the far end node of the array
  return h_nodes[AUDIO_P_COUNT - 1].prs;
}

#define METALLIC_FIX_STAGES 4
static const int BLUR_DELAYS[METALLIC_FIX_STAGES] = {1433, 1973, 2657, 3529};

typedef struct {
  float *buffer;
  int size;
  int idx;
  float lpf_state;
} OrganicBlurStage;

static OrganicBlurStage blur_L[METALLIC_FIX_STAGES];
static OrganicBlurStage blur_R[METALLIC_FIX_STAGES];

// Hydro-acoustic resonance filters (Bi-quad state variables to shape brook
// gurgles)
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

  // Clear out biquad resonators
  x1_L = x2_L = y1_L = y2_L = 0.0f;
  x1_R = x2_R = y1_R = y2_R = 0.0f;

  blur_initialized = 1;
}

float process_organic_sample(float input, OrganicBlurStage *stages) {
  float sample = input;
  const float diffusion = 0.65f;
  const float damping = 0.35f;

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

  // --- TUNING A REALISTIC BROOK CHOP ---
  // const float ocean_wash_blend =
  //  0.30f; // Lowered (was 0.55f) to bring out bubble sharpness
  //  const float stream_gain_limit =
  //   1.75f; // Boosted (was 0.75f) to elevate the bubble ringing peaks
  // Change lines 477-480
  const float ocean_wash_blend =
      0.15f; // Raised from 0.30f to simulate water rushing over rocks
  const float stream_gain_limit =
      5.75f; // Raised from 1.75f to allow for louder individual splash peaks

  // Extra headroom to kill remaining line static

  // Brook resonant filter coefficients (tuned for a 450Hz liquid "hollow"
  // cavity boost at 44.1kHz) Changing these shapes whether the brook sounds
  // like a tiny stream or a wider river bed.
  const float b0 = 0.046f, b1 = 0.0f, b2 = -0.046f;
  const float a1 = -1.890f, a2 = 0.907f;
  // Change lines 487-488
  // const float b0 = 0.082f, b1 = 0.0f, b2 = -0.082f;
  // const float a1 = -1.715f, a2 = 0.836f;
  // Change lines 487-488
  // const float b0 = 0.065f, b1 = 0.0f, b2 = -0.065f;
  // const float a1 = -1.482f, a2 = 0.870f;

  for (int f = 0; f < frames; f++) {
    float left_in = (float)buffer[f * 2];
    float right_in = (float)buffer[f * 2 + 1];

    // 1. Process the perfect ocean blur setting to get our underlying water
    // friction flow
    float wet_l = process_organic_sample(left_in, blur_L);
    float wet_r = process_organic_sample(right_in, blur_R);

    // 2. Mix the raw stream with the wash before cavity filtering
    float raw_mix_L =
        (left_in * (1.0f - ocean_wash_blend)) + (wet_l * ocean_wash_blend);
    float raw_mix_R =
        (right_in * (1.0f - ocean_wash_blend)) + (wet_r * ocean_wash_blend);

    // 3. Pass the mixed audio through the Hydro-Acoustic Cavity Resonators
    // This isolates the static hiss, rounds it off, and pushes the liquid
    // "bloop" notes forward
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

    // 4. Scale and compress gently using headroom limits
    float out_l = brook_L * stream_gain_limit;
    float out_r = brook_R * stream_gain_limit;

    // Hard boundary protection clamps
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
  double speed_modifier = 0.7; // 1.0 is normal, 0.5 is half speed (slower)
  double dt = (1.0 / SAMPLE_RATE / BUBBLE_RATE_HZ) * speed_modifier;

  // double dt = 1.0 / SAMPLE_RATE / BUBBLE_RATE_HZ;

  printf("Now Streaming: Babbling Brook...\n");

  while (1) {
    for (int f = 0; f < BUFFER_FRAMES; f++) {
      double cluster_jitter = 1.0 + 0.8 * sin(total_elapsed_time * 7.3) *
                                        cos(total_elapsed_time * 19.1);
      if (rand_double(0.0, 100.0) < (1.0 * speed_modifier * cluster_jitter)) {
        trigger_droplet();
      }
      if (rand_double(0.0, 100.0) < (1.0 * speed_modifier)) {
        trigger_vandoel_medium_bubble();
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
      //  current_grid_density =
      //   0.00008f + ((float)cell_x / (float)GRID_SIZE) * 0.00018f;
      // Change lines 622-623 to a lighter processing sweet spot:
      // current_grid_density = 0.0015f + ((float)cell_x / (float)GRID_SIZE) *
      // 0.0035f;
      current_grid_density =
          0.0020f + ((float)cell_x / (float)GRID_SIZE) * 0.0040f;

      // ============================================================================
      // POLYPHONIC MATRIX SOUND GENERATION ENGINE
      // ============================================================================
      // Fast Pseudo-Random Generator (Xorshift)
      float scholastic_water_L = 0.0f;
      float scholastic_water_R = 0.0f;
      noise_state ^= (noise_state << 13);
      noise_state ^= (noise_state >> 17);
      noise_state ^= (noise_state << 5);
      float random_roll_L = (float)(noise_state & 0xFFFF) / 65535.0f;
      float random_roll_R = (float)((noise_state >> 16) & 0xFFFF) / 65535.0f;
      // === VAN DEN DOEL MATRIX OSCILLATOR (Lines 364-400) ===
      for (int v = 0; v < MAX_POLYPHONY; v++) {
        if (drops_L[v].active || drops_R[v].active) {
          // Update age based on sample steps
          if (drops_L[v].active)
            drops_L[v].age += 1.0f;
          if (drops_R[v].active)
            drops_R[v].age += 1.0f;

          // Physics-based parameters: 15% freq rise, exponential decay
          float progressL = drops_L[v].age / drops_L[v].duration;
          float progressR = drops_R[v].age / drops_R[v].duration;

          // ... (Applied to freq and amp for L/R)

          // Final synthesis

          // New Left Channel
          float target_freq_L =
              drops_L[v].base_freq * (1.0f + 0.02f * progressL);
          float envelope_window_L =
              sinf(3.14159265f * progressL) * expf(-35.0f * progressL);
          scholastic_water_L +=
              sinf(drops_L[v].age *
                   (2.0f * 3.14159265f * target_freq_L / 44100.0f)) *
              (drops_L[v].amp_envelope * envelope_window_L);
        }
      }
      // }
      //  if (random_roll_R < current_grid_density) {
      // === VAN DEN DOEL MATRIX OSCILLATOR (Lines 364-400) ===
      /*    for (int v = 0; v < MAX_POLYPHONY; v++) {
            if (drops_L[v].active || drops_R[v].active) {
              // Update age based on sample steps
              if (drops_L[v].active)
                drops_L[v].age += 1.0f;
              if (drops_R[v].active)
                drops_R[v].age += 1.0f;

              // Physics-based parameters: 15% freq rise, exponential decay
              float progressL = drops_L[v].age / drops_L[v].duration;
              float progressR = drops_R[v].age / drops_R[v].duration;

              // ... (Applied to freq and amp for L/R)

              // Final synthesis

              // New Right Channel
              float target_freq_R =
                  drops_R[v].base_freq * (1.0f + 0.02f * progressR);
              float envelope_window_R =
                  sinf(3.14159265f * progressR) * expf(-35.0f * progressR);
              scholastic_water_R +=
                  sinf(drops_R[v].age *
                       (2.0f * 3.14159265f * target_freq_R / 44100.0f)) *
                  (drops_R[v].amp_envelope * envelope_window_R);
            }
          }
          // Sum overlapping polyphonic voices smoothly
          // float scholastic_water_L = 0.0f;
          //  float scholastic_water_R = 0.0f;

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
          }*/

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
      // --- Inside your f-frame generation loop (around line 784) ---
      // double mixed_left = carrier_left;
      // double mixed_right = carrier_right;

      // 1. Process your existing heavy/large gargling bubble array here
      // ... (Your loop that adds droplet code to mixed_left and mixed_right)
      // ...

      // 2. Call the soft brook engine to get the separate float channel outputs
      float brook_splash_L = 0.0f;
      float brook_splash_R = 0.0f;
      process_van_den_doel_brook(&brook_splash_L, &brook_splash_R);

      // 3. Accumulate them directly onto the master stereo mixing bus
      mixed_left += (double)brook_splash_L;
      mixed_right += (double)brook_splash_R;

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

          double sweep_end = droplets[i].sweep_start * rand_double(3.0, 6.0);
          droplets[i].sweep_factor = sweep_end / droplets[i].sweep_start;

          double progress = droplets[i].age / droplets[i].duration;

          // 1. Dynamic Pitch Slur (Exp. frequency change)
          double current_freq =
              droplets[i].sweep_start * pow(droplets[i].pitch_slur, progress);

          // 2. Exponential Volume Decay (Damping)
          double envelope = droplets[i].amplitude *
                            exp(-droplets[i].damping_factor * progress) *
                            sin(PI * progress);

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

      // float background_roar_l = generate_pink_noise(&pink_left) * 0.04f; //
      // 4% mix level float background_roar_r = generate_pink_noise(&pink_right)
      // * 0.04f;
      //  5. Output clean raw interleaved binary streams to standard out
      //   int16_t out_sample;
      //     float constant_background_roar =
      //      (((float)rand() / (float)RAND_MAX) * 2.0f) - 1.0f;
      // Replace lines 699 - 709 with this line:
      float background_roar =
          generate_pink_noise() *
          5.0f; // Increased to 12% for a richer, deeper background water roar

      // Apply a rough bandpass filter to mask the noise or damp it heavily
      //  constant_background_roar *=
      0.03f; // Keep it low (3% volume) to act as a glue layer

      if (sample_counter++ >= 64) {
        cached_hydro_sample = step_sph_hydro_sample(150.0f); // Run SPH step
        sample_counter = 0;                                  // Reset counter
      }

      float hydro_sample = cached_hydro_sample;

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
          (int16_t)(((amplified_L + hydro_sample + background_roar)));
      buffer[f * 2 + 1] =
          (int16_t)(((amplified_R + hydro_sample + background_roar)));
    }
    //   safe_volume_boost_alsa(buffer, BUFFER_FRAMES, 1.0f);
    // Locate line ~769 in your main loop right before snd_pcm_writei:
    // A smoothing_radius of 3.0f to 5.0f smooths out bubble popping textures
    // nicely.
    smooth_bubbles_sph(buffer, BUFFER_FRAMES, 5.0f);
    blur_bubbles_engine(buffer, BUFFER_FRAMES);
    // snd_pcm_writei(pcm_handle, buffer, BUFFER_FRAMES);

    // smooth_bubbles_sph(buffer, BUFFER_FRAMES, 5.0f);

    // snd_pcm_writei(pcm_handle, buffer, BUFFER_FRAMES);

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
