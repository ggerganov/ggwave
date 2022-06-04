#include "ggwave/ggwave.h"

#include <PDM.h>

using TSample = int16_t;
static const size_t kSampleSize_bytes = sizeof(TSample);

// default number of output channels
static const char channels = 1;

// default PCM output frequency
static const int frequency = 6000;

static const int qpow = 9;
static const int qmax = 1 << qpow;

volatile int qhead = 0;
volatile int qtail = 0;
volatile int qsize = 0;

// Buffer to read samples into, each sample is 16-bits
TSample sampleBuffer[qmax];

void setup() {
    Serial.begin(57600);
    while (!Serial);

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
        Serial.println("Failed to start PDM!");
        while (1);
    }
}

volatile int err = 0;

void loop() {
    Serial.println("trying to create ggwave instance");

    auto p = GGWave::getDefaultParameters();
    p.sampleRateInp = frequency;
    p.sampleRateOut = frequency;
    p.sampleRate = frequency;
    p.sampleFormatInp = GGWAVE_SAMPLE_FORMAT_I16;
    p.samplesPerFrame = 128;
    p.payloadLength = 16;
    p.operatingMode = (ggwave_OperatingMode) (GGWAVE_OPERATING_MODE_RX | GGWAVE_OPERATING_MODE_TX | GGWAVE_OPERATING_MODE_USE_DSS | GGWAVE_OPERATING_MODE_TX_ONLY_TONES);

    {
        auto & protocols = GGWave::getTxProtocols();
        for (auto & p : protocols) {
            p.enabled = false;
        }
        protocols[GGWAVE_TX_PROTOCOL_MT_FASTEST].enabled = true;
        protocols[GGWAVE_TX_PROTOCOL_DT_FASTEST].enabled = true;
    }

    GGWave ggwave(p);
    ggwave.setRxProtocols({
            //ggwave.getTxProtocol(GGWAVE_TX_PROTOCOL_MT_FASTEST),
            ggwave.getTxProtocol(GGWAVE_TX_PROTOCOL_DT_FASTEST),
            });

    Serial.println("Instance initialized");

    int nr = 0;
    int niter = 0;

    GGWave::TxRxData result;
    while (true) {
        while (qsize >= p.samplesPerFrame) {
            auto tStart = millis();

            //Serial.print(qhead);
            //Serial.print(" ");
            //Serial.print(qtail);
            //Serial.print(" ");
            //Serial.print(qsize);
            //Serial.print(" ");
            //Serial.print(ggwave.getSamplesNeeded());
            //Serial.println("");

            ggwave.decode(sampleBuffer + qhead, p.samplesPerFrame*kSampleSize_bytes);
            qsize -= p.samplesPerFrame;
            qhead += p.samplesPerFrame;
            if (qhead >= qmax) {
                qhead = 0;
            }

            auto tEnd = millis();
            if (++niter % 10 == 0) {
                Serial.println(tEnd - tStart);
            }

            nr = ggwave.takeRxData(result);
            if (nr > 0) {
                Serial.println(tEnd - tStart);
                Serial.println(nr);
                Serial.println((char *)result.data());

                if (strcmp((char *)result.data(), "test") == 0) {
                    ggwave.init("hello", ggwave.getTxProtocol(GGWAVE_TX_PROTOCOL_MT_FASTEST));
                    ggwave.encode();

                    const auto & tones = ggwave.txTones();
                    for (int i = 0; i < (int) tones.size(); ++i) {
                        Serial.print(" - frame ");
                        Serial.print(i);
                        Serial.print(", ");
                        Serial.print(tones[i].size());
                        Serial.print(": ");
                        for (int j = 0; j < (int) tones[i].size(); ++j) {
                            Serial.print((int)(tones[i][j].freq_hz));
                            Serial.print(" ");
                        }
                        Serial.println();
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
