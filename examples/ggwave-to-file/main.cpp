#include "ggwave/ggwave.h"

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

#include "ggwave-common.h"

#include <cstdio>
#include <cstring>
#include <iostream>

int main(int argc, char** argv) {
    fprintf(stderr, "Usage: %s [-vN] [-sN] [-pN]\n", argv[0]);
    fprintf(stderr, "    -vN - output volume, N in (0, 100], (default: 50)\n");
    fprintf(stderr, "    -sN - output sample rate, N in [1024, %d], (default: %d)\n", (int) GGWave::kBaseSampleRate, (int) GGWave::kBaseSampleRate);
    fprintf(stderr, "    -pN - select the transmission protocol (default: 1)\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "    Available protocols:\n");

    const auto & protocols = GGWave::getTxProtocols();
    for (int i = 0; i < (int) protocols.size(); ++i) {
        fprintf(stderr, "      %d - %s\n", i, protocols[i].name);
    }
    fprintf(stderr, "\n");

    if (argc < 1) {
        return -1;
    }

    auto argm = parseCmdArguments(argc, argv);

    if (argm.find("h") != argm.end()) {
        return 0;
    }

    int protocolId = argm["p"].empty() ? 1 : std::stoi(argm["p"]);
    int volume = argm["v"].empty() ? 50 : std::stoi(argm["v"]);
    int sampleRateOut = argm["s"].empty() ? GGWave::kBaseSampleRate : std::stoi(argm["s"]);

    fprintf(stderr, "Enter a text message:\n");

    std::string message;
    std::getline(std::cin, message);

    if (message.size() == 0) {
        fprintf(stderr, "Invalid message: size = 0\n");
        return -2;
    }

    if (message.size() > 140) {
        fprintf(stderr, "Invalid message: size > 140\n");
        return -3;
    }


    fprintf(stderr, "Generating waveform for message '%s' ...\n", message.c_str());

    GGWave ggWave(GGWave::kBaseSampleRate, sampleRateOut, 1024, 4, 2);
    ggWave.init(message.size(), message.data(), ggWave.getTxProtocols()[protocolId], volume);

    std::vector<char> bufferPCM;
    GGWave::CBQueueAudio cbQueueAudio = [&](const void * data, uint32_t nBytes) {
        bufferPCM.resize(nBytes);
        std::memcpy(bufferPCM.data(), data, nBytes);
    };

    if (ggWave.send(cbQueueAudio) == false) {
        fprintf(stderr, "Failed to generate waveform!\n");
        return -4;
    }

    fprintf(stderr, "Output size = %d bytes\n", (int) bufferPCM.size());

    drwav_data_format format;
    format.container = drwav_container_riff;
    format.format = DR_WAVE_FORMAT_PCM;
    format.channels = 1;
    format.sampleRate = sampleRateOut;
    format.bitsPerSample = 16;

    fprintf(stderr, "Writing WAV data ...\n");

    drwav wav;
    drwav_init_file_write(&wav, "/dev/stdout", &format, NULL);
    drwav_uint64 framesWritten = drwav_write_pcm_frames(&wav, bufferPCM.size()/2, bufferPCM.data());

    fprintf(stderr, "WAV frames written = %d\n", (int) framesWritten);

    return 0;
}
