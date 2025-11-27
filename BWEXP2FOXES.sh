#!/bin/bash
#author: alex terranova
#date:11/08/2025
#name:rumblefishzen
#version:3.7X
#description: Brainwave-Entrainment
input=""
ZMIN=500
ZMAX=1900
ZMINTIME=1
ZMAXTIME=3
ZRANGE_DIFF=$(echo "$ZMAXTIME - $ZMINTIME" | bc -l)
ZRANGE=$((ZMAX - ZMIN + 1)) 

function chime_soundr() {
    local freq=$1
    local duration=$2
    local gain=$3

    play -q -n  -r 44100   -c1  synth "$duration" sine "$freq" fade 0.001 0 0.2 "$duration" lowpass 100 vol 1 remix 0 1 > /dev/null 2>&1 &
}
function chime_soundl() {
    local freq=$1
    local duration=$2
    local gain=$3


    play -q -n  -r 44100  -c1  synth "$duration" sine "$freq" fade 0.001 0 0.2 "$duration" lowpass 100 vol 1 remix 1 0 > /dev/null 2>&1 &
}

MIN=2000
MAX=4000

MINTIME=0.2
MAXTIME=0.3

RANGE_DIFF=$(echo "$MAXTIME - $MINTIME" | bc -l)

RANGE=$((MAX - MIN + 1))

# Define variables
BEAT_FREQUENCY=1  # Delta wave beat (e.g., 3 Hz)
CARRIER_FREQUENCY=45  # Base frequency for one channel (e.g., 150 Hz)

# Calculate the second frequency for the binaural beat
FREQUENCY_RIGHT=$((CARRIER_FREQUENCY + BEAT_FREQUENCY))

echo "Now Playing: Brainwave Entrainment"
# Generate and play the binaural beat using the `play` command
#play -q -n synth \
#     sine $CARRIER_FREQUENCY \
 #    sine $FREQUENCY_RIGHT \
#     reverb 50 50 100 100 20 vol 0.1 > /dev/null 2>&1 & 

play -q  -n synth brownnoise gain -20 vol 1 reverb 50 50 100 100 20 lowpass 200 highpass 600 &

function chimexr() {
while [[ "$input" != "q" ]]; do

ZRAND_0_TO_1=$(echo "scale=2; $RANDOM / 32767" | bc -l)
ZRAND_FLOAT=$(echo "scale=2; $ZMINTIME + ($ZRAND_0_TO_1 * $ZRANGE_DIFF)" | bc -l)
ZRANDOM_NUMBER=$((RANDOM % ZRANGE + ZMIN))  
chime_soundr $ZRANDOM_NUMBER 0.2 0
sleep $ZRAND_FLOAT

done &

}
function chimexl() {
while [[ "$input" != "q" ]]; do

ZRAND_0_TO_1=$(echo "scale=2; $RANDOM / 32767" | bc -l)
ZRAND_FLOAT=$(echo "scale=2; $ZMINTIME + ($ZRAND_0_TO_1 * $ZRANGE_DIFF)" | bc -l)
ZRANDOM_NUMBER=$((RANDOM % ZRANGE + ZMIN))  
chime_soundl $ZRANDOM_NUMBER 0.2 0
sleep $ZRAND_FLOAT

done &

}

chimexr &
chimexr &
chimexl &
chimexl &

