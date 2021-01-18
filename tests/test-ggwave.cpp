#include "ggwave/ggwave.h"

#include <string>

#define CHECK(cond) \
    if (!(cond)) { \
        fprintf(stderr, "[%s:%d] Check failed: %s\n", __FILE__, __LINE__, #cond); \
        exit(1); \
    }

#define CHECK_T(cond) CHECK(cond)
#define CHECK_F(cond) CHECK(!(cond))

int main() {
    GGWave instance(48000, 48000, 1024, 4, 2);

    std::string payload = "hello";

    CHECK(instance.init(payload.size(), payload.c_str()));

    // data
    CHECK_F(instance.init(-1, "asd"));
    CHECK_T(instance.init(0, nullptr));
    CHECK_T(instance.init(0, "asd"));
    CHECK_T(instance.init(1, "asd"));
    CHECK_T(instance.init(2, "asd"));
    CHECK_T(instance.init(3, "asd"));

    // volume
    CHECK_F(instance.init(payload.size(), payload.c_str(), -1));
    CHECK_T(instance.init(payload.size(), payload.c_str(), 0));
    CHECK_T(instance.init(payload.size(), payload.c_str(), 50));
    CHECK_T(instance.init(payload.size(), payload.c_str(), 100));
    CHECK_F(instance.init(payload.size(), payload.c_str(), 101));

    // todo ..

    return 0;
}
