<img width="500" height="500" alt="1000078709" src="https://github.com/user-attachments/assets/9366e333-1b56-4a01-8e4c-fddc89d5dcf4" />

# Audio Preview

https://youtube.com/shorts/BH82JcXv5JU?feature=shared

# This program will generate a synthetic babbling brook with delta binaural beats.
> Warning: Use headphones with caution!
> 
Press Ctrl+C or CTRL+\ at any time to exit.

# Based on Algorithms
 * Minnaert's formula
 * Dillon Baston
 * Van Den Doel
 * Jos Stams<br><br>
Note: No audio files are used; all sounds are synthetic and random.

# Compilation
Compile on Linux or Android (Termux) with:<br>
gcc -O3 waternoize_private.c -o waternoize -lasound -lm

# Android (Termux) Requirements
apt-get update<br>
apt-get install build-essential<br>
apt-get install alsa-lib alsa-utils alsa-plugins

# After the program has been compiled:
cp waternoize /data/data/com.termux/files/usr/bin<br>
chmod +x /data/data/com.termux/files/usr/bin/waternoize

# Run program with the command:
waternoize

# Android 16 or Above Update if sound does not work:
 * Download Simple Protocol Player from the Google Play Store.<br>
 * Run the following commands in your terminal:<br>
   cd ~/<br>
cd ..<br>
nano /usr/etc/pulse/default.pa

 * Paste the following line at the very bottom:<br>
   load-module module-simple-protocol-tcp source=auto_null.monitor record=true port=12345 rate=44100

 * Save and exit (CTRL-X, Y, CTRL-M).<br>
 * Start Simple Protocol Player and press the Play button.

# Linux Requirements
apt-get update<br>
sudo apt install build-essential<br>
sudo apt install pipewire-audio-client-libraries pipewire-alsa<br>
sudo apt install libasound2-dev

# After the program has been compiled, run it with:
./waternoize

# This program was created for relaxation/entertainment purposes.<br>
Thank you!
