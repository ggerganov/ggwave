#include "ggwave/ggwave.h"

#include <PDM.h>

// default number of output channels
static const char channels = 1;

// default PCM output frequency
static const int frequency = 6000;

const int qmax = 1024;
volatile int qhead = 0;
volatile int qtail = 0;
volatile int qsize = 0;

// Buffer to read samples into, each sample is 16-bits
short sampleBuffer[qmax];

// Number of audio samples read
//volatile int samplesRead = 0;

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
    Serial.println("hello4");

    //delay(1000);

    Serial.println("trying to create ggwave instance");
    //delay(1000);
    auto p = GGWave::getDefaultParameters();
    p.sampleRateInp = frequency;
    p.sampleRate = frequency;
    p.sampleFormatInp = GGWAVE_SAMPLE_FORMAT_I16;
    p.samplesPerFrame = 128;
    p.payloadLength = 16;
    p.operatingMode = GGWAVE_OPERATING_MODE_ONLY_RX;
    GGWave instance(p);
    instance.setRxProtocols({
            { GGWAVE_TX_PROTOCOL_DT_FASTEST, instance.getTxProtocol(GGWAVE_TX_PROTOCOL_DT_FASTEST) },
            });
    Serial.println("Instance initialized");

    static GGWave::CBWaveformInp cbWaveformInp = [&](void * data, uint32_t nMaxBytes) {
        if (2*qsize < nMaxBytes) {
            return 0;
        }
        //Serial.println(nMaxBytes);
        //Serial.println(qsize);
        int nCopied = std::min((uint32_t) 2*qsize, nMaxBytes);
        //Serial.println(qsize);
        qsize -= nCopied / 2;
        //Serial.println(nCopied);
        //Serial.println("---------");
        for (int i = 0; i < nCopied/2; ++i) {
            //if (i == 0) Serial.println(sampleBuffer[qhead]);
            //data[i] = sampleBuffer[qhead];
            memcpy(((char *)data) + 2*i, (char *)(sampleBuffer + qhead), 2);
            qhead = (qhead + 1) % qmax;
        }
        //std::copy((char *) sampleBuffer, ((char *) sampleBuffer) + nCopied, (char *) data);
        return nCopied;
    };

    int nr = 0;
    GGWave::TxRxData result;
    while (true) {
        //Serial.println(sampleBuffer[10]);
        if (qsize >= 512) {
            //Serial.println(sampleBuffer[10]);
            //Serial.println(qsize);
            instance.decode(cbWaveformInp);
            nr = instance.takeRxData(result);
            if (nr > 0) {
                Serial.println(nr);
                Serial.println((char *)result.data());
            }
            //samplesRead = 0;
        }
        if (err > 0) {
            Serial.println("ERRROR");
            Serial.println(err);
        }
    }
}

/**
  Callback function to process the data from the PDM microphone.
NOTE: This callback is executed as part of an ISR.
Therefore using `Serial` to print messages inside this function isn't supported.
 * */
void onPDMdata() {
    // Query the number of available bytes
    int bytesAvailable = PDM.available();
    int ns = bytesAvailable / 2;
    if (qsize + ns > qmax) {
        qhead = 0;
        qtail = 0;
        qsize = 0;
    }

    // Read into the sample buffer
    PDM.read(sampleBuffer + qtail, bytesAvailable);

    qtail += ns;
    qsize += ns;
    if (qtail > qmax) {
        ++err;
    }
    if (qtail >= qmax) {
        qtail -= qmax;
    }
}
