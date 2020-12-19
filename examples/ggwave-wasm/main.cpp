#include "build_timestamp.h"

#include "ggwave-common-sdl2.h"

#include "emscripten/emscripten.h"

void update() {
    GGWave_mainLoop();
}

int main(int , char** argv) {
    printf("Build time: %s\n", BUILD_TIMESTAMP);
    printf("Press the Init button to start\n");

    if (argv[1]) {
        GGWave_setDefaultCaptureDeviceName(argv[1]);
    }

    emscripten_set_main_loop(update, 60, 1);

    return 0;
}
