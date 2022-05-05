#include "ggwave.h"

#define kPinLed0 13
#define kPinSpeaker 10

void setup() {
    pinMode(kPinLed0,    OUTPUT);
    pinMode(kPinSpeaker, OUTPUT);
}

char txt[64]; // used for printf

void loop() {
    char tx[16];
    memset(tx, 0, sizeof(tx));
    strcpy(tx, "Hello World!");

    digitalWrite(kPinLed0, HIGH);
    GGWave::send(kPinSpeaker, tx, GGWave::TX_ARDUINO_512_FASTEST);
    digitalWrite(kPinLed0, LOW);

    delay(250);

    digitalWrite(kPinLed0, HIGH);
    GGWave::send(kPinSpeaker, "This is GGWave!!", GGWave::TX_ARDUINO_512_FASTEST);
    digitalWrite(kPinLed0, LOW);

    delay(250);

    digitalWrite(kPinLed0, HIGH);
    GGWave::send(kPinSpeaker, "This is NORMAL", GGWave::TX_ARDUINO_512_NORMAL);
    digitalWrite(kPinLed0, LOW);

    delay(250);

    digitalWrite(kPinLed0, HIGH);
    GGWave::send(kPinSpeaker, "This is FAST", GGWave::TX_ARDUINO_512_FAST);
    digitalWrite(kPinLed0, LOW);

    delay(250);

    digitalWrite(kPinLed0, HIGH);
    GGWave::send(kPinSpeaker, "This is FASTEST", GGWave::TX_ARDUINO_512_FASTEST);
    digitalWrite(kPinLed0, LOW);

    delay(5000);
}
