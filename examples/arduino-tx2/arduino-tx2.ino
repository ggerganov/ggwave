// To build this example for Arduino UNO, make sure to install the ArduinoSTL library:
//#include <ArduinoSTL.h>

#include "ggwave/ggwave.h"

const int kPinLed0    = 13;
const int kPinSpeaker = 10;
const int kPinButton0 = 2;
const int kPinButton1 = 4;

const int samplesPerFrame = 128;
const int sampleRate = 6000;

GGWave * g_ggwave = nullptr;

void send_text(GGWave & ggwave, uint8_t pin, const char * text, GGWave::TxProtocolId protocolId) {
    ggwave.init(text, protocolId);
    ggwave.encode();

    const auto & tones = ggwave.txTones();
    //Serial.println(tones.size());
    float freq_hz = -1.0f;
    //float frameDuration_ms = (1000.0*samplesPerFrame)/sampleRate;
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
        //duration_ms += frameDuration_ms;
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

    pinMode(kPinLed0,    OUTPUT);
    pinMode(kPinSpeaker, OUTPUT);
    pinMode(kPinButton0, INPUT);
    pinMode(kPinButton1, INPUT);

    Serial.println(F("Trying to create ggwave instance"));

    auto p = GGWave::getDefaultParameters();
    p.payloadLength   = 16;
    p.sampleRateInp   = sampleRate;
    p.sampleRateOut   = sampleRate;
    p.sampleRate      = sampleRate;
    p.samplesPerFrame = samplesPerFrame;
    p.sampleFormatInp = GGWAVE_SAMPLE_FORMAT_I16;
    p.sampleFormatOut = GGWAVE_SAMPLE_FORMAT_U8;
    p.operatingMode   = (ggwave_OperatingMode) (GGWAVE_OPERATING_MODE_TX | GGWAVE_OPERATING_MODE_TX_ONLY_TONES | GGWAVE_OPERATING_MODE_USE_DSS);

    GGWave::Protocols::tx().only(GGWAVE_PROTOCOL_MT_FASTEST);
    delay(1000);
    static GGWave ggwave(p);
    ggwave.setLogFile(nullptr);
    Serial.println(ggwave.heapSize());

    g_ggwave = &ggwave;

    Serial.println(F("Instance initialized"));
}

char txt[16];
int pressed = 0;
bool isDown = false;

void loop() {
    auto & ggwave = *g_ggwave;

    //Serial.println(F("hello"));

    delay(1000);

    ////Serial.println(F("sending .."));
    digitalWrite(kPinLed0, HIGH);
    send_text(ggwave, kPinSpeaker, "Hello!", GGWAVE_PROTOCOL_MT_FASTEST);
    digitalWrite(kPinLed0, LOW);
    ////Serial.println(F("done .."));

    //delay(2000);

    //digitalWrite(kPinLed0, HIGH);
    //send_text(ggwave, kPinSpeaker, "This is a",   GGWAVE_PROTOCOL_MT_FASTEST);
    //send_text(ggwave, kPinSpeaker, "ggwave demo", GGWAVE_PROTOCOL_MT_FASTEST);
    //digitalWrite(kPinLed0, LOW);

    //delay(2000);

    //digitalWrite(kPinLed0, HIGH);
    //send_text(ggwave, kPinSpeaker, "The arduino",      GGWAVE_PROTOCOL_MT_FASTEST);
    //delay(200);
    //send_text(ggwave, kPinSpeaker, "transmits data",   GGWAVE_PROTOCOL_MT_FASTEST);
    //delay(200);
    //send_text(ggwave, kPinSpeaker, "using sound",      GGWAVE_PROTOCOL_MT_FASTEST);
    //delay(200);
    //send_text(ggwave, kPinSpeaker, "through a buzzer", GGWAVE_PROTOCOL_MT_FASTEST);
    //digitalWrite(kPinLed0, LOW);

    //delay(1000);

    //digitalWrite(kPinLed0, HIGH);
    //send_text(ggwave, kPinSpeaker, "The sound is", GGWAVE_PROTOCOL_MT_FASTEST);
    //delay(200);
    //send_text(ggwave, kPinSpeaker, "decoded in a", GGWAVE_PROTOCOL_MT_FASTEST);
    //delay(200);
    //send_text(ggwave, kPinSpeaker, "web page.",    GGWAVE_PROTOCOL_MT_FASTEST);
    //digitalWrite(kPinLed0, LOW);

    //delay(1000);

    //digitalWrite(kPinLed0, HIGH);
    //send_text(ggwave, kPinSpeaker, "Press the button!", GGWAVE_PROTOCOL_MT_FASTEST);
    //digitalWrite(kPinLed0, LOW);

    //while (true) {
    //    int but0 = digitalRead(kPinButton0);
    //    int but1 = digitalRead(kPinButton1);

    //    if (but1 == LOW && isDown == false) {
    //        delay(200);
    //        ++pressed;
    //        isDown = true;
    //    } else if (but1 == HIGH) {
    //        isDown = false;
    //    }

    //    if (but0 == LOW) {
    //        snprintf(txt, 16, "Pressed: %d", pressed);

    //        digitalWrite(kPinLed0, HIGH);
    //        send_text(ggwave, kPinSpeaker, txt, GGWAVE_PROTOCOL_MT_FASTEST);
    //        digitalWrite(kPinLed0, LOW);
    //        pressed = 0;
    //    }
    //}
}
