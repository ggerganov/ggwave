#include "Arduino_AVRSTL.h"

#include "ggwave/ggwave.h"

const int kPinLed0    = 13;
const int kPinSpeaker = 10;
const int kPinButton0 = 2;
const int kPinButton1 = 4;

void setup() {
    Serial.begin(57600);
    display_freeram();

    //pinMode(kPinLed0,    OUTPUT);
    //pinMode(kPinSpeaker, OUTPUT);
    //pinMode(kPinButton0, INPUT);
    //pinMode(kPinButton1, INPUT);

    //delay(3000);

    //digitalWrite(kPinLed0, HIGH);
    //GGWave::send_text(kPinSpeaker, "Hello!", GGWave::TX_ARDUINO_512_FASTEST);
    //digitalWrite(kPinLed0, LOW);

    //delay(2000);

    //digitalWrite(kPinLed0, HIGH);
    //GGWave::send_text(kPinSpeaker, "This is a",   GGWave::TX_ARDUINO_512_FASTEST);
    //GGWave::send_text(kPinSpeaker, "ggwave demo", GGWave::TX_ARDUINO_512_FASTEST);
    //digitalWrite(kPinLed0, LOW);

    //delay(2000);

    //digitalWrite(kPinLed0, HIGH);
    //GGWave::send_text(kPinSpeaker, "The arduino",      GGWave::TX_ARDUINO_512_FASTEST);
    //delay(200);
    //GGWave::send_text(kPinSpeaker, "transmits data",   GGWave::TX_ARDUINO_512_FASTEST);
    //delay(200);
    //GGWave::send_text(kPinSpeaker, "using sound",      GGWave::TX_ARDUINO_512_FASTEST);
    //delay(200);
    //GGWave::send_text(kPinSpeaker, "through a buzzer", GGWave::TX_ARDUINO_512_FASTEST);
    //digitalWrite(kPinLed0, LOW);

    //delay(1000);

    //digitalWrite(kPinLed0, HIGH);
    //GGWave::send_text(kPinSpeaker, "The sound is", GGWave::TX_ARDUINO_512_FASTEST);
    //delay(200);
    //GGWave::send_text(kPinSpeaker, "decoded in a", GGWave::TX_ARDUINO_512_FASTEST);
    //delay(200);
    //GGWave::send_text(kPinSpeaker, "web page.",    GGWave::TX_ARDUINO_512_FASTEST);
    //digitalWrite(kPinLed0, LOW);

    //delay(1000);

    //digitalWrite(kPinLed0, HIGH);
    //GGWave::send_text(kPinSpeaker, "Press the button!", GGWave::TX_ARDUINO_512_FASTEST);
    //digitalWrite(kPinLed0, LOW);
}

char txt[16];
int pressed = 0;
bool isDown = false;

void display_freeram() {
  Serial.print(F("- SRAM left: "));
  Serial.println(freeRam());
}

int freeRam() {
    extern int __heap_start,*__brkval;
    int v;
    return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int) __brkval);
}

void loop() {
    Serial.println("hello");

    auto p = GGWave::getDefaultParameters();
    p.payloadLength   = 4;
    p.sampleRateInp   = 6000;
    p.sampleRateOut   = 6000;
    p.sampleRate      = 6000;
    p.samplesPerFrame = 128;
    p.sampleFormatInp = GGWAVE_SAMPLE_FORMAT_I16;
    p.sampleFormatOut = GGWAVE_SAMPLE_FORMAT_U8;
    p.operatingMode   = (ggwave_OperatingMode) (GGWAVE_OPERATING_MODE_TX | GGWAVE_OPERATING_MODE_TX_ONLY_TONES);

    delay(1000);
    GGWave::Protocols::kDefault() = {};
    Serial.println("aaaaaaaaaaaa");
    GGWave::Protocols::tx() = { { "[MT] Fastest", 24,  3, 1, 2, true, } };

    delay(1000);
    display_freeram();
    Serial.println("xxxxxxxxxxx");
    delay(1000);
    Serial.println("yyyyyyyyyyyy");
    GGWave ggwave(p);
    display_freeram();

    while (true) {
        delay(1000);
        Serial.println("working");
    }

    //int but0 = digitalRead(kPinButton0);
    //int but1 = digitalRead(kPinButton1);

    //if (but1 == LOW && isDown == false) {
    //    delay(200);
    //    ++pressed;
    //    isDown = true;
    //} else if (but1 == HIGH) {
    //    isDown = false;
    //}

    //if (but0 == LOW) {
    //    snprintf(txt, 16, "Pressed: %d", pressed);

    //    digitalWrite(kPinLed0, HIGH);
    //    GGWave::send_text(kPinSpeaker, txt, GGWave::TX_ARDUINO_512_FASTEST);
    //    digitalWrite(kPinLed0, LOW);
    //    pressed = 0;
    //}
}
