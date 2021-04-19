# r2t2

Transmit data with the PC speaker

<a href="https://user-images.githubusercontent.com/1991296/115141782-cba9f480-a046-11eb-9462-791477b856f5.mp4"><img width="100%" src="https://user-images.githubusercontent.com/1991296/115141739-a1583700-a046-11eb-94e7-a411d52ecf30.png"></img></a>
<p align="center">
  <i>Vid. <b>r2t2</b> demonstration</i>
</p>

This is a command-line program that encodes short messages/data into audio and plays it via the motherboard's PC speaker. To use this tool, you need to attach a [piezo speaker/buzzer](https://en.wikipedia.org/wiki/Piezoelectric_speaker) to your motherboard. Some computers have such speaker already attached.

You can then run the following commands:

```bash
# transmit the message "test" with default protocol "[R2T2] Normal"
echo test | sudo r2t2

# transmit the message "hello" with protocol "[R2T2] Fast"
echo hello | sudo r2t2 -t10

# transmit the message "foo bar" with protocol "[R2T2] Fastest"
echo "foo bar" | sudo r2t2 -t11
```

To receive the transmitted message, open the following page on your phone and place it near the speaker:

https://r2t2.ggerganov.com

## Applications

This tool can be useful when you need to transmit data from air-gapped machines. The hardware requirements are very cheap - you only need a PC speaker. For example, you can make an automated script to periodically emit some sensor data, which can be received by someone nearby running the `r2t2` receiver application.

## Requirements

- [PC speaker / buzzer](https://www.amazon.com/SoundOriginal-Motherboard-Internal-Speaker-Buzzer/dp/B01DM56TFY/ref=sr_1_1_sspa?dchild=1&keywords=Motherboard+Speaker&qid=1614504288&sr=8-1-spons&psc=1&spLa=ZW5jcnlwdGVkUXVhbGlmaWVyPUEzTkpFVlk4SzRXS1lWJmVuY3J5cHRlZElkPUEwOTU3NzI3MkpCQUZJRFIxSzZGNSZlbmNyeXB0ZWRBZElkPUEwODk0ODQ4MlVBQzFSR1RHMTYyMiZ3aWRnZXROYW1lPXNwX2F0ZiZhY3Rpb249Y2xpY2tSZWRpcmVjdCZkb05vdExvZ0NsaWNrPXRydWU=) attached to the motherboard.
  
  Here are the ones that I use:
  
<p align="center">
    <table border=0>
<tr>
<td>
    <img width="100%" alt="Talking buttons" src="https://user-images.githubusercontent.com/1991296/115141260-ee86d980-a043-11eb-9699-587e0af53af9.jpg"></img>
</td>
<td>
 <img width="100%" alt="Talking buttons" src="https://user-images.githubusercontent.com/1991296/115141261-f0509d00-a043-11eb-82cf-a89040b51f13.jpg"></img>
</td>
</tr>
</table>
</p>
<p align="center">
  <i>Img. Left: PC speaker plugged into a motherboard. Right: two PC speakers with a coin for size comparison</i>
</p>

- Unix operating system
- Add the `pcspkr` kernel module: `sudo modprobe pcspkr`
- The program requires to run as `sudo` in order to access the PC speaker

## Build

```bash
git clone https://github.com/ggerganov/ggwave --recursive
cd ggwave
mkdir build && cd build
make
./bin/r2t2
```

## Acknowledgements

Thanks to [Radoslav Gerganov](https://github.com/rgerganov) for this cool idea!
