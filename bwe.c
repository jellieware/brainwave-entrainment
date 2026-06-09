#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <time.h>
#include <alsa/asoundlib.h>

#define SAMPLE_RATE 44100
#define PI 3.14159265358979323846
#define PCM_DEVICE "default"
#define BUFFER_FRAMES 2048
#define NUM_DROPLETS 500	// Targeted dense overlap array pool size

// Global Master Loudness Control (0.0 to 1.0)
const double MASTER_VOLUME = 15;
double DECAY_RATE = 6.0;
double BUBBLE_RATE_HZ = 1.0;
double DROPLET_SIZE_MIN = 0.00005;
double DROPLET_SIZE_MAX = 0.0025;
typedef struct
{
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

double
rand_double (double min, double max)
{
  return min + ((double) rand () / (double) RAND_MAX) * (max - min);
}

void
trigger_droplet ()
{
  for (int i = 0; i < NUM_DROPLETS; i++)
    {
      if (!droplets[i].active)
	{
	  droplets[i].active = 1;
	  droplets[i].current_phase = 0.0;
	  droplets[i].age = 0.0;



	  double size_range = DROPLET_SIZE_MAX - DROPLET_SIZE_MIN;
	  double randomized_radius =
	    DROPLET_SIZE_MIN + (((double) rand () / RAND_MAX) * size_range);

	  double size_factor = DROPLET_SIZE_MIN / randomized_radius;
	  double micro_drift = (((double) rand () / RAND_MAX) - 0.5) * 40.0;
	  // Varied short life cycles keeps the texture fluid rather than robotic
	  droplets[i].duration = rand_double (0.06, 0.1);

	  // Scale baseline per-droplet volume explicitly to prevent 100-wave summation clipping
	  droplets[i].amplitude =
	    rand_double (0.3, 0.7) * (1.0 / (double) NUM_DROPLETS);

	  // STRICT USER TARGET: Base pitch initializes between 50Hz and 150Hz
	  droplets[i].sweep_start =
	    rand_double (280, 320.0) * size_factor + micro_drift;

	  // Sweep target climbs swiftly away from bass rumble to create clean fluid definition
	  droplets[i].sweep_end =
	    droplets[i].sweep_start + rand_double (2000,
						   16500.0) * size_factor;
	  droplets[i].sweep_range =
	    droplets[i].sweep_end - droplets[i].sweep_start;

	  // Stereo Position assignment (0.0 = Far Left, 1.0 = Far Right)
	  double pan = rand_double (0.0, 1.0);
	  droplets[i].pan_left = sqrt (1.0 - pan);	// Equal-power panning curve
	  droplets[i].pan_right = sqrt (pan);
	  break;
	}
    }
}

int
main ()
{
  srand ((unsigned int) time (NULL));

  snd_pcm_t *pcm_handle;
  snd_pcm_hw_params_t *params;
  int err;

  if ((err =
       snd_pcm_open (&pcm_handle, PCM_DEVICE, SND_PCM_STREAM_PLAYBACK,
		     0)) < 0)
    {
      fprintf (stderr, "ERROR: Can't open \"%s\" PCM device: %s\n",
	       PCM_DEVICE, snd_strerror (err));
      return 1;
    }

  snd_pcm_hw_params_alloca (&params);
  snd_pcm_hw_params_any (pcm_handle, params);
  snd_pcm_hw_params_set_access (pcm_handle, params,
				SND_PCM_ACCESS_RW_INTERLEAVED);
  snd_pcm_hw_params_set_format (pcm_handle, params, SND_PCM_FORMAT_S16_LE);
  snd_pcm_hw_params_set_channels (pcm_handle, params, 2);	// Explicit stereo stream config

  unsigned int rate = SAMPLE_RATE;
  snd_pcm_hw_params_set_rate_near (pcm_handle, params, &rate, 0);

  if ((err = snd_pcm_hw_params (pcm_handle, params)) < 0)
    {
      fprintf (stderr, "ERROR: Can't set hardware parameters: %s\n",
	       snd_strerror (err));
      snd_pcm_close (pcm_handle);
      return 1;
    }

  for (int i = 0; i < NUM_DROPLETS; i++)
    {
      droplets[i].active = 0;
    }

  int16_t buffer[BUFFER_FRAMES * 2];
  double dt = 1.0 / SAMPLE_RATE / BUBBLE_RATE_HZ;

  printf ("Now Streaming: Babbling Brook...\n");

  while (1)
    {
      for (int f = 0; f < BUFFER_FRAMES; f++)
	{

	  // Highly aggressive spawn rate to keep the 100-channel allocation engine saturated
	  if (rand_double (0.0, 100.0) < 1.5)
	    {
	      trigger_droplet ();
	    }

	  double mixed_left = 0.0;
	  double mixed_right = 0.0;

	  for (int i = 0; i < NUM_DROPLETS; i++)
	    {
	      if (droplets[i].active)
		{
		  if (droplets[i].age >= droplets[i].duration)
		    {
		      droplets[i].active = 0;
		      continue;
		    }

		  //    double progress = droplets[i].age / droplets[i].duration;

		  // Smooth window curve clears out edge clicks and abrupt pops
		  //     double envelope = sin(progress * PI);

		  // Pure linear calculation prevents sub-bass lingering distortion
		  //      double current_freq = droplets[i].sweep_start + (droplets[i].sweep_range * progress);

		  double sweep_end =
		    droplets[i].sweep_start * rand_double (3.0, 6.0);
		  droplets[i].sweep_factor =
		    sweep_end / droplets[i].sweep_start;
		  double progress = droplets[i].age / droplets[i].duration;

		  if (progress >= 1.0)
		    {
		      droplets[i].active = 0;
		      continue;
		    }

		  // 1. EXPONENTIAL FREQUENCY SWEEP
		  // Pitch accelerates upward drastically near the end
		  double current_freq =
		    droplets[i].sweep_start * pow (droplets[i].sweep_factor,
						   progress);

		  // 2. EXPONENTIAL VOLUME DECAY ENVELOPE
		  // Sharp initial burst with a smooth, natural ringing tail
		  double envelope =
		    droplets[i].amplitude * exp (-5.0 * progress) * sin (PI *
									 progress);

		  // Generate sine wave sample
		  double sample_mono =
		    envelope * sin (droplets[i].current_phase);

		  // Update phase based on the accelerating frequency
		  droplets[i].current_phase +=
		    (2.0 * PI * current_freq) / SAMPLE_RATE;
		  if (droplets[i].current_phase > 2.0 * PI)
		    {
		      droplets[i].current_phase -= 2.0 * PI;
		    }




		  // Track strict phase continuity to ensure smooth transitions
		  droplets[i].current_phase += 2.0 * PI * current_freq * dt;
		  if (droplets[i].current_phase > 2.0 * PI)
		    {
		      droplets[i].current_phase =
			fmod (droplets[i].current_phase, 2.0 * PI);
		    }

		  // Compute single mono raw sample contribution
		  //    double sample_mono = sin(droplets[i].current_phase) * envelope * droplets[i].amplitude;

		  // Route mono drop signal into wide stereo bus using assigned pan coordinates
		  mixed_left += sample_mono * droplets[i].pan_left;
		  mixed_right += sample_mono * droplets[i].pan_right;

		  droplets[i].age += dt;
		}
	    }

	  // Apply soft-saturation clipping protection along both distinct channels
	  double sat_left = tanh (mixed_left) * MASTER_VOLUME;
	  double sat_right = tanh (mixed_right) * MASTER_VOLUME;

	  // Convert floating point coordinates to native PCM 16-bit sound card boundaries
	  buffer[f * 2] = (int16_t) (sat_left * 32767.0);	// Left Channel Index
	  buffer[f * 2 + 1] = (int16_t) (sat_right * 32767.0);	// Right Channel Index
	}

      snd_pcm_sframes_t written =
	snd_pcm_writei (pcm_handle, buffer, BUFFER_FRAMES);
      if (written < 0)
	{
	  written = snd_pcm_recover (pcm_handle, written, 0);
	}
      if (written < 0)
	{
	  fprintf (stderr, "ERROR: Failed writing data to PCM device: %s\n",
		   snd_strerror (written));
	  break;
	}
    }

  snd_pcm_drain (pcm_handle);
  snd_pcm_close (pcm_handle);
  return 0;
}
