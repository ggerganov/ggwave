Experimental software, further tweaked by me. Look at the code mostly. If I do as per below: am I sending the whole file, line by line, indeed? : 


```
~/Downloads $ cat answer1.md 
Here's how to solve this problem:

**1. Understand the Initial Scenario**

* **Rope:** Has zero elasticity (perfectly inextensible).
* **Placement:** Placed tautly around the Earth's equator.
* **Length:** The length of the rope is the circumference of the Earth at the equator: $C = 2\pi R$.
* **Height:** The rope is exactly on the surface of the Earth ($h=0$).

**2. Understand the New Scenario**

* **New Shape:** The rope forms a ring 1m above the ground everywhere.
* **Radius:** The new radius ($R'$) is the Earth's radius plus the height: $R' = R + h = 6378 \text{ km} + 1 \text{ m}$.
* **New Circumference ($C'$):** $C' = 2\pi R'$.
* **New Length ($L'$):** This is the new required length of the rope.

**3. Calculate the New Length ($L'$)**

First, convert everything to meters:
$R = 6378 \times 10^3 \text{ m} = 6,378,000 \text{ m}$
$h = 1 \text{ m}$
$R' = 6,378,000 \text{ m} + 1 \text{ m} = 6,378,001 \text{ m}$

$C' = 2\pi R' = 2\pi (6,378,001 \text{ m}) \approx 40,071,800 \text{ m}$

**4. Calculate the Required Increase in Length**

The required extra length ($\Delta L$) is the difference between the new circumference and the original circumference:
$$\Delta L = C' - C$$
$$\Delta L = 2\pi (R' - R) = 2\pi (h) = 2\pi (1 \text{ m})$$
$$\Delta L = 2\pi \text{ m} \approx 6.283 \text{ m}$$

**5. Final Answer Formulation**

The required increase in length is simply the height of the ring above the ground ($h=1\text{ m}$), multiplied by 2 (for the circumference).

$$\text{Increase in length} = 2\pi h = 2\pi (1 \text{ m}) \approx 6.283 \text{ m}$$

---
**Answer:** The rope would need to be approximately **6.283 meters** longer to form a ring 1 meter above the ground everywhere.
~/Downloads $ cat answer1.md | ggwave-cli 
Usage: ggwave-cli [-cN] [-pN] [-tN] [-lN]
    -cN - select capture device N
    -pN - select playback device N
    -tN - transmission protocol
    -lN - fixed payload length of size N, N in [1, 64]
    -d  - use Direct Sequence Spread (DSS)
    -v  - print generated tones on resend

Examples:
    ggwave-cli                     (interactive mode)
    echo "hello" | ggwave-cli      (one-off transmission)

Found 2 playback devices:
    - Playback device #0: 'OpenSL ES Output'
    - Playback device #1: 'AAudio Output'
Found 0 capture devices:
Initializing playback ...
Attempt to open playback device 0 : 'OpenSL ES Output' ...
Obtained spec for output device (SDL Id = 2):
    - Sample rate:       48000 (required: 48000)
    - Format:            32784 (required: 32784)
    - Channels:          1 (required: 1)
    - Samples per frame: 16384 (required: 16384)
Attempt to open capture device 0 : '(null)' ...
Obtained spec for input device (SDL Id = 3):
    - Sample rate:       48000
    - Format:            33056 (required: 33056)
    - Channels:          1 (required: 1)
    - Samples per frame: 1024
Available Tx protocols:
      0 - Normal
      1 - Fast
      2 - Fastest
      3 - [U] Normal
      4 - [U] Fast
      5 - [U] Fastest
      6 - [DT] Normal
      7 - [DT] Fast
      8 - [DT] Fastest
      9 - [MT] Normal
      10 - [MT] Fast
      11 - [MT] Fastest
Selecting Tx protocol 1
Sending ...
Sending ...
Sending ...
Sending ...
Sending ...
Sending ...
Sending ...
Sending ...
Sending ...
Sending ...
Sending ...
Sending ...
Sending ...
Sending ...
Sending ...
Sending ...
Sending ...
Sending ...
Sending ...
Sending ...
Sending ...
Sending ...
Sending ...
Sending ...
Sending ...
Sending ...
Sending ...
~/Downloads $ cd GitHub/ggwave/
~/.../GitHub/ggwave $ 

```



FYI: 

~/.../GitHub/ggwave $  build/bin/ggwave-cli --help
Usage: build/bin/ggwave-cli [-cN] [-pN] [-tN] [-lN]
    -cN - select capture device N
    -pN - select playback device N
    -tN - transmission protocol
    -lN - fixed payload length of size N, N in [1, 64]
    -d  - use Direct Sequence Spread (DSS)
    -v  - print generated tones on resend

Examples:
    build/bin/ggwave-cli                     (interactive mode)
    echo "hello" | build/bin/ggwave-cli      (one-off transmission)

Found 2 playback devices:
    - Playback device #0: 'OpenSL ES Output'
    - Playback device #1: 'AAudio Output'
Found 0 capture devices:
Initializing playback ...
Attempt to open playback device 0 : 'OpenSL ES Output' ...
Obtained spec for output device (SDL Id = 2):
    - Sample rate:       48000 (required: 48000)
    - Format:            32784 (required: 32784)
    - Channels:          1 (required: 1)
    - Samples per frame: 16384 (required: 16384)
Attempt to open capture device 0 : '(null)' ...
Obtained spec for input device (SDL Id = 3):
    - Sample rate:       48000
    - Format:            33056 (required: 33056)
    - Channels:          1 (required: 1)
    - Samples per frame: 1024
Available Tx protocols:
      0 - Normal
      1 - Fast
      2 - Fastest
      3 - [U] Normal
      4 - [U] Fast
      5 - [U] Fastest
      6 - [DT] Normal
      7 - [DT] Fast
      8 - [DT] Fastest
      9 - [MT] Normal
      10 - [MT] Fast
      11 - [MT] Fastest
Selecting Tx protocol 1
Enter text: 


and: 

~/.../GitHub/ggwave $ ls build/bin
 arduino-rx-web   ggwave-cli   ggwave-from-file   ggwave-rx   ggwave-to-file   r2t2   r2t2-rx   spectrogram   test-ggwave-c   test-ggwave-cpp   waver
~/.../GitHub/ggwave $ 


