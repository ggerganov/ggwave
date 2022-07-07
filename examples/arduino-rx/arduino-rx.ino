// arduino-rx
//
// Sample sketch for receiving data using "ggwave"
//
// Tested with:
//   - Arduino Nano RP2040 Connect
//
// The Arduino Nano RP2040 Connect board has a built-in microphone which is used
// in this example to capture audio data.
//
// The sketch optionally supports displaying the received "ggwave" data on an OLED display.
// Use the DISPLAY_OUTPUT macro to enable or disable this functionality.
//
// If you don't have a display, you can simply observe the decoded data in the serial monitor.
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
// Demo: https://youtu.be/HiDpGvnxPLs
//
// Sketch: https://github.com/ggerganov/ggwave/tree/master/examples/arduino-rx
//

// Uncoment this line to enable SSD1306 display output
//#define DISPLAY_OUTPUT 1

#include <ggwave.h>

#include <PDM.h>

// Pin configuration
const int kPinButton0 = 5;
const int kPinSpeaker = 10;

// Audio capture configuration
using TSample = int16_t;
const size_t kSampleSize_bytes = sizeof(TSample);

const char channels        = 1;
const int  sampleRate      = 6000;
const int  samplesPerFrame = 128;

// Audio capture ring-buffer
const int qpow = 9;
const int qmax = 1 << qpow;

volatile int qhead = 0;
volatile int qtail = 0;
volatile int qsize = 0;

TSample sampleBuffer[qmax];

// Error handling
volatile int err = 0;

// Global GGwave instance
GGWave ggwave;

#ifdef DISPLAY_OUTPUT

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
// The pins for I2C are defined by the Wire-library.
// On an arduino UNO:       A4(SDA), A5(SCL)
// On an arduino MEGA 2560: 20(SDA), 21(SCL)
// On an arduino LEONARDO:   2(SDA),  3(SCL), ...
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#endif

// Helper function to output the generated GGWave waveform via a buzzer
void send_text(GGWave & ggwave, uint8_t pin, const char * text, GGWave::TxProtocolId protocolId) {
    Serial.print(F("Sending text: "));
    Serial.println(text);

    ggwave.init(text, protocolId);
    ggwave.encode();

    const auto & protocol = GGWave::Protocols::tx()[protocolId];
    const auto tones = ggwave.txTones();
    const auto duration_ms = protocol.txDuration_ms(ggwave.samplesPerFrame(), ggwave.sampleRateOut());
    for (auto & curTone : tones) {
        const auto freq_hz = (protocol.freqStart + curTone)*ggwave.hzPerSample();
        tone(pin, freq_hz);
        delay(duration_ms);
    }

    noTone(pin);
    digitalWrite(pin, LOW);
}

void setup() {
    Serial.begin(57600);
    //while (!Serial);

    pinMode(kPinSpeaker, OUTPUT);
    pinMode(kPinButton0, INPUT_PULLUP);

#ifdef DISPLAY_OUTPUT
    {
        // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
        if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
            Serial.println(F("SSD1306 allocation failed"));
            for(;;); // Don't proceed, loop forever
        }

        // Show initial display buffer contents on the screen --
        // the library initializes this with an Adafruit splash screen.
        //display.display();
        //delay(2000); // Pause for 2 seconds

        // Clear the buffer
        display.clearDisplay();

        display.setTextSize(2);
        display.setTextColor(SSD1306_WHITE); // Draw white text
        display.setCursor(0, 0);     // Start at top-left corner
        display.println(F("GGWave!"));
        display.setTextSize(1);
        display.println(F(""));
        display.println(F("Listening..."));

        display.display();
    }
#endif

    // Initialize "ggwave"
    {
        Serial.println(F("Trying to initialize the ggwave instance"));

        ggwave.setLogFile(nullptr);

        auto p = GGWave::getDefaultParameters();

        // Adjust the "ggwave" parameters to your needs.
        // Make sure that the "payloadLength" parameter matches the one used on the transmitting side.
        p.payloadLength   = 16;
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
        //GGWave::Protocols::tx().toggle(GGWAVE_PROTOCOL_DT_NORMAL,  true);
        //GGWave::Protocols::tx().toggle(GGWAVE_PROTOCOL_DT_FAST,    true);
        //GGWave::Protocols::tx().toggle(GGWAVE_PROTOCOL_DT_FASTEST, true);
        //GGWave::Protocols::tx().toggle(GGWAVE_PROTOCOL_MT_NORMAL,  true);
        //GGWave::Protocols::tx().toggle(GGWAVE_PROTOCOL_MT_FAST,    true);
        GGWave::Protocols::tx().toggle(GGWAVE_PROTOCOL_MT_FASTEST, true);

        // Protocols to use for RX
        // Remove the ones that you don't need to reduce memory usage
        GGWave::Protocols::rx().disableAll();
        GGWave::Protocols::rx().toggle(GGWAVE_PROTOCOL_DT_NORMAL,  true);
        GGWave::Protocols::rx().toggle(GGWAVE_PROTOCOL_DT_FAST,    true);
        GGWave::Protocols::rx().toggle(GGWAVE_PROTOCOL_DT_FASTEST, true);
        GGWave::Protocols::rx().toggle(GGWAVE_PROTOCOL_MT_NORMAL,  true);
        GGWave::Protocols::rx().toggle(GGWAVE_PROTOCOL_MT_FAST,    true);
        GGWave::Protocols::rx().toggle(GGWAVE_PROTOCOL_MT_FASTEST, true);

        // Initialize the ggwave instance and print the memory usage
        ggwave.prepare(p);

        Serial.print(F("Instance initialized successfully! Memory used: "));
        Serial.print(ggwave.heapSize());
        Serial.println(F(" bytes"));
    }

    // Start capturing audio
    {
        // Configure the data receive callback
        PDM.onReceive(onPDMdata);

        // Optionally set the gain
        // Defaults to 20 on the BLE Sense and -10 on the Portenta Vision Shields
        //PDM.setGain(30);

        // Initialize PDM:
        if (!PDM.begin(channels, sampleRate)) {
            Serial.println(F("Failed to start PDM!"));
            while (1);
        }
    }
}

