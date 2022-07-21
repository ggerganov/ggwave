#include "ggwave/ggwave.h"

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

#include "ggwave-common.h"

#include <cstdio>
#include <cstring>
#include <iostream>

int main(int argc, char** argv) {
    fprintf(stderr, "Usage: %s audio.wav [-lN] [-d]\n", argv[0]);
    fprintf(stderr, "    -lN - fixed payload length of size N, N in [1, %d]\n", GGWave::kMaxLengthFixed);
    fprintf(stderr, "    -d  - use Direct Sequence Spread (DSS)\n");
    fprintf(stderr, "\n");

    if (argc < 2) {
        return -1;
    }

    const auto argm = parseCmdArguments(argc, argv);

    if (argm.count("h") > 0) {
        return 0;
    }

    const int   payloadLength = argm.count("l") == 0 ? -1 : std::stoi(argm.at("l"));
    const bool  useDSS        = argm.count("d") >  0;

    drwav wav;
    if (!drwav_init_file(&wav, argv[1], nullptr)) {
        fprintf(stderr, "Failed to open WAV file\n");
        return -4;
    }

    if (wav.channels != 1) {
        fprintf(stderr, "Only mono WAV files are supported\n");
        return -5;
    }

    // Read WAV samples into a buffer
    // Add 3 seconds of silence at the end
    const size_t samplesSilence = 3*wav.sampleRate;
    const size_t samplesCount = wav.totalPCMFrameCount;
    const size_t samplesSize = wav.bitsPerSample/8;
          size_t samplesTotal = samplesCount + samplesSilence;
    std::vector<uint8_t> samples(samplesTotal*samplesSize*wav.channels, 0);

    printf("[+] Number of channels: %d\n", wav.channels);
    printf("[+] Sample rate: %d\n", wav.sampleRate);
    printf("[+] Bits per sample: %d\n", wav.bitsPerSample);
    printf("[+] Total samples: %zu\n", samplesCount);

    printf("[+] Decoding .. \n\n");

    GGWave::Parameters parameters = GGWave::getDefaultParameters();

    parameters.payloadLength = payloadLength;
    parameters.sampleRateInp = wav.sampleRate;
    parameters.operatingMode = GGWAVE_OPERATING_MODE_RX;
    if (useDSS) parameters.operatingMode |= GGWAVE_OPERATING_MODE_USE_DSS;

    switch (wav.bitsPerSample) {
        case 16:
            drwav_read_pcm_frames_s16(&wav, samplesCount, reinterpret_cast<int16_t*>(samples.data()));

            if (wav.channels > 1) {
                for (size_t i = 0; i < samplesCount; ++i) {
                    int16_t sample = 0;
                    for (size_t j = 0; j < wav.channels; ++j) {
                        sample += reinterpret_cast<int16_t*>(samples.data())[i*wav.channels + j];
                    }
                    reinterpret_cast<int16_t*>(samples.data())[i] = sample / wav.channels;
                }
            }

            parameters.sampleFormatInp = GGWAVE_SAMPLE_FORMAT_I16;

            break;
        case 32:
            drwav_read_pcm_frames_f32(&wav, samplesCount, reinterpret_cast<float*>(samples.data()));

            if (wav.channels > 1) {
                for (size_t i = 0; i < samplesCount; ++i) {
                    float sample = 0.0f;
                    for (size_t j = 0; j < wav.channels; ++j) {
                        sample += reinterpret_cast<float*>(samples.data())[i*wav.channels + j];
                    }
                    reinterpret_cast<float*>(samples.data())[i] = sample / wav.channels;
                }
            }

            parameters.sampleFormatInp = GGWAVE_SAMPLE_FORMAT_F32;

            break;
        default:
            fprintf(stderr, "Unsupported WAV format\n");
            return -6;
    }

    GGWave ggWave(parameters);
    ggWave.setLogFile(nullptr);

    GGWave::TxRxData data;
    auto ptr = samples.data();
    while ((int) samplesTotal >= parameters.samplesPerFrame) {
        if (ggWave.decode(ptr, parameters.samplesPerFrame*samplesSize*wav.channels) == false) {
            fprintf(stderr, "Failed to decode the waveform in the WAV file\n");
            return -7;
        }

        ptr += parameters.samplesPerFrame*samplesSize*wav.channels;
        samplesTotal -= parameters.samplesPerFrame;

        const int n = ggWave.rxTakeData(data);
        if (n > 0) {
            printf("[+] Decoded message with length %d: '", n);
            for (auto i = 0; i < n; ++i) {
                printf("%c", data[i]);
            }
            printf("'\n");
        }

    }

    printf("\n[+] Done\n");

    return 0;
}
