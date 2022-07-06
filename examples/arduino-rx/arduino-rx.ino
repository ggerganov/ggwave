#include <ggwave.h>

#include <PDM.h>

const int kPinButton0 = 5;
const int kPinSpeaker = 10;

using TSample = int16_t;
const size_t kSampleSize_bytes = sizeof(TSample);

// default number of output channels
const char channels = 1;

// default PCM output sampleRate
const int sampleRate = 6000;
const int samplesPerFrame = 128;

const int qpow = 9;
const int qmax = 1 << qpow;

volatile int qhead = 0;
volatile int qtail = 0;
volatile int qsize = 0;

// buffer to read samples into, each sample is 16-bits
TSample sampleBuffer[qmax];

volatile int err = 0;

// global GGwave instance
GGWave ggwave;

// uncoment this to enable SSD1306 display output
#define DISPLAY_OUTPUT 1

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

// helper function to output the generated GGWave waveform via a buzzer
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
    while (!Serial);

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

    Serial.println(F("Trying to create ggwave instance"));

    ggwave.setLogFile(nullptr);

    {
        auto p = GGWave::getDefaultParameters();

        p.payloadLength   = 16;
        p.sampleRateInp   = sampleRate;
        p.sampleRateOut   = sampleRate;
        p.sampleRate      = sampleRate;
        p.samplesPerFrame = samplesPerFrame;
        p.sampleFormatInp = GGWAVE_SAMPLE_FORMAT_I16;
        p.sampleFormatOut = GGWAVE_SAMPLE_FORMAT_I16;
        p.operatingMode   = GGWAVE_OPERATING_MODE_RX | GGWAVE_OPERATING_MODE_TX | GGWAVE_OPERATING_MODE_USE_DSS | GGWAVE_OPERATING_MODE_TX_ONLY_TONES;

        GGWave::Protocols::tx().disableAll();
        GGWave::Protocols::tx().toggle(GGWAVE_PROTOCOL_DT_NORMAL,  true);
        GGWave::Protocols::tx().toggle(GGWAVE_PROTOCOL_DT_FAST,    true);
        GGWave::Protocols::tx().toggle(GGWAVE_PROTOCOL_DT_FASTEST, true);
        GGWave::Protocols::tx().toggle(GGWAVE_PROTOCOL_MT_NORMAL,  true);
        GGWave::Protocols::tx().toggle(GGWAVE_PROTOCOL_MT_FAST,    true);
        GGWave::Protocols::tx().toggle(GGWAVE_PROTOCOL_MT_FASTEST, true);

        GGWave::Protocols::rx().disableAll();
        GGWave::Protocols::rx().toggle(GGWAVE_PROTOCOL_DT_NORMAL,  true);
        GGWave::Protocols::rx().toggle(GGWAVE_PROTOCOL_DT_FAST,    true);
        GGWave::Protocols::rx().toggle(GGWAVE_PROTOCOL_DT_FASTEST, true);
        GGWave::Protocols::rx().toggle(GGWAVE_PROTOCOL_MT_NORMAL,  true);
        GGWave::Protocols::rx().toggle(GGWAVE_PROTOCOL_MT_FAST,    true);
        GGWave::Protocols::rx().toggle(GGWAVE_PROTOCOL_MT_FASTEST, true);


        ggwave.prepare(p);
        Serial.println(ggwave.heapSize());
    }

    delay(1000);

    Serial.println(F("Instance initialized"));

    {
        // Configure the data receive callback
        PDM.onReceive(onPDMdata);

        // Optionally set the gain
        // Defaults to 20 on the BLE Sense and -10 on the Portenta Vision Shields
        //PDM.setGain(30);

        // Initialize PDM with:
        // - one channel (mono mode)
        // - a 16 kHz sample rate for the Arduino Nano 33 BLE Sense
        // - a 32 kHz or 64 kHz sample rate for the Arduino Portenta Vision Shields
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

    while (true) {
        while (qsize >= samplesPerFrame) {
            auto tStart = millis();

            ggwave.decode(sampleBuffer + qhead, samplesPerFrame*kSampleSize_bytes);
            qsize -= samplesPerFrame;
            qhead += samplesPerFrame;
            if (qhead >= qmax) {
                qhead = 0;
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

        if (err > 0) {
            Serial.println(F("ERRROR"));
            Serial.println(err);
            err = 0;
        }

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
        // if you hit this error, try to increase qmax
        err += 10;

        qhead = 0;
        qtail = 0;
        qsize = 0;
    }

    PDM.read(sampleBuffer + qtail, bytesAvailable);

    qtail += nSamples;
    qsize += nSamples;

    if (qtail > qmax) {
        // if you hit this error, qmax is probably not a multiple of the recorded samples
        err += 1;
    }

    if (qtail >= qmax) {
        qtail -= qmax;
    }
}