void loop() {
    int nr = 0;
    int niter = 0;
    int but0Prev = HIGH;

    GGWave::TxRxData result;
    char resultLast[17];

    // Main loop ..
    while (true) {
        while (qsize >= samplesPerFrame) {
            // We have enough captured samples - try to decode any "ggwave" data:
            auto tStart = millis();

            ggwave.decode(sampleBuffer + qhead, samplesPerFrame*kSampleSize_bytes);
            qsize -= samplesPerFrame;
            qhead += samplesPerFrame;
            if (qhead >= qmax) {
                qhead = 0;
            }

            auto tEnd = millis();
            if (++niter % 10 == 0) {
                // Print the time it took the last decode() call to complete.
                // The time should be smaller than samplesPerFrame/sampleRate seconds
                // For example: samplesPerFrame = 128, sampleRate = 6000 => not more than 20 ms
                Serial.println(tEnd - tStart);
                if (tEnd - tStart > 1000*(float(samplesPerFrame)/sampleRate)) {
                    Serial.println(F("Warning: decode() took too long to execute!"));
                }
            }

            // Check if we have successfully decoded any data:
            nr = ggwave.rxTakeData(result);
            if (nr > 0) {
                Serial.println(tEnd - tStart);
                Serial.print(F("Received data with length "));
                Serial.print(nr); // should be equal to p.payloadLength
                Serial.println(F(" bytes:"));

                Serial.println((char *) result.data());

#ifdef DISPLAY_OUTPUT
                {
                    display.clearDisplay();

                    display.setTextSize(2);
                    display.setTextColor(SSD1306_WHITE);
                    display.setCursor(0, 0);
                    display.println((char *) result.data());

                    display.display();
                }
#endif
                strcpy(resultLast, (char *) result.data());
            }
        }

        // This should never happen.
        // If it does - there is something wrong with the audio capturing callback.
        // For example, the microcontroller is not able to process the captured data in real-time.
        if (err > 0) {
            Serial.println(F("ERRROR"));
            Serial.println(err);
            err = 0;
        }

        // If the button has been presse - transmit the last received data:
        int but0 = digitalRead(kPinButton0);
        if (but0 == LOW && but0Prev == HIGH) {
            Serial.println(F("Button 0 pressed - transmitting .."));

            {
                // pause microphone capture while transmitting
                PDM.end();
                delay(500);

                send_text(ggwave, kPinSpeaker, resultLast, GGWAVE_PROTOCOL_MT_FASTEST);

                // resume microphone capture
                if (!PDM.begin(channels, sampleRate)) {
                    Serial.println(F("Failed to start PDM!"));
                    while (1);
                }
            }

            Serial.println(F("Done"));

            but0Prev = LOW;
        } else if (but0 == HIGH && but0Prev == LOW) {
            but0Prev = HIGH;
        }
    }
}

/**
  Callback function to process the data from the PDM microphone.
NOTE: This callback is executed as part of an ISR.
Therefore using `Serial` to print messages inside this function isn't supported.
 * */
void onPDMdata() {
    const int bytesAvailable = PDM.available();
    const int nSamples = bytesAvailable/kSampleSize_bytes;

    if (qsize + nSamples > qmax) {
        // If you hit this error, try to increase qmax
        err += 10;

        qhead = 0;
        qtail = 0;
        qsize = 0;
    }

    PDM.read(sampleBuffer + qtail, bytesAvailable);

    qtail += nSamples;
    qsize += nSamples;

    if (qtail > qmax) {
        // If you hit this error, qmax is probably not a multiple of the recorded samples
        err += 1;
    }

    if (qtail >= qmax) {
        qtail -= qmax;
    }
}
