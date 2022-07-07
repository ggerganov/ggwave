// esp32-rx
//
// Sample sketch for receiving data using "ggwave"
//
// Tested with:
//   - NodeMCU-ESP32-S
//
// Tested microphones:
//   - MAX9814
//   - KY-037
//   - KY-038
//   - WS Sound sensor
//
// The ESP32 microcontroller has a built-int 12-bit ADC which is used to digitalize the analog signal
// from the external microphone.
//
// The sketch optionally supports displaying the received "ggwave" data on an OLED display.
// Use the DISPLAY_OUTPUT macro to enable or disable this functionality.
//
// If you don't have a display, you can simply observe the decoded data in the serial monitor.
//
// If you want to perform a quick test, you can use the free "Waver" application:
//   - Web:     https://waver.ggerganov.com
//   - Android: https://play.google.com/store/apps/details?id=com.ggerganov.Waver
//   - iOS:     https://apps.apple.com/us/app/waver-data-over-sound/id1543607865
//
// Make sure to enable the "Fixed-length" option in "Waver"'s settings and set the number of
// bytes to be equal to "payloadLength" used in the sketch. Also, select a protocol that is
// listed as Rx in the current sketch.
//
// Demo: https://youtu.be/38JoMwdpH6I
//
// Sketch: https://github.com/ggerganov/ggwave/tree/master/examples/esp32-rx
//

// Uncoment this line to enable SSD1306 display output
//#define DISPLAY_OUTPUT 1

#include <ggwave.h>

#include <soc/adc_channel.h>
#include <driver/i2s.h>

// Global GGwave instance
GGWave ggwave;

// Audio capture configuration
using TSample = int16_t;
const size_t kSampleSize_bytes = sizeof(TSample);

// High sample rate - better quality, but more CPU/Memory usage
const int sampleRate = 24000;
const int samplesPerFrame = 512;

// Low sample rate
// Only MT protocols will work in this mode
//const int sampleRate = 12000;
//const int samplesPerFrame = 256;

TSample sampleBuffer[samplesPerFrame];

// ADC configuration
const i2s_port_t     i2s_port    = I2S_NUM_0;
const adc_unit_t     adc_unit    = ADC_UNIT_1;
const adc1_channel_t adc_channel = ADC1_GPIO35_CHANNEL;

// i2s config for using the internal ADC
const i2s_config_t adc_i2s_config = {
    .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_ADC_BUILT_IN),
    .sample_rate          = sampleRate,
    .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format       = I2S_CHANNEL_FMT_ONLY_RIGHT,
    .communication_format = I2S_COMM_FORMAT_I2S_LSB,
    .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count        = 4,
    .dma_buf_len          = samplesPerFrame,
    .use_apll             = false,
    .tx_desc_auto_clear   = false,
    .fixed_mclk           = 0
};

#ifdef DISPLAY_OUTPUT

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
// The pins for I2C are defined by the Wire-library.
// On an arduino UNO:       A4(SDA), A5(SCL)
// On an arduino MEGA 2560: 20(SDA), 21(SCL)
// On an arduino LEONARDO:   2(SDA),  3(SCL), ...
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#endif

void setup() {
    Serial.begin(115200);
    while (!Serial);

#ifdef DISPLAY_OUTPUT
    {
        // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
        if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
            Serial.println(F("SSD1306 allocation failed"));
            for(;;); // Don't proceed, loop forever
        }

        // Show initial display buffer contents on the screen --
        // the library initializes this with an Adafruit splash screen.
        //display.display();
        //delay(2000); // Pause for 2 seconds

        // Clear the buffer
        display.clearDisplay();

        display.setTextSize(2);
        display.setTextColor(SSD1306_WHITE); // Draw white text
        display.setCursor(0, 0);     // Start at top-left corner
        display.println(F("GGWave!"));
        display.setTextSize(1);
        display.println(F(""));
        display.println(F("Listening..."));

        display.display();
    }
