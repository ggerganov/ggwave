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
    GGWave::send_text(kPinSpeaker, "Hello!", GGWave::TX_ARDUINO_512_FASTEST);
    digitalWrite(kPinLed0, LOW);

    delay(2000);

    digitalWrite(kPinLed0, HIGH);
    GGWave::send_text(kPinSpeaker, "This is a",   GGWave::TX_ARDUINO_512_FASTEST);
    GGWave::send_text(kPinSpeaker, "ggwave demo", GGWave::TX_ARDUINO_512_FASTEST);
    digitalWrite(kPinLed0, LOW);

    delay(2000);

    digitalWrite(kPinLed0, HIGH);
    GGWave::send_text(kPinSpeaker, "The arduino",      GGWave::TX_ARDUINO_512_FASTEST);
    delay(200);
    GGWave::send_text(kPinSpeaker, "transmits data",   GGWave::TX_ARDUINO_512_FASTEST);
    delay(200);
    GGWave::send_text(kPinSpeaker, "using sound",      GGWave::TX_ARDUINO_512_FASTEST);
    delay(200);
    GGWave::send_text(kPinSpeaker, "through a buzzer", GGWave::TX_ARDUINO_512_FASTEST);
    digitalWrite(kPinLed0, LOW);

    delay(1000);

    digitalWrite(kPinLed0, HIGH);
    GGWave::send_text(kPinSpeaker, "The sound is", GGWave::TX_ARDUINO_512_FASTEST);
    delay(200);
    GGWave::send_text(kPinSpeaker, "decoded in a", GGWave::TX_ARDUINO_512_FASTEST);
    delay(200);
    GGWave::send_text(kPinSpeaker, "web page.",    GGWave::TX_ARDUINO_512_FASTEST);
    digitalWrite(kPinLed0, LOW);

    delay(1000);

    digitalWrite(kPinLed0, HIGH);
    GGWave::send_text(kPinSpeaker, "Press the button!", GGWave::TX_ARDUINO_512_FASTEST);
    digitalWrite(kPinLed0, LOW);
}

char txt[16];
int pressed = 0;
bool isDown = false;

void loop() {
    Serial.println("hello");

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
        GGWave::send_text(kPinSpeaker, txt, GGWave::TX_ARDUINO_512_FASTEST);
        digitalWrite(kPinLed0, LOW);
        pressed = 0;
    }
}
