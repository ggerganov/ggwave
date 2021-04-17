# r2t2

Transmit that via the motherboard speaker

To use this program, you need to attach a [piezo speaker/buzzer](https://en.wikipedia.org/wiki/Piezoelectric_speaker) to your motherboard. Most servers already have one attached.

You can then run the following command:

```bash
echo test | sudo r2t2
```

This will transmit the message "test" via sound through the buzzer. Here is what it sounds like:

To receive the transmitted message, open the following page on your phone and keep it close to the speaker:

https://r2t2.ggerganov.com

## Build

```bash
git clone https://github.com/ggerganov/ggwave --recursive
cd ggwave
mkdir build && cd build
make
./bin/r2t2
```
