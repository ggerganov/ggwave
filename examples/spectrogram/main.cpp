#include "fft.h"

#include "ggwave/ggwave.h"
#include "ggwave-common.h"

#include <imgui-extra/imgui_impl.h>

#include <SDL.h>
#include <SDL_opengl.h>

#include <fstream>
#include <vector>
#include <functional>

namespace {

std::string g_defaultCaptureDeviceName = "";

SDL_AudioDeviceID g_devIdInp = 0;
SDL_AudioDeviceID g_devIdOut = 0;

SDL_AudioSpec g_obtainedSpecInp;
SDL_AudioSpec g_obtainedSpecOut;

struct FreqData {
    float freq;

    std::vector<float> mag;
};

bool g_isCapturing = true;
constexpr int g_nSamplesPerFrame = 1024;

int g_binMin = 40;
int g_binMax = 40 + 96;

float g_scale = 5.0;

bool g_showControls = true;

int g_freqDataHead = 0;
int g_freqDataSize = 0;
std::vector<FreqData> g_freqData;

}

void GGWave_setDefaultCaptureDeviceName(std::string name) {
    g_defaultCaptureDeviceName = std::move(name);
}

bool GGWave_init(
        const int playbackId,
        const int captureId) {

    if (g_devIdInp && g_devIdOut) {
        return false;
    }

    if (g_devIdInp == 0 && g_devIdOut == 0) {
        SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

        if (SDL_Init(SDL_INIT_AUDIO) < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s\n", SDL_GetError());
            return (1);
        }

        SDL_SetHintWithPriority(SDL_HINT_AUDIO_RESAMPLING_MODE, "medium", SDL_HINT_OVERRIDE);

        {
            int nDevices = SDL_GetNumAudioDevices(SDL_FALSE);
            printf("Found %d playback devices:\n", nDevices);
            for (int i = 0; i < nDevices; i++) {
                printf("    - Playback device #%d: '%s'\n", i, SDL_GetAudioDeviceName(i, SDL_FALSE));
            }
        }
        {
            int nDevices = SDL_GetNumAudioDevices(SDL_TRUE);
            printf("Found %d capture devices:\n", nDevices);
            for (int i = 0; i < nDevices; i++) {
                printf("    - Capture device #%d: '%s'\n", i, SDL_GetAudioDeviceName(i, SDL_TRUE));
            }
        }
    }

    bool reinit = false;

    if (g_devIdOut == 0) {
        printf("Initializing playback ...\n");

        SDL_AudioSpec playbackSpec;
        SDL_zero(playbackSpec);

        playbackSpec.freq = GGWave::kBaseSampleRate;
        playbackSpec.format = AUDIO_S16SYS;
        playbackSpec.channels = 1;
        playbackSpec.samples = 16*1024;
        playbackSpec.callback = NULL;

        SDL_zero(g_obtainedSpecOut);

        if (playbackId >= 0) {
            printf("Attempt to open playback device %d : '%s' ...\n", playbackId, SDL_GetAudioDeviceName(playbackId, SDL_FALSE));
            g_devIdOut = SDL_OpenAudioDevice(SDL_GetAudioDeviceName(playbackId, SDL_FALSE), SDL_FALSE, &playbackSpec, &g_obtainedSpecOut, 0);
        } else {
            printf("Attempt to open default playback device ...\n");
            g_devIdOut = SDL_OpenAudioDevice(NULL, SDL_FALSE, &playbackSpec, &g_obtainedSpecOut, 0);
        }

        if (!g_devIdOut) {
            printf("Couldn't open an audio device for playback: %s!\n", SDL_GetError());
            g_devIdOut = 0;
        } else {
            printf("Obtained spec for output device (SDL Id = %d):\n", g_devIdOut);
            printf("    - Sample rate:       %d (required: %d)\n", g_obtainedSpecOut.freq, playbackSpec.freq);
            printf("    - Format:            %d (required: %d)\n", g_obtainedSpecOut.format, playbackSpec.format);
            printf("    - Channels:          %d (required: %d)\n", g_obtainedSpecOut.channels, playbackSpec.channels);
            printf("    - Samples per frame: %d (required: %d)\n", g_obtainedSpecOut.samples, playbackSpec.samples);

            if (g_obtainedSpecOut.format != playbackSpec.format ||
                g_obtainedSpecOut.channels != playbackSpec.channels ||
                g_obtainedSpecOut.samples != playbackSpec.samples) {
                g_devIdOut = 0;
                SDL_CloseAudio();
                fprintf(stderr, "Failed to initialize playback SDL_OpenAudio!");

                return false;
            }

            reinit = true;
        }
    }

    if (g_devIdInp == 0) {
        SDL_AudioSpec captureSpec;
        captureSpec = g_obtainedSpecOut;
        captureSpec.freq = GGWave::kBaseSampleRate;
        captureSpec.format = AUDIO_F32SYS;
        captureSpec.samples = g_nSamplesPerFrame;

        SDL_zero(g_obtainedSpecInp);

        if (captureId >= 0) {
            printf("Attempt to open capture device %d : '%s' ...\n", captureId, SDL_GetAudioDeviceName(captureId, SDL_FALSE));
            g_devIdInp = SDL_OpenAudioDevice(SDL_GetAudioDeviceName(captureId, SDL_TRUE), SDL_TRUE, &captureSpec, &g_obtainedSpecInp, 0);
        } else {
            printf("Attempt to open default capture device ...\n");
            g_devIdInp = SDL_OpenAudioDevice(g_defaultCaptureDeviceName.empty() ? nullptr : g_defaultCaptureDeviceName.c_str(),
                                            SDL_TRUE, &captureSpec, &g_obtainedSpecInp, 0);
        }
        if (!g_devIdInp) {
            printf("Couldn't open an audio device for capture: %s!\n", SDL_GetError());
            g_devIdInp = 0;
        } else {
            printf("Obtained spec for input device (SDL Id = %d):\n", g_devIdInp);
            printf("    - Sample rate:       %d\n", g_obtainedSpecInp.freq);
            printf("    - Format:            %d (required: %d)\n", g_obtainedSpecInp.format, captureSpec.format);
            printf("    - Channels:          %d (required: %d)\n", g_obtainedSpecInp.channels, captureSpec.channels);
            printf("    - Samples per frame: %d\n", g_obtainedSpecInp.samples);

            reinit = true;
        }
    }

    GGWave::SampleFormat sampleFormatInp = GGWAVE_SAMPLE_FORMAT_UNDEFINED;
    GGWave::SampleFormat sampleFormatOut = GGWAVE_SAMPLE_FORMAT_UNDEFINED;

    switch (g_obtainedSpecInp.format) {
        case AUDIO_U8:      sampleFormatInp = GGWAVE_SAMPLE_FORMAT_U8;  break;
        case AUDIO_S8:      sampleFormatInp = GGWAVE_SAMPLE_FORMAT_I8;  break;
        case AUDIO_U16SYS:  sampleFormatInp = GGWAVE_SAMPLE_FORMAT_U16; break;
        case AUDIO_S16SYS:  sampleFormatInp = GGWAVE_SAMPLE_FORMAT_I16; break;
        case AUDIO_S32SYS:  sampleFormatInp = GGWAVE_SAMPLE_FORMAT_F32; break;
        case AUDIO_F32SYS:  sampleFormatInp = GGWAVE_SAMPLE_FORMAT_F32; break;
    }

    switch (g_obtainedSpecOut.format) {
        case AUDIO_U8:      sampleFormatOut = GGWAVE_SAMPLE_FORMAT_U8;  break;
        case AUDIO_S8:      sampleFormatOut = GGWAVE_SAMPLE_FORMAT_I8;  break;
        case AUDIO_U16SYS:  sampleFormatOut = GGWAVE_SAMPLE_FORMAT_U16; break;
        case AUDIO_S16SYS:  sampleFormatOut = GGWAVE_SAMPLE_FORMAT_I16; break;
        case AUDIO_S32SYS:  sampleFormatOut = GGWAVE_SAMPLE_FORMAT_F32; break;
        case AUDIO_F32SYS:  sampleFormatOut = GGWAVE_SAMPLE_FORMAT_F32; break;
            break;
    }

    return true;
}

