#define kPinLed 13
#define kPinSpeaker 11

void setup() {
    pinMode(kPinLed,     OUTPUT);
    pinMode(kPinSpeaker, OUTPUT);

    randomSeed(analogRead(0));
}

void loop() {
    if (1) {
        tone(kPinSpeaker,  1125.00); delay(  64);
        tone(kPinSpeaker,   937.50); delay(  64);
        tone(kPinSpeaker,   984.38); delay(  64);
        tone(kPinSpeaker,  1031.25); delay(  64);
        tone(kPinSpeaker,  1312.50); delay(  64);
        tone(kPinSpeaker,  1031.25); delay(  64);
        tone(kPinSpeaker,  1312.50); delay(  64);
        tone(kPinSpeaker,  1031.25); delay(  64);
        tone(kPinSpeaker,  1453.12); delay(  64);
        tone(kPinSpeaker,  1031.25); delay(  64);
        tone(kPinSpeaker,   750.00); delay(  64);
        tone(kPinSpeaker,   843.75); delay(  64);
        tone(kPinSpeaker,   937.50); delay(  64);
        tone(kPinSpeaker,   984.38); delay(  64);
        tone(kPinSpeaker,  1078.12); delay( 128);
        tone(kPinSpeaker,  1171.88); delay(  64);
        tone(kPinSpeaker,  1031.25); delay(  64);
        tone(kPinSpeaker,   937.50); delay(  64);
        tone(kPinSpeaker,  1078.12); delay(  64);
        tone(kPinSpeaker,   937.50); delay(  64);
        tone(kPinSpeaker,  1078.12); delay(  64);
        tone(kPinSpeaker,   984.38); delay(  64);
        tone(kPinSpeaker,  1031.25); delay(  64);
        tone(kPinSpeaker,   843.75); delay(  64);
        tone(kPinSpeaker,  1078.12); delay(  64);
        tone(kPinSpeaker,   796.88); delay(  64);
        tone(kPinSpeaker,   843.75); delay(  64);
        tone(kPinSpeaker,   796.88); delay(  64);
        tone(kPinSpeaker,  1453.12); delay(  64);
        tone(kPinSpeaker,   796.88); delay(  64);
        tone(kPinSpeaker,  1171.88); delay(  64);
        tone(kPinSpeaker,  1125.00); delay(  64);
        tone(kPinSpeaker,  1265.62); delay(  64);
        tone(kPinSpeaker,  1031.25); delay(  64);
        tone(kPinSpeaker,  1312.50); delay(  64);
    }

    noTone(kPinSpeaker);
    delay(3000);
}
