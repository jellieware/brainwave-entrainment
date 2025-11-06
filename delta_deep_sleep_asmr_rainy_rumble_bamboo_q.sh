#!/bin/bash
#author: alex terranova
#date:11/04/2025
#name:rumblefish
#version:1.0
#description: Brainwave-Entrainment
input=""
ZMIN=600
ZMAX=1400
ZMINTIME=0.2
ZMAXTIME=3
ZRANGE_DIFF=$(echo "$ZMAXTIME - $ZMINTIME" | bc -l)
ZRANGE=$((ZMAX - ZMIN + 1)) 

function chime_sound() {
    local freq=$1
    local duration=$2
    local gain=$3

    # Use 'play -n' to synthesize sound and send directly to the sound card
    # without creating a file.
    # We use multiple 'synth' effects piped together to layer slightly detuned
    # sine waves, creating a richer, more bell-like tone.
    # The 'fade' effect gives it a natural attack and a long, logarithmic decay.
    play -q -n -c1 synth "$duration" sine "$freq" fade l 0 0.2 "$duration" lowpass 100 vol 0.5 &
}

thundertimeMIN=50
thundertimeMAX=200

# Calculate the range size (inclusive)
RANGEthunder=$((thundertimeMAX - thundertimeMIN + 1))

# Generate a random number within the range and add the minimum value

thunderlengthMIN=6
thunderlengthMAX=10

# Calculate the range size (inclusive)
RANGElength=$((thunderlengthMAX - thunderlengthMIN + 1))

MIN=400
MAX=800

MINTIME=0.2
MAXTIME=1

tempomin=0.5
tempomax=1.5

BAMBOOTMIN=100
BAMBOOTMAX=200

pitchmin=20
pitchmax=120


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
END_FREQ=300

# Volume level (a multiplier from 0 to 1).
# A lower value might sound more natural.
VOLUME=0.2

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

echo "Playing Now..."
echo "Left channel: ${CARRIER_FREQUENCY}Hz | Right channel: ${FREQUENCY_RIGHT}Hz"
echo "Beat frequency: ${BEAT_FREQUENCY}Hz"

# Generate and play the binaural beat using the `play` command
play -q -n synth \
     sine $CARRIER_FREQUENCY \
     sine $FREQUENCY_RIGHT \
     reverb 40 50 100 pitch -31.76665 vol 0.05 & 
play -q -n -c2 synth pinknoise mix band -n 9000 1800 tremolo 2000 1 lowpass 200 reverb 40 50 100 pitch -31.76665 vol 1 \
chorus $GAIN_IN $GAIN_OUT "$BASE_DELAY_MS" "$DECAY_MS" "$SPEED_HZ" "$DEPTH_MS" -t &

# --- Main command ---
# play: This SoX alias plays the audio directly.
# -n:  Specifies that no input file is used; SoX generates the sound.
while [[ "$input" != "q" ]]; do
((thunder++))
RANDOM_thunder=$(( (RANDOM % RANGEthunder) + thundertimeMIN ))
if [[ $thunder -gt RANDOM_thunder ]]; then
thunder=0
RANDOM_length=$(( (RANDOM % RANGElength) + thunderlengthMIN ))

play -q -n -c1 synth pinknoise \
        fade h 0.5 $RANDOM_length $RANDOM_length \
        flanger 0.5 0.8 50 0.5 2 sine \
        tremolo 0.1 50 \
        lowpass 40 \
        vol 1.2 \
        reverb 50 50 100 100 100 0 > /dev/null 2>&1 & # Add reverb for space and power



fi
# Generate a random number within the desired range

ZRAND_0_TO_1=$(echo "scale=2; $RANDOM / 32767" | bc -l)
ZRAND_FLOAT=$(echo "scale=2; $ZMINTIME + ($ZRAND_0_TO_1 * $ZRANGE_DIFF)" | bc -l)
ZRANDOM_NUMBER=$((RANDOM % ZRANGE + ZMIN))  
chime_sound $ZRANDOM_NUMBER 0.2 -5
sleep $ZRAND_FLOAT
  
done &

while [[ "$input" != "q" ]]; do

ZRAND_0_TO_1=$(echo "scale=2; $RANDOM / 32767" | bc -l)
ZRAND_FLOAT=$(echo "scale=2; $ZMINTIME + ($ZRAND_0_TO_1 * $ZRANGE_DIFF)" | bc -l)
ZRANDOM_NUMBER=$((RANDOM % ZRANGE + ZMIN))  
chime_sound $ZRANDOM_NUMBER 0.2 -5
sleep $ZRAND_FLOAT

done &
while [[ "$input" != "q" ]]; do

ZRAND_0_TO_1=$(echo "scale=2; $RANDOM / 32767" | bc -l)
ZRAND_FLOAT=$(echo "scale=2; $ZMINTIME + ($ZRAND_0_TO_1 * $ZRANGE_DIFF)" | bc -l)
ZRANDOM_NUMBER=$((RANDOM % ZRANGE + ZMIN))  
chime_sound $ZRANDOM_NUMBER 0.2 -5
sleep $ZRAND_FLOAT

done &
while [[ "$input" != "q" ]]; do

ZRAND_0_TO_1=$(echo "scale=2; $RANDOM / 32767" | bc -l)
ZRAND_FLOAT=$(echo "scale=2; $ZMINTIME + ($ZRAND_0_TO_1 * $ZRANGE_DIFF)" | bc -l)
ZRANDOM_NUMBER=$((RANDOM % ZRANGE + ZMIN))  
chime_sound $ZRANDOM_NUMBER 0.2 -5
sleep $ZRAND_FLOAT

done &
while [[ "$input" != "q" ]]; do

ZRAND_0_TO_1=$(echo "scale=2; $RANDOM / 32767" | bc -l)
ZRAND_FLOAT=$(echo "scale=2; $ZMINTIME + ($ZRAND_0_TO_1 * $ZRANGE_DIFF)" | bc -l)
ZRANDOM_NUMBER=$((RANDOM % ZRANGE + ZMIN))  
chime_sound $ZRANDOM_NUMBER 0.2 -5
sleep $ZRAND_FLOAT

done &
while [[ "$input" != "q" ]]; do

ZRAND_0_TO_1=$(echo "scale=2; $RANDOM / 32767" | bc -l)
ZRAND_FLOAT=$(echo "scale=2; $ZMINTIME + ($ZRAND_0_TO_1 * $ZRANGE_DIFF)" | bc -l)
ZRANDOM_NUMBER=$((RANDOM % ZRANGE + ZMIN))  
chime_sound $ZRANDOM_NUMBER 0.2 -5
sleep $ZRAND_FLOAT

done &





while true; do
echo "Enter 'q' to exit..."
read -n 1 input_char

if [[ "$input_char" == "q" ]]; then
    input="q"
    sleep 2
    pkill play
    pkill -f "delta_deep_sleep_asmr_rainy_rumble_bamboo_q.sh"
    sleep 2
    xkill
    sleep 2
    exit 0 # Exit with a success status
else
    echo ""
    echo "Continuing script."
    # Add the rest of your script's logic here
    echo "You entered: $input_char"
fi
done
wait