bool GGWave_mainLoop() {
    if (g_devIdInp == 0 && g_devIdOut == 0) {
        return false;
    }

    static GGWave::CBWaveformOut cbQueueAudio = [&](const void * data, uint32_t nBytes) {
        SDL_QueueAudio(g_devIdOut, data, nBytes);
    };

    static GGWave::CBWaveformInp cbWaveformInp = [&](void * data, uint32_t nMaxBytes) {
        return SDL_DequeueAudio(g_devIdInp, data, nMaxBytes);
    };

    SDL_PauseAudioDevice(g_devIdInp, SDL_FALSE);
    if (!g_isCapturing) {
        SDL_ClearQueuedAudio(g_devIdInp);
    }

    int n = 0;
    static float data[g_nSamplesPerFrame];
    static float out[2*g_nSamplesPerFrame];
    do {
        n = cbWaveformInp(data, sizeof(float)*g_nSamplesPerFrame);
        if (n <= 0) break;

        FFT(data, out, g_nSamplesPerFrame, 1.0);

        for (int i = 0; i < g_nSamplesPerFrame; ++i) {
            out[i] = std::sqrt(out[2*i + 0]*out[2*i + 0] + out[2*i + 1]*out[2*i + 1]);
        }
        for (int i = 1; i < g_nSamplesPerFrame/2; ++i) {
            out[i] += out[g_nSamplesPerFrame - i];
        }

        for (int i = 0; i < (int) g_freqData.size(); ++i) {
            g_freqData[i].mag[g_freqDataHead] = out[i];
        }
        if (++g_freqDataHead == g_freqDataSize) {
            g_freqDataHead = 0;
        }
    } while (n > 0);

    return true;
}

