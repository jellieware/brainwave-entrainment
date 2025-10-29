#!/bin/bash

# Delay parameters for the chorus effect
# The chorus effect simulates a delay and is used here for its LFO capabilities.
# A chorus effect with a slow speed and small delay variation will create a panning effect.
# Syntax: <gain-in> <gain-out> <delay> <decay> <speed> <depth>
# delay: Base delay in ms.
# decay: Amount of decay (feedback) in ms.
# speed: LFO speed in Hz. Lower values create slower sweeps.
# depth: Depth of the LFO modulation in ms.
GAIN_IN=0.9
GAIN_OUT=0.9
BASE_DELAY_MS=20
DECAY_MS=0
SPEED_HZ=0.15  # Adjust for sweep speed. Slower = less frantic panning.
DEPTH_MS=10   # Adjust for the amount of phase difference.

# Define variables
BEAT_FREQUENCY=1  # Delta wave beat (e.g., 3 Hz)
CARRIER_FREQUENCY=54  # Base frequency for one channel (e.g., 150 Hz)

# Calculate the second frequency for the binaural beat
FREQUENCY_RIGHT=$((CARRIER_FREQUENCY + BEAT_FREQUENCY))

echo "Playing binaural delta waves with rain..."
echo "Left channel: ${CARRIER_FREQUENCY}Hz | Right channel: ${FREQUENCY_RIGHT}Hz"
echo "Beat frequency: ${BEAT_FREQUENCY}Hz"
echo "Press Ctrl+C to stop."

# Generate and play the binaural beat using the `play` command
play -q -n synth \
     sine $CARRIER_FREQUENCY \
     sine $FREQUENCY_RIGHT \
     reverb 40 50 100 pitch -31.76665 vol 0.05 & 
play -q -n -c2 synth pinknoise mix band -n 9000 1800 tremolo 2000 1 lowpass 1000 reverb 40 50 100 pitch -31.76665 vol 2.0 \
chorus $GAIN_IN $GAIN_OUT "$BASE_DELAY_MS" "$DECAY_MS" "$SPEED_HZ" "$DEPTH_MS" -t &

wait
