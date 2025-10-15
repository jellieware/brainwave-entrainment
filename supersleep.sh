#!/bin/bash


freq2="35"+"7.8";
minutes=${1:-'59'}
repeats=$(( minutes - 1 ))
center=${2:-'1786'}
wave=${3:-'0.0333333'}
noise='brown'
len='01:00'

mynoise="play $progress -q -c 2  --null  -t pulseaudio  synth  $len  ${noise}noise  \
     band -n $center 499               \
     tremolo $wave    43   reverb 19   \
     bass -11              treble -1   \
     vol     14dB                      \
     repeat  $repeats"

# A script to play delta wave binaural beats using SoX.

# Delta wave beat frequency range: 0.5-4 Hz.
# Carrier frequency: Use a low, non-intrusive frequency like 150 Hz.
# Time: The duration to play the sound, in seconds.

# Define variables
BEAT_FREQUENCY=3    # Delta wave beat (e.g., 3 Hz)
CARRIER_FREQUENCY=25  # Base frequency for one channel (e.g., 150 Hz)
DURATION=3600         # Play for 30 minutes (30 * 60 seconds)

# Calculate the second frequency for the binaural beat
FREQUENCY_RIGHT=$((CARRIER_FREQUENCY + BEAT_FREQUENCY))

echo "Playing binaural delta waves for $((DURATION / 60)) minutes."
echo "Left channel: ${CARRIER_FREQUENCY}Hz | Right channel: ${FREQUENCY_RIGHT}Hz"
echo "Beat frequency: ${BEAT_FREQUENCY}Hz"
echo "Press Ctrl+C to stop."

# Generate and play the binaural beat using the `play` command
play -q -n synth $DURATION \
     sine $CARRIER_FREQUENCY \
     sine $FREQUENCY_RIGHT \
     vol 1.5 gain -3  & $mynoise