bool GGWave_deinit() {
    if (g_devIdInp == 0 && g_devIdOut == 0) {
        return false;
    }

    SDL_PauseAudioDevice(g_devIdInp, 1);
    SDL_CloseAudioDevice(g_devIdInp);
    SDL_PauseAudioDevice(g_devIdOut, 1);
    SDL_CloseAudioDevice(g_devIdOut);
    SDL_CloseAudio();
    SDL_Quit();

    return true;
}

bool ImGui_BeginFrame(SDL_Window * window) {
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        ImGui_ProcessEvent(&event);
        if (event.type == SDL_QUIT) return false;
        if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window)) return false;
    }

    ImGui_NewFrame(window);

    return true;
}

bool ImGui_EndFrame(SDL_Window * window) {
    // Rendering
    int display_w, display_h;
    SDL_GetWindowSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.0f, 0.0f, 0.0f, 0.4f);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui::Render();
    ImGui_RenderDrawData(ImGui::GetDrawData());

    SDL_GL_SwapWindow(window);

    return true;
}

bool ImGui_SetStyle() {
    ImGuiStyle & style = ImGui::GetStyle();

    style.AntiAliasedFill = true;
    style.AntiAliasedLines = true;
    style.WindowRounding = 0.0f;

    style.WindowPadding = ImVec2(8, 8);
    style.WindowRounding = 0.0f;
    style.FramePadding = ImVec2(4, 3);
    style.FrameRounding = 0.0f;
    style.ItemSpacing = ImVec2(8, 4);
    style.ItemInnerSpacing = ImVec2(4, 4);
    style.IndentSpacing = 21.0f;
    style.ScrollbarSize = 16.0f;
    style.ScrollbarRounding = 9.0f;
    style.GrabMinSize = 10.0f;
    style.GrabRounding = 3.0f;

    style.Colors[ImGuiCol_Text]                  = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_TextDisabled]          = ImVec4(0.24f, 0.41f, 0.41f, 1.00f);
    style.Colors[ImGuiCol_WindowBg]              = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
    //style.Colors[ImGuiCol_ChildWindowBg]         = ImVec4(0.07f, 0.07f, 0.09f, 1.00f);
    style.Colors[ImGuiCol_PopupBg]               = ImVec4(0.07f, 0.07f, 0.09f, 1.00f);
    style.Colors[ImGuiCol_Border]                = ImVec4(0.31f, 0.31f, 0.31f, 0.71f);
    style.Colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style.Colors[ImGuiCol_FrameBg]               = ImVec4(0.00f, 0.39f, 0.39f, 0.39f);
    style.Colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.26f, 1.00f, 1.00f, 0.39f);
    style.Colors[ImGuiCol_FrameBgActive]         = ImVec4(0.00f, 0.78f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_TitleBg]               = ImVec4(0.00f, 0.50f, 0.50f, 0.70f);
    style.Colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.00f, 0.50f, 0.50f, 1.00f);
    style.Colors[ImGuiCol_TitleBgActive]         = ImVec4(0.00f, 0.70f, 0.70f, 1.00f);
    style.Colors[ImGuiCol_MenuBarBg]             = ImVec4(0.00f, 0.70f, 0.70f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.10f, 0.27f, 0.27f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.80f, 0.80f, 0.83f, 0.31f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.26f, 1.00f, 1.00f, 0.39f);
    style.Colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.00f, 0.78f, 0.00f, 1.00f);
    //style.Colors[ImGuiCol_ComboBg]               = ImVec4(0.00f, 0.39f, 0.39f, 1.00f);
    style.Colors[ImGuiCol_CheckMark]             = ImVec4(0.80f, 0.80f, 0.83f, 0.39f);
    style.Colors[ImGuiCol_SliderGrab]            = ImVec4(0.80f, 0.80f, 0.83f, 0.39f);
    style.Colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.00f, 0.78f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_Button]                = ImVec4(0.13f, 0.55f, 0.55f, 1.00f);
    style.Colors[ImGuiCol_ButtonHovered]         = ImVec4(0.61f, 1.00f, 0.00f, 0.51f);
    style.Colors[ImGuiCol_ButtonActive]          = ImVec4(0.00f, 0.78f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_Header]                = ImVec4(0.79f, 0.51f, 0.00f, 0.51f);
    style.Colors[ImGuiCol_HeaderHovered]         = ImVec4(0.79f, 0.51f, 0.00f, 0.67f);
    style.Colors[ImGuiCol_HeaderActive]          = ImVec4(0.79f, 0.51f, 0.00f, 0.67f);
    //style.Colors[ImGuiCol_Column]                = ImVec4(0.79f, 0.51f, 0.00f, 0.67f);
    //style.Colors[ImGuiCol_ColumnHovered]         = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
    //style.Colors[ImGuiCol_ColumnActive]          = ImVec4(0.79f, 0.51f, 0.00f, 0.67f);
    style.Colors[ImGuiCol_ResizeGrip]            = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style.Colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.26f, 1.00f, 1.00f, 0.39f);
    style.Colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.00f, 0.78f, 0.00f, 1.00f);
    //style.Colors[ImGuiCol_CloseButton]           = ImVec4(0.40f, 0.39f, 0.38f, 0.16f);
    //style.Colors[ImGuiCol_CloseButtonHovered]    = ImVec4(0.26f, 1.00f, 1.00f, 0.39f);
    //style.Colors[ImGuiCol_CloseButtonActive]     = ImVec4(0.79f, 0.51f, 0.00f, 0.67f);
    style.Colors[ImGuiCol_PlotLines]             = ImVec4(1.00f, 0.65f, 0.38f, 0.67f);
    style.Colors[ImGuiCol_PlotLinesHovered]      = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogram]         = ImVec4(1.00f, 0.65f, 0.38f, 0.67f);
    style.Colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.25f, 1.00f, 0.00f, 0.43f);
    style.Colors[ImGuiCol_ModalWindowDarkening]  = ImVec4(1.00f, 0.98f, 0.95f, 0.78f);

    return true;
}

