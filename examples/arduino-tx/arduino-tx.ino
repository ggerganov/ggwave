// This example uses a custom ggwave imlpementation specifically for Arduino UNO.
// Since the Arduino UNO has only 2KB of RAM, the ggwave library is not able to
// to fit into the Arduino's memory (eventhough it is very close).
// If your microcontroller has more than 2KB of RAM, you should check the other Tx
// examples to see if you can use the original ggwave library.
#include "ggwave.h"

const int kPinLed0    = 13;
const int kPinSpeaker = 10;
const int kPinButton0 = 2;
const int kPinButton1 = 4;

void setup() {
    Serial.begin(57600);

    pinMode(kPinLed0,    OUTPUT);
    pinMode(kPinSpeaker, OUTPUT);
    pinMode(kPinButton0, INPUT);
    pinMode(kPinButton1, INPUT);

    delay(3000);

    digitalWrite(kPinLed0, HIGH);
    GGWave::send_text(kPinSpeaker, "Hello!", GGWave::GGWAVE_PROTOCOL_MT_FASTEST);
    digitalWrite(kPinLed0, LOW);

    delay(2000);

    digitalWrite(kPinLed0, HIGH);
    GGWave::send_text(kPinSpeaker, "This is a",   GGWave::GGWAVE_PROTOCOL_MT_FASTEST);
    GGWave::send_text(kPinSpeaker, "ggwave demo", GGWave::GGWAVE_PROTOCOL_MT_FASTEST);
    digitalWrite(kPinLed0, LOW);

    delay(2000);

    digitalWrite(kPinLed0, HIGH);
    GGWave::send_text(kPinSpeaker, "The arduino",      GGWave::GGWAVE_PROTOCOL_MT_FASTEST);
    delay(200);
    GGWave::send_text(kPinSpeaker, "transmits data",   GGWave::GGWAVE_PROTOCOL_MT_FASTEST);
    delay(200);
    GGWave::send_text(kPinSpeaker, "using sound",      GGWave::GGWAVE_PROTOCOL_MT_FASTEST);
    delay(200);
    GGWave::send_text(kPinSpeaker, "through a buzzer", GGWave::GGWAVE_PROTOCOL_MT_FASTEST);
    digitalWrite(kPinLed0, LOW);

    delay(1000);

    digitalWrite(kPinLed0, HIGH);
    GGWave::send_text(kPinSpeaker, "The sound is", GGWave::GGWAVE_PROTOCOL_MT_FASTEST);
    delay(200);
    GGWave::send_text(kPinSpeaker, "decoded in a", GGWave::GGWAVE_PROTOCOL_MT_FASTEST);
    delay(200);
    GGWave::send_text(kPinSpeaker, "web page.",    GGWave::GGWAVE_PROTOCOL_MT_FASTEST);
    digitalWrite(kPinLed0, LOW);

    delay(1000);

    digitalWrite(kPinLed0, HIGH);
    GGWave::send_text(kPinSpeaker, "Press the button!", GGWave::GGWAVE_PROTOCOL_MT_FASTEST);
    digitalWrite(kPinLed0, LOW);
}

char txt[16];
int pressed = 0;
bool isDown = false;

void loop() {
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
        GGWave::send_text(kPinSpeaker, txt, GGWave::GGWAVE_PROTOCOL_MT_FASTEST);
        digitalWrite(kPinLed0, LOW);
        pressed = 0;
    }
}
