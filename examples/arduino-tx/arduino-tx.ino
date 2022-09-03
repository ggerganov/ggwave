// arduino-tx
//
// Sample sketch for transmitting data using "ggwave"
//
// Tested with:
//   - Arduino Uno R3
//   - Arduino Nano RP2040 Connect
//   - NodeMCU-ESP32-S
//   - NodeMCU-ESP8266EX
//
// If you want to perform a quick test, you can use the free "Waver" application:
//   - Web:     https://waver.ggerganov.com
//   - Android: https://play.google.com/store/apps/details?id=com.ggerganov.Waver
//   - iOS:     https://apps.apple.com/us/app/waver-data-over-sound/id1543607865
//
// Make sure to enable the "Fixed-length" option in "Waver"'s settings and set the number of
// bytes to be equal to "payloadLength" used in the sketch.
//
// Demo: https://youtu.be/qbzKo3zbQcI
//
// Sketch: https://github.com/ggerganov/ggwave/tree/master/examples/arduino-tx
//

#include <ggwave.h>

// Pin configuration
const int kPinLed0    = 13;
const int kPinSpeaker = 10;
const int kPinButton0 = 2;
const int kPinButton1 = 4;

const int samplesPerFrame = 128;
const int sampleRate      = 6000;

// Global GGwave instance
GGWave ggwave;

char txt[64];
#define P(str) (strcpy_P(txt, PSTR(str)), txt)

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
    while (!Serial);

    pinMode(kPinLed0,    OUTPUT);
    pinMode(kPinSpeaker, OUTPUT);
    pinMode(kPinButton0, INPUT);
    pinMode(kPinButton1, INPUT);

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
        p.operatingMode   = GGWAVE_OPERATING_MODE_TX | GGWAVE_OPERATING_MODE_TX_ONLY_TONES | GGWAVE_OPERATING_MODE_USE_DSS;

        // Protocols to use for TX
        GGWave::Protocols::tx().only(GGWAVE_PROTOCOL_MT_FASTEST);

        // Sometimes, the speaker might not be able to produce frequencies in the Mono-tone range of 1-2 kHz.
        // In such cases, you can shift the base frequency up by changing the "freqStart" parameter of the protocol.
        // Make sure that in the receiver (for example, the "Waver" app) the base frequency is shifted by the same amount.
        // Here we shift the frequency by +48 bins. Each bin is equal to 48000/1024 = 46.875 Hz.
        // So the base frequency is shifted by +2250 Hz.
        //GGWave::Protocols::tx()[GGWAVE_PROTOCOL_MT_FASTEST].freqStart += 48;

        // Initialize the ggwave instance and print the memory usage
        ggwave.prepare(p);
        Serial.println(ggwave.heapSize());

        Serial.println(F("Instance initialized successfully!"));
    }
}

// Button state
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