static std::function<bool()> g_doInit;
static std::function<void(int, int)> g_setWindowSize;
static std::function<bool()> g_mainUpdate;

void mainUpdate(void *) {
    g_mainUpdate();
}

int main(int argc, char** argv) {
    auto argm = parseCmdArguments(argc, argv);
    int captureId = argm["c"].empty() ? 0 : std::stoi(argm["c"]);
    int playbackId = argm["p"].empty() ? 0 : std::stoi(argm["p"]);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "Error: %s\n", SDL_GetError());
        return -1;
    }

    ImGui_PreInit();

    int windowX = 1920;
    int windowY = 1200;

    const char * windowTitle = "spectrogram";

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window * window = SDL_CreateWindow(windowTitle, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowX, windowY, window_flags);

    void * gl_context = SDL_GL_CreateContext(window);

    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync

    ImGui_Init(window, gl_context);
    ImGui::GetIO().IniFilename = nullptr;

    ImGui_SetStyle();

    ImGui_NewFrame(window);
    ImGui::Render();

    bool isInitialized = false;

    g_doInit = [&]() {
        if (GGWave_init(playbackId, captureId) == false) {
            fprintf(stderr, "Failed to initialize GGWave\n");
            return false;
        }

        g_freqDataSize = (3*GGWave::kBaseSampleRate)/g_nSamplesPerFrame;

        float df = float(GGWave::kBaseSampleRate)/g_nSamplesPerFrame;
        g_freqData.resize(g_nSamplesPerFrame/2);
        for (int i = 0; i < g_nSamplesPerFrame/2; ++i) {
            g_freqData[i].freq = df*i;
            g_freqData[i].mag.resize(g_freqDataSize);
        }

        isInitialized = true;

        return true;
    };

    g_setWindowSize = [&](int sizeX, int sizeY) {
        SDL_SetWindowSize(window, sizeX, sizeY);
    };

    g_mainUpdate = [&]() {
        if (isInitialized == false) {
            return true;
        }

        if (ImGui_BeginFrame(window) == false) {
            return false;
        }

        const auto& displaySize = ImGui::GetIO().DisplaySize;

        ImGui::SetNextWindowPos({ 0, 0, });
        ImGui::SetNextWindowSize(displaySize);
        ImGui::Begin("Main", nullptr,
                     ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoTitleBar |
                     ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoSavedSettings);

        auto drawList = ImGui::GetWindowDrawList();

        float sum = 0.0;
        for (int i = g_binMin; i < g_binMax; ++i) {
            for (int j = 0; j < g_freqDataSize; ++j) {
                sum += g_freqData[i].mag[j];
            }
        }

        int nf = g_binMax - g_binMin;
        sum /= (nf*g_freqDataSize);

        const float dx = displaySize.x/(g_freqDataSize + 1);
        const float dy = displaySize.y/(nf + 1);

        auto p0 = ImGui::GetCursorScreenPos();
        for (int i = 0; i < nf; ++i) {
            for (int j = 0; j < g_freqDataSize; ++j) {
                int k = g_freqDataHead + j;
                if (k >= g_freqDataSize) k -= g_freqDataSize;
                auto v = g_freqData[g_binMin + i].mag[k];
                ImVec4 c = { 0.0f, 1.0f, 0.0, 0.0f };
                c.w = v/(g_scale*sum);
                drawList->AddRectFilled({ p0.x + j*dx, p0.y + i*dy }, { p0.x + j*dx + dx - 1, p0.y + i*dy + dy - 1 }, ImGui::ColorConvertFloat4ToU32(c));
            }
        }

        //for (int i = 0; i < (int) g_freqData.size(); ++i) {
        //    ImGui::PushID(i);
        //    ImGui::PlotLines("##signal", g_freqData[i].mag.data(), g_freqData[i].mag.size(), g_freqData[i].head, std::to_string(g_freqData[i].freq).c_str(), 0.0f, FLT_MAX, { ImGui::GetContentRegionAvailWidth(), 20 });
        //    ImGui::PopID();
        //}
        ImGui::End();

        bool togglePause = false;

        if (g_showControls) {
            ImGui::SetNextWindowFocus();
            ImGui::SetNextWindowPos({ 20, 20 });
            ImGui::SetNextWindowSize({ 400, 300 });
            ImGui::Begin("Controls", &g_showControls);
            ImGui::Text("Press 'c' to hide/show this window");
            ImGui::DragInt("Min", &g_binMin, 1, 0, g_binMax - 1);
            ImGui::DragInt("Max", &g_binMax, 1, g_binMin + 1, g_nSamplesPerFrame/2);
            ImGui::DragFloat("Scale", &g_scale, 1.0f, 1.0f, 1000.0f);
            if (ImGui::Button("Pause")) {
                togglePause = true;
            }
            if (ImGui::IsKeyPressed(40)) {
                togglePause = true;
            }
            ImGui::End();
        }

        if (togglePause) {
            g_isCapturing = !g_isCapturing;
        }

        if (ImGui::IsKeyPressed(6)) {
            g_showControls = !g_showControls;
        }

        GGWave_mainLoop();

        ImGui_EndFrame(window);

        return true;
    };

    if (g_doInit() == false) {
        printf("Error: failed to initialize audio\n");
        return -2;
    }

    while (true) {
        if (g_mainUpdate() == false) break;
    }

    GGWave_deinit();

    // Cleanup
    ImGui_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