function bubblyr() {
while [[ "$input" != "q" ]]; do

minimin=500
minimax=600
minirange=$((minimax - minimin + 1))
mini_random_number=$((RANDOM % minirange + minimin))
START_FREQ=$mini_random_number

miniminz=100
minimaxz=500
minirangez=$((minimaxz - miniminz + 1))
mini_random_numberz=$((RANDOM % minirangez + miniminz))

miniminy=-1000
minimaxy=-500
minirangey=$((minimaxy - miniminy + 1))
mini_random_numbery=$((RANDOM % minirangey + miniminy))


MIND=100
MAXD=500
# Calculate the range size
RANGED=$(($MAXD - $MIND + 1))

# Generate the random number
DD=$(echo "scale=0; $RANDOM % $RANGED + $MIND" | bc -l)
  
random_int=$RANDOM

# Scale the integer to a range of 0 to 8000 (0.8 * 10000)
# and add 1000 (0.1 * 10000) to shift the range
scaled_int=$(( (random_int % 8000) + 1000 ))

# Divide by 10000 to get a floating-point number between 0.1 and 0.9
random_float=$(echo "scale=1; $scaled_int / 10000" | bc)



DURATION=$( echo "scale=2; 0.03 + 0.1 * $RANDOM / 32767" | bc )

RANDOM_NUMBER=$((RANDOM % RANGE + MIN))
play -q -n  -r 44100   synth $DURATION \
  sine "${START_FREQ}-${RANDOM_NUMBER}" \
  vol $random_float \
  fade h 0.01 $DURATION 0.05 \
  gain -15 \
  tremolo 0.3 100 \
  phaser 0.2 0.3 0.03 0.1 0.8 \
  reverb 50 50 50 100 100 \
  flanger 0 4 2 50 1 100 \
  bass +5 100 \
  highpass $mini_random_numberz \
  pitch $mini_random_numbery \
  remix 0 1 \
  lowpass $DD > /dev/null 2>&1 

  sleep $( echo "scale=2; 0.2 + 1 * $RANDOM / 32767" | bc )
  
done &
}
function bubblyl() {
while [[ "$input" != "q" ]]; do


minimin=500
minimax=600
minirange=$((minimax - minimin + 1))
mini_random_number=$((RANDOM % minirange + minimin))
START_FREQ=$mini_random_number

miniminz=100
minimaxz=500
minirangez=$((minimaxz - miniminz + 1))
mini_random_numberz=$((RANDOM % minirangez + miniminz))

miniminy=-1000
minimaxy=-500
minirangey=$((minimaxy - miniminy + 1))
mini_random_numbery=$((RANDOM % minirangey + miniminy))

MIND=100
MAXD=500
# Calculate the range size
RANGED=$(($MAXD - $MIND + 1))

# Generate the random number
DD=$(echo "scale=0; $RANDOM % $RANGED + $MIND" | bc -l)
  
random_int=$RANDOM

# Scale the integer to a range of 0 to 8000 (0.8 * 10000)
# and add 1000 (0.1 * 10000) to shift the range
scaled_int=$(( (random_int % 8000) + 1000 ))

# Divide by 10000 to get a floating-point number between 0.1 and 0.9
random_float=$(echo "scale=1; $scaled_int / 10000" | bc)


DURATION=$( echo "scale=2; 0.03 + 0.1 * $RANDOM / 32767" | bc )

RANDOM_NUMBER=$((RANDOM % RANGE + MIN))
play -q -n  -r 44100   synth $DURATION \
  sine "${START_FREQ}-${RANDOM_NUMBER}" \
  vol $random_float \
  fade h 0.01 $DURATION 0.05 \
  gain -15 \
  tremolo 0.3 100 \
  phaser 0.2 0.3 0.03 0.1 0.8 \
  reverb 50 50 50 100 100 \
  flanger 0 4 2 50 1 100 \
  bass +5 100 \
  pitch $mini_random_numbery \
  highpass $mini_random_numberz \
  remix 1 0 \
  lowpass $DD > /dev/null 2>&1 
  

  sleep $( echo "scale=2; 0.2 + 1 * $RANDOM / 32767" | bc )
  

  
done &
}
function bubblylr() {
while [[ "$input" != "q" ]]; do

minimin=500
minimax=600
minirange=$((minimax - minimin + 1))
mini_random_number=$((RANDOM % minirange + minimin))
START_FREQ=$mini_random_number

miniminz=100
minimaxz=500
minirangez=$((minimaxz - miniminz + 1))
mini_random_numberz=$((RANDOM % minirangez + miniminz))

miniminy=-1000
minimaxy=-500
minirangey=$((minimaxy - miniminy + 1))
mini_random_numbery=$((RANDOM % minirangey + miniminy))

MIND=100
MAXD=500
# Calculate the range size
RANGED=$(($MAXD - $MIND + 1))

# Generate the random number
DD=$(echo "scale=0; $RANDOM % $RANGED + $MIND" | bc -l)
  
random_int=$RANDOM

# Scale the integer to a range of 0 to 8000 (0.8 * 10000)
# and add 1000 (0.1 * 10000) to shift the range
scaled_int=$(( (random_int % 8000) + 1000 ))

# Divide by 10000 to get a floating-point number between 0.1 and 0.9
random_float=$(echo "scale=1; $scaled_int / 10000" | bc)


DURATION=$( echo "scale=2; 0.03 + 0.1 * $RANDOM / 32767" | bc )

RANDOM_NUMBER=$((RANDOM % RANGE + MIN))
play -q -n  -r 44100  synth $DURATION \
  sine "${START_FREQ}-${RANDOM_NUMBER}" \
  vol $random_float \
  fade h 0.01 $DURATION 0.05 \
  tremolo 0.3 100 \
  phaser 0.2 0.3 0.03 0.1 0.8 \
  gain -15 \
  reverb 50 50 50 100 100 \
  flanger 0 4 2 50 1 100 \
  bass +5 100 \
  highpass $mini_random_numberz \
  remix 1 1 \
  lowpass $DD > /dev/null 2>&1 
   
  sleep $( echo "scale=2; 0.2 + 1  * $RANDOM / 32767" | bc )
  
done &
}

bubblyr &
bubblyr &
bubblyr &
bubblyr &
bubblyr &
bubblyr &
bubblyr &
bubblyr &
bubblyr &
bubblyr &
bubblyr &
bubblyr &


bubblyl &
bubblyl &
bubblyl &
bubblyl &
bubblyl &
bubblyl &
bubblyl &
bubblyl &
bubblyl &
bubblyl &
bubblyl &
bubblyl &

bubblylr &
bubblylr &
bubblylr &
bubblylr &
bubblylr &
bubblylr &
bubblylr &
bubblylr &
bubblylr &
bubblylr &
bubblylr &
bubblylr &

while true; do
echo "Enter 'q' to exit..."
read -n 1 input_char

if [[ "$input_char" == "q" ]]; then
    input="q"
    sleep 2
    pkill play
    pkill -f "BWEXP2FOXES.sh"
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

sleep 1
done
wait
