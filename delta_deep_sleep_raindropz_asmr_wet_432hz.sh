#!/bin/bash
MIN=400
MAX=800

MINTIME=1
MAXTIME=3

tempomin=0.5
tempomax=1.5

BAMBOOTMIN=60
BAMBOOTMAX=100

pitchmin=25
pitchmax=75


# Calculate the range difference
RANGE_DIFF=$(echo "$MAXTIME - $MINTIME" | bc -l)
RANGE_DIFFX=$(echo "$tempomax - $tempomin" | bc -l)
RANGE_DIFFXX=$(echo "$pitchmax - $pitchmin" | bc -l)
# Generate a random number between 0 and 1 (approx)
# Use a higher scale for better precision if needed

RANGEX=$((BAMBOOTMAX - BAMBOOTMIN + 1))
# Calculate the range size
RANGE=$((MAX - MIN + 1))
# --- User-customizable parameters ---
# Duration of the sound in seconds. Shorter means a faster "plop".
DURATION=0.2

# Starting and ending frequencies for the pitch-bend.
# Adjust these values to change the character of the drop.
# A higher start frequency creates a "thinner" sounding drop.
START_FREQ=100
END_FREQ=700

# Volume level (a multiplier from 0 to 1).
# A lower value might sound more natural.
VOLUME=0.1

# Echo effect.
# Adjust these values to change the room ambiance.
# Example: 0.8 0.5 100 0.5 200 0.3
# First pair: Volume scale and decay.
# Second pair: Delay and decay.
# To disable echo, comment out the line containing `echo`.
ECHO_PARAMS="0.8 0.5 50 0.5"

# Reverb effect.
# For more detail, check the SoX documentation on the 'reverb' effect.
# To disable reverb, comment out the line containing `reverb`.
REVERB_PARAMS="100 50 100"


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

# --- Main command ---
# play: This SoX alias plays the audio directly.
# -n:  Specifies that no input file is used; SoX generates the sound.
while true; do

# Generate a random number within the desired range

RANDOM_NUMBER=$((RANDOM % RANGE + MIN))
RANDOM_NUMBERX=$((RANDOM % RANGEX + BAMBOOTMIN))
  RAND_0_TO_1X=$(echo "scale=2; $RANDOM / 32767" | bc -l)
  RAND_FLOATX=$(echo "scale=2; $tempomin + ($RAND_0_TO_1X * $RANGE_DIFFX)" | bc -l)
  
  RAND_0_TO_1XX=$(echo "scale=2; $RANDOM / 32767" | bc -l)
  RAND_FLOATXX=$(echo "scale=2; $pitchmin + ($RAND_0_TO_1XX * $RANGE_DIFFXX)" | bc -l)
  
  
  
  RAND_0_TO_1=$(echo "scale=2; $RANDOM / 32767" | bc -l)
  RAND_FLOAT=$(echo "scale=2; $MINTIME + ($RAND_0_TO_1 * $RANGE_DIFF)" | bc -l)
  

# Scale and shift the random number to the desired range

play -q -n synth $DURATION \
  sine "${START_FREQ}-${RANDOM_NUMBER}" \
  vol $VOLUME \
  pitch $RAND_FLOATXX \
  tempo $RAND_FLOATX \
  reverb 80 50 100 \
  echos 0.8 0.7 10 0.5 \
  lowpass $RANDOM_NUMBERX &
  

  sleep $RAND_FLOAT
  
done

wait
