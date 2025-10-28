#!/bin/bash



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
play -q -n -c2 synth pinknoise mix band -n 9000 1800 tremolo 2000 1 lowpass 1000 reverb 40 50 100 pitch -31.76665 vol 2.0 &

wait
