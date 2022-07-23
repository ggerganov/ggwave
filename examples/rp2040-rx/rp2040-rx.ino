// rp2040-rx
//
// Sample sketch for receiving sound data using "ggwave"
//
// Tested MCU boards:
//   - Raspberry Pi Pico
//   - Arduino Nano RP2040 Connect
//
// Tested analog microphones:
//   - MAX9814
//   - KY-037
//   - KY-038
//   - WS Sound sensor
//
// The RP2040 microcontroller has a built-in 12-bit ADC which is used to digitalize the analog signal
// from the external analog microphone. The MCU supports sampling rates up to 500kHz which makes it
// capable of even recording audio in the ultrasound range, given that your microphone's sensitivity
// supports it.
//
// If you want to perform a quick test, you can use the free "Waver" application:
//   - Web:     https://waver.ggerganov.com
//   - Android: https://play.google.com/store/apps/details?id=com.ggerganov.Waver
//   - iOS:     https://apps.apple.com/us/app/waver-data-over-sound/id1543607865
//
// Make sure to enable the "Fixed-length" option in "Waver"'s settings and set the number of
// bytes to be equal to "payloadLength" used in the sketch. Also, select a protocol that is
// listed as Rx in the current sketch.
//
// Sketch: https://github.com/ggerganov/ggwave/tree/master/examples/rp2040-rx
//
// ## Pinout
//
// ### Analog Microphone
//
// | MCU     | Mic       |
// | ------- | --------- |
// | GND     | GND       |
// | 3.3V    | VCC / VDD |
// | GPIO 26 | Out       |
//

// Uncomment the line coresponding to your microhpone
#define MIC_ANALOG

// Uncoment this line to enable long-range transmission
// The used protocols are slower and use more memory to decode, but are much more robust
//#define LONG_RANGE 1

#include <ggwave.h>

// Audio capture configuration
using TSample = int16_t;

const size_t kSampleSize_bytes = sizeof(TSample);

// High sample rate - better quality, but more CPU/Memory usage
const int sampleRate = 48000;
const int samplesPerFrame = 1024;

// Low sample rate
//const int sampleRate = 24000;
//const int samplesPerFrame = 512;

TSample sampleBuffer[samplesPerFrame];

#if defined(MIC_ANALOG)

#include "mic-analog.h"

volatile int samplesRead = 0;

const struct analog_microphone_config config = {
    // GPIO to use for input, must be ADC compatible (GPIO 26 - 28)
    .gpio = 26,

    // bias voltage of microphone in volts
    .bias_voltage = 1.25,

    // sample rate in Hz
    .sample_rate = sampleRate,

    // number of samples to buffer
    .sample_buffer_size = samplesPerFrame,
};

void on_analog_samples_ready()
{
    // callback from library when all the samples in the library
    // internal sample buffer are ready for reading
    samplesRead = analog_microphone_read(sampleBuffer, samplesPerFrame);
}

#endif

// Global GGwave instance
GGWave ggwave;

