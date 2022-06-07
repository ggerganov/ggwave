#include "ggwave/ggwave.h"

#include <PDM.h>

const int kPinSpeaker = 10;

using TSample = int16_t;
static const size_t kSampleSize_bytes = sizeof(TSample);

// default number of output channels
static const char channels = 1;

// default PCM output frequency
static const int frequency = 6000;
static const int samplesPerFrame = 128;

static const int qpow = 9;
static const int qmax = 1 << qpow;

volatile int qhead = 0;
volatile int qtail = 0;
volatile int qsize = 0;

// Buffer to read samples into, each sample is 16-bits
TSample sampleBuffer[qmax];

volatile int err = 0;

GGWave * g_ggwave = nullptr;

// helper function to output the generated GGWave waveform via a buzzer
void send_text(GGWave & ggwave, uint8_t pin, const char * text, GGWave::TxProtocolId protocolId) {
    ggwave.init(text, protocolId);
    ggwave.encode();

    const auto & tones = ggwave.txTones();
    float freq_hz = -1.0f;
    float duration_ms = -1.0f;
    for (int i = 0; i < (int) tones.size(); ++i) {
        if (tones[i].size() == 0) continue;
        const auto & curTone = tones[i].front();

        if (curTone.freq_hz != freq_hz) {
            if (duration_ms > 0) {
                tone(pin, freq_hz);
                delay(duration_ms);
            }
            freq_hz = curTone.freq_hz;
            duration_ms = 0.0f;
        }
        duration_ms += curTone.duration_ms;
    }

    if (duration_ms > 0) {
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

    // Configure the data receive callback
    PDM.onReceive(onPDMdata);

    // Optionally set the gain
    // Defaults to 20 on the BLE Sense and -10 on the Portenta Vision Shields
    //PDM.setGain(30);

    // Initialize PDM with:
    // - one channel (mono mode)
    // - a 16 kHz sample rate for the Arduino Nano 33 BLE Sense
    // - a 32 kHz or 64 kHz sample rate for the Arduino Portenta Vision Shields
    if (!PDM.begin(channels, frequency)) {
        Serial.println(F("Failed to start PDM!"));
        while (1);
    }

    Serial.println(F("Trying to create ggwave instance"));

    auto p = GGWave::getDefaultParameters();

    p.payloadLength   = 16;
    p.sampleRateInp   = frequency;
    p.sampleRateOut   = frequency;
    p.sampleRate      = frequency;
    p.samplesPerFrame = samplesPerFrame;
    p.sampleFormatInp = GGWAVE_SAMPLE_FORMAT_I16;
    p.sampleFormatOut = GGWAVE_SAMPLE_FORMAT_I16;
    p.operatingMode   = (ggwave_OperatingMode) (GGWAVE_OPERATING_MODE_RX | GGWAVE_OPERATING_MODE_TX | GGWAVE_OPERATING_MODE_USE_DSS | GGWAVE_OPERATING_MODE_TX_ONLY_TONES);

    GGWave::Protocols::tx().only({GGWAVE_PROTOCOL_MT_FASTEST, GGWAVE_PROTOCOL_DT_FASTEST});
    GGWave::Protocols::rx().only({GGWAVE_PROTOCOL_MT_FASTEST, GGWAVE_PROTOCOL_DT_FASTEST});

    static GGWave ggwave(p);
    ggwave.setLogFile(nullptr);

    g_ggwave = &ggwave;

    Serial.println(F("Instance initialized"));
}

void loop() {
    auto & ggwave = *g_ggwave;

    int nr = 0;
    int niter = 0;

    GGWave::TxRxData result;
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
                // should be smaller than samplesPerFrame/frequency seconds
                // for example: samplesPerFrame = 128, frequency = 6000 => not more than 20 ms
                Serial.println(tEnd - tStart);
                if (tEnd - tStart > 1000*(float(samplesPerFrame)/frequency)) {
                    Serial.println("Warning: decode() took too long to execute!");
                }
            }

            nr = ggwave.rxTakeData(result);
            if (nr > 0) {
                Serial.println(tEnd - tStart);
                Serial.print("Received data with length ");
                Serial.print(nr); // should be equal to p.payloadLength
                Serial.println(" bytes:");

                Serial.println((char *) result.data());

                if (strcmp((char *)result.data(), "test") == 0) {
                    // pause microphone capture while transmitting
                    PDM.end();
                    delay(500);

                    send_text(ggwave, kPinSpeaker, "hello", GGWAVE_PROTOCOL_MT_FASTEST);

                    // resume microphone capture
                    if (!PDM.begin(channels, frequency)) {
                        Serial.println("Failed to start PDM!");
                        while (1);
                    }
                }
            }
        }

        if (err > 0) {
            Serial.println("ERRROR");
            Serial.println(err);
            err = 0;
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
