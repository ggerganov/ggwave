# r2t2

Transmit data with the PC speaker

This is a command-line program that encodes short messages/data into audio and plays it via the motherboard's PC speaker. To use this tool, you need to attach a [piezo speaker/buzzer](https://en.wikipedia.org/wiki/Piezoelectric_speaker) to your motherboard. Some computers already have such speaker already attached.

You can then run the following command:

```bash
echo test | sudo r2t2
```

This will transmit the message "test" via sound through the buzzer.
To receive the transmitted message, open the following page on your phone and keep it close to the speaker:

https://r2t2.ggerganov.com

## Requirements

- [Buzzer](https://www.amazon.com/SoundOriginal-Motherboard-Internal-Speaker-Buzzer/dp/B01DM56TFY/ref=sr_1_1_sspa?dchild=1&keywords=Motherboard+Speaker&qid=1614504288&sr=8-1-spons&psc=1&spLa=ZW5jcnlwdGVkUXVhbGlmaWVyPUEzTkpFVlk4SzRXS1lWJmVuY3J5cHRlZElkPUEwOTU3NzI3MkpCQUZJRFIxSzZGNSZlbmNyeXB0ZWRBZElkPUEwODk0ODQ4MlVBQzFSR1RHMTYyMiZ3aWRnZXROYW1lPXNwX2F0ZiZhY3Rpb249Y2xpY2tSZWRpcmVjdCZkb05vdExvZ0NsaWNrPXRydWU=)
- Unix operating system
- The program requires to run as `sudo` in order to access the PC speaker

## Build

```bash
git clone https://github.com/ggerganov/ggwave --recursive
cd ggwave
mkdir build && cd build
make
./bin/r2t2
```