void setup() {
    Serial.begin(115200);
    while (!Serial);

    // Initialize "ggwave"
    {
        Serial.println(F("Trying to initialize the ggwave instance"));

        ggwave.setLogFile(nullptr);

        auto p = GGWave::getDefaultParameters();

        // Adjust the "ggwave" parameters to your needs.
        // Make sure that the "payloadLength" parameter matches the one used on the transmitting side.
#ifdef LONG_RANGE
        // The "FAST" protocols require 2x more memory, so we reduce the payload length to compensate:
        p.payloadLength   = 8;
#else
        p.payloadLength   = 16;
#endif
        Serial.print(F("Using payload length: "));
        Serial.println(p.payloadLength);

        p.sampleRateInp   = sampleRate;
        p.sampleRateOut   = sampleRate;
        p.sampleRate      = sampleRate;
        p.samplesPerFrame = samplesPerFrame;
        p.sampleFormatInp = GGWAVE_SAMPLE_FORMAT_I16;
        p.sampleFormatOut = GGWAVE_SAMPLE_FORMAT_U8;
        p.operatingMode   = GGWAVE_OPERATING_MODE_RX | GGWAVE_OPERATING_MODE_TX | GGWAVE_OPERATING_MODE_USE_DSS | GGWAVE_OPERATING_MODE_TX_ONLY_TONES;

        // Protocols to use for TX
        // Remove the ones that you don't need to reduce memory usage
        GGWave::Protocols::tx().disableAll();
        //GGWave::Protocols::tx().toggle(GGWAVE_PROTOCOL_MT_NORMAL,  true);
        //GGWave::Protocols::tx().toggle(GGWAVE_PROTOCOL_MT_FAST,    true);
        GGWave::Protocols::tx().toggle(GGWAVE_PROTOCOL_MT_FASTEST, true);

        // Protocols to use for RX
        // Remove the ones that you don't need to reduce memory and CPU usage
        GGWave::Protocols::rx().disableAll();

        //GGWave::Protocols::rx().toggle(GGWAVE_PROTOCOL_AUDIBLE_NORMAL, true);
        //GGWave::Protocols::rx().toggle(GGWAVE_PROTOCOL_DT_NORMAL,      true);
        //GGWave::Protocols::rx().toggle(GGWAVE_PROTOCOL_MT_NORMAL,      true);

        //GGWave::Protocols::rx().toggle(GGWAVE_PROTOCOL_AUDIBLE_NORMAL, true);
        //GGWave::Protocols::rx().toggle(GGWAVE_PROTOCOL_DT_NORMAL,      true);
        //GGWave::Protocols::rx().toggle(GGWAVE_PROTOCOL_MT_NORMAL,      true);

        //GGWave::Protocols::rx().toggle(GGWAVE_PROTOCOL_ULTRASOUND_NORMAL,  true);
        //GGWave::Protocols::rx().toggle(GGWAVE_PROTOCOL_ULTRASOUND_FAST,    true);
        //GGWave::Protocols::rx().toggle(GGWAVE_PROTOCOL_ULTRASOUND_FASTEST, true);

#ifdef LONG_RANGE
        GGWave::Protocols::rx().toggle(GGWAVE_PROTOCOL_AUDIBLE_FAST, true);
        GGWave::Protocols::rx().toggle(GGWAVE_PROTOCOL_DT_FAST,      true);
        GGWave::Protocols::rx().toggle(GGWAVE_PROTOCOL_MT_FAST,      true);
#endif

        GGWave::Protocols::rx().toggle(GGWAVE_PROTOCOL_AUDIBLE_FASTEST, true);
        GGWave::Protocols::rx().toggle(GGWAVE_PROTOCOL_DT_FASTEST,      true);
        GGWave::Protocols::rx().toggle(GGWAVE_PROTOCOL_MT_FASTEST,      true);

        // Print the memory required for the "ggwave" instance:
        ggwave.prepare(p, false);

        Serial.print(F("Required memory by the ggwave instance: "));
        Serial.print(ggwave.heapSize());
        Serial.println(F(" bytes"));

        // Initialize the "ggwave" instance:
        ggwave.prepare(p, true);
        Serial.print(F("Instance initialized successfully! Memory used: "));
    }

    // initialize the analog microphone
    if (analog_microphone_init(&config) < 0) {
        Serial.println(F("analog microphone initialization failed!"));
        while (1) { tight_loop_contents(); }
    }

    // set callback that is called when all the samples in the library
    // internal sample buffer are ready for reading
    analog_microphone_set_samples_ready_handler(on_analog_samples_ready);

    // start capturing data from the analog microphone
    if (analog_microphone_start() < 0) {
        Serial.println(F("Analog microphone start failed!"));
        while (1) { tight_loop_contents();  }
    }

    Serial.println(F("setup() done"));
}

int niter = 0;
int tLastReceive = -10000;

GGWave::TxRxData result;

void loop() {
    // wait for new samples
    while (samplesRead == 0) { tight_loop_contents(); }

    // store and clear the samples read from the callback
    int nSamples = samplesRead;
    samplesRead = 0;

    // Use this with the serial plotter to observe real-time audio signal
    //for (int i = 0; i < nSamples; i++) {
    //    Serial.printf("%d\n", sampleBuffer[i]);
    //}

    // Try to decode any "ggwave" data:
    auto tStart = millis();

    if (ggwave.decode(sampleBuffer, samplesPerFrame*kSampleSize_bytes) == false) {
        Serial.println("Failed to decode");
    }

    auto tEnd = millis();

    if (++niter % 10 == 0) {
        // print the time it took the last decode() call to complete
        // should be smaller than samplesPerFrame/sampleRate seconds
        // for example: samplesPerFrame = 128, sampleRate = 6000 => not more than 20 ms
        Serial.println(tEnd - tStart);
        if (tEnd - tStart > 1000*(float(samplesPerFrame)/sampleRate)) {
            Serial.println(F("Warning: decode() took too long to execute!"));
        }
    }

    // Check if we have successfully decoded any data:
    int nr = ggwave.rxTakeData(result);
    if (nr > 0) {
        Serial.println(tEnd - tStart);
        Serial.print(F("Received data with length "));
        Serial.print(nr); // should be equal to p.payloadLength
        Serial.println(F(" bytes:"));

        Serial.println((char *) result.data());

        tLastReceive = tEnd;
    }
}
