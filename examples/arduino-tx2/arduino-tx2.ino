#include "ggwave/ggwave.h"

const int kPinLed0    = 13;
const int kPinSpeaker = 10;
const int kPinButton0 = 2;
const int kPinButton1 = 4;

const int samplesPerFrame = 128;
const int sampleRate      = 6000;

// global GGwave instance
GGWave ggwave;

char txt[64];
#define P(str) (strcpy_P(txt, PSTR(str)), txt)

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
    ggwave.prepare(p);
    ggwave.setLogFile(nullptr);
    Serial.println(ggwave.heapSize());

    Serial.println(F("Instance initialized"));
}

int pressed = 0;
bool isDown = false;

void loop() {
    delay(1000);

    digitalWrite(kPinLed0, HIGH);
    send_text(ggwave, kPinSpeaker, P("Hello!"), GGWAVE_PROTOCOL_MT_FASTEST);
    digitalWrite(kPinLed0, LOW);

    delay(2000);

    digitalWrite(kPinLed0, HIGH);
    send_text(ggwave, kPinSpeaker, P("This is a"),   GGWAVE_PROTOCOL_MT_FASTEST);
    send_text(ggwave, kPinSpeaker, P("ggwave demo"), GGWAVE_PROTOCOL_MT_FASTEST);
    digitalWrite(kPinLed0, LOW);

    delay(2000);

    digitalWrite(kPinLed0, HIGH);
    send_text(ggwave, kPinSpeaker, P("The arduino"),      GGWAVE_PROTOCOL_MT_FASTEST);
    delay(200);
    send_text(ggwave, kPinSpeaker, P("transmits data"),   GGWAVE_PROTOCOL_MT_FASTEST);
    delay(200);
    send_text(ggwave, kPinSpeaker, P("using sound"),      GGWAVE_PROTOCOL_MT_FASTEST);
    delay(200);
    send_text(ggwave, kPinSpeaker, P("through a buzzer"), GGWAVE_PROTOCOL_MT_FASTEST);
    digitalWrite(kPinLed0, LOW);

    delay(1000);

    digitalWrite(kPinLed0, HIGH);
    send_text(ggwave, kPinSpeaker, P("The sound is"), GGWAVE_PROTOCOL_MT_FASTEST);
    delay(200);
    send_text(ggwave, kPinSpeaker, P("decoded in a"), GGWAVE_PROTOCOL_MT_FASTEST);
    delay(200);
    send_text(ggwave, kPinSpeaker, P("web page."),    GGWAVE_PROTOCOL_MT_FASTEST);
    digitalWrite(kPinLed0, LOW);

    delay(1000);

    digitalWrite(kPinLed0, HIGH);
    send_text(ggwave, kPinSpeaker, P("Press the button!"), GGWAVE_PROTOCOL_MT_FASTEST);
    digitalWrite(kPinLed0, LOW);

    Serial.println(F("Starting main loop"));

    while (true) {
        int but0 = digitalRead(kPinButton0);
        int but1 = digitalRead(kPinButton1);

        if (but1 == LOW && isDown == false) {
            delay(200);
            ++pressed;
            isDown = true;
        } else if (but1 == HIGH) {
            isDown = false;
        }

        if (but0 == LOW) {
            snprintf(txt, 16, "Pressed: %d", pressed);

            digitalWrite(kPinLed0, HIGH);
            send_text(ggwave, kPinSpeaker, txt, GGWAVE_PROTOCOL_MT_FASTEST);
            digitalWrite(kPinLed0, LOW);
            pressed = 0;
        }
    }
}
