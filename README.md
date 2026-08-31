Audio Preview
https://whyp.it/tracks/351104/waternoize?token=cJMN6
<img width="320" height="180" alt="1000075498" src="[https://github.com/user-attachments/assets/439c9911-e4db-4982-b6f2-e03bd4c6b06c](https://github.com/user-attachments/assets/439c9911-e4db-4982-b6f2-e03bd4c6b06c)" />
<img width="1408" height="768" alt="1001322465" src="[https://github.com/user-attachments/assets/7544055d-2064-41de-8402-61dc877faf31](https://github.com/user-attachments/assets/7544055d-2064-41de-8402-61dc877faf31)" />
<img width="1915" height="821" alt="1001322590" src="[https://github.com/user-attachments/assets/c06dd451-1be6-4822-9879-2b12a10e0218](https://github.com/user-attachments/assets/c06dd451-1be6-4822-9879-2b12a10e0218)" />
This program will generate a synthetic babbling brook with delta binaural beats.
> Warning: Use headphones with caution!
> 
Press Ctrl+C at any time to exit.
Based on Algorithms
 * "Minnaert's formula"
 * Dillon Baston
 * Van Den Doel
 * Jos Stams
Note: No audio files are used; all sounds are synthetic and random.
Compilation
Compile on Linux or Android (Termux) with:
gcc -O3 waternoize_private.c -o waternoize -lasound -lm

Android (Termux) Requirements
apt-get update
apt-get install build-essential
apt-get install alsa-lib alsa-utils alsa-plugins

After the program has been compiled:
cp waternoize /data/data/com.termux/files/usr/bin
chmod +x /data/data/com.termux/files/usr/bin/waternoize

Run program with the command:
waternoize

Android 16 or Above Update
 * Download Simple Protocol Player from the Google Play Store.
 * Run the following commands in your terminal:
   cd ~/
cd ..
nano /usr/etc/pulse/default.pa

 * Paste the following line at the very bottom:
   load-module module-simple-protocol-tcp source=auto_null.monitor record=true port=12345 rate=44100

 * Save and exit (CTRL-X, Y, CTRL-M).
 * Start Simple Protocol Player and press the Play button.
<img width="1080" height="2400" alt="1001323926" src="[https://github.com/user-attachments/assets/27359259-643a-4ca0-bfca-f6a45a13c0f4](https://github.com/user-attachments/assets/27359259-643a-4ca0-bfca-f6a45a13c0f4)" />
Linux Requirements
apt-get update
sudo apt install build-essential
sudo apt install pipewire-audio-client-libraries pipewire-alsa
sudo apt install libasound2-dev

After the program has been compiled, run it with:
./waternoize

This program was created for relaxation/entertainment purposes.
Thank you!
<img width="500" height="500" alt="1001322599" src="[https://github.com/user-attachments/assets/ba6cb51e-282e-4c52-a2a6-18fd0ad1dd53](https://github.com/user-attachments/assets/ba6cb51e-282e-4c52-a2a6-18fd0ad1dd53)" />