#endif

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
        p.operatingMode   = GGWAVE_OPERATING_MODE_RX | GGWAVE_OPERATING_MODE_TX | GGWAVE_OPERATING_MODE_USE_DSS | GGWAVE_OPERATING_MODE_TX_ONLY_TONES;

        // Protocols to use for TX
        // Remove the ones that you don't need to reduce memory usage
        GGWave::Protocols::tx().disableAll();
        //GGWave::Protocols::tx().toggle(GGWAVE_PROTOCOL_MT_NORMAL,  true);
        //GGWave::Protocols::tx().toggle(GGWAVE_PROTOCOL_MT_FAST,    true);
        GGWave::Protocols::tx().toggle(GGWAVE_PROTOCOL_MT_FASTEST, true);

        // Protocols to use for RX
        // Remove the ones that you don't need to reduce memory usage
        GGWave::Protocols::rx().disableAll();
        //GGWave::Protocols::rx().toggle(GGWAVE_PROTOCOL_DT_NORMAL,  true);
        //GGWave::Protocols::rx().toggle(GGWAVE_PROTOCOL_DT_FAST,    true);
        GGWave::Protocols::rx().toggle(GGWAVE_PROTOCOL_DT_FASTEST, true);
        //GGWave::Protocols::rx().toggle(GGWAVE_PROTOCOL_MT_NORMAL,  true);
        //GGWave::Protocols::rx().toggle(GGWAVE_PROTOCOL_MT_FAST,    true);
        GGWave::Protocols::rx().toggle(GGWAVE_PROTOCOL_MT_FASTEST, true);

        // Initialize the ggwave instance and print the memory usage
        ggwave.prepare(p);

        Serial.print(F("Instance initialized successfully! Memory used: "));
        Serial.print(ggwave.heapSize());
        Serial.println(F(" bytes"));
    }

    // Start capturing audio
    {
        Serial.println(F("Trying to start I2S ADC"));

        // Install and start i2s driver
        i2s_driver_install(i2s_port, &adc_i2s_config, 0, NULL);

        // Init ADC pad
        i2s_set_adc_mode(adc_unit, adc_channel);

        // Enable the adc
        i2s_adc_enable(i2s_port);

        Serial.println(F("I2S ADC started"));
    }
}

void loop() {
    int nr = 0;
    int niter = 0;

    GGWave::TxRxData result;

    // Read from i2s - the samples are 12-bit so we need to do some massaging to make them 16-bit
    {
        size_t bytes_read = 0;
        i2s_read(i2s_port, sampleBuffer, sizeof(int16_t)*samplesPerFrame, &bytes_read, portMAX_DELAY);

        int samples_read = bytes_read / sizeof(int16_t);
        if (samples_read != samplesPerFrame) {
            Serial.println("Failed to read samples");
            return;
        }

        for (int i = 0; i < samples_read; i += 2) {
            auto & s0 = sampleBuffer[i];
            auto & s1 = sampleBuffer[i + 1];

            s0 = s0 & 0x0fff;
            s1 = s1 & 0x0fff;

            s0 = s0 ^ s1;
            s1 = s0 ^ s1;
            s0 = s0 ^ s1;
        }

        // Use this with the serial plotter to observe real-time audio signal
        //for (int i = 0; i < samples_read; i++) {
        //    Serial.println(sampleBuffer[i]);
        //}
    }

    // Try to decode any "ggwave" data:
    auto tStart = millis();

    if (ggwave.decode(sampleBuffer, samplesPerFrame*kSampleSize_bytes) == false) {
        Serial.println("Failed to decode");
    }

    auto tEnd = millis();

    if (++niter % 10 == 0) {
        // print the time it took the last decode() call to complete
        // should be smaller than samplesPerFrame/sampleRate seconds
        // for example: samplesPerFrame = 128, sampleRate = 6000 => not more than 20 ms
        Serial.println(tEnd - tStart);
        if (tEnd - tStart > 1000*(float(samplesPerFrame)/sampleRate)) {
            Serial.println(F("Warning: decode() took too long to execute!"));
        }
    }

    // Check if we have successfully decoded any data:
    nr = ggwave.rxTakeData(result);
    if (nr > 0) {
        Serial.println(tEnd - tStart);
        Serial.print(F("Received data with length "));
        Serial.print(nr); // should be equal to p.payloadLength
        Serial.println(F(" bytes:"));

        Serial.println((char *) result.data());

#ifdef DISPLAY_OUTPUT
        {
            display.clearDisplay();

            display.setTextSize(2);
            display.setTextColor(SSD1306_WHITE);
            display.setCursor(0, 0);
            display.println((char *) result.data());

            display.display();
        }
#endif
    }
}
