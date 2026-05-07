#include "engine/Engine.hpp"
#include "game/SmokeTest.hpp"

#include <string>

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--smoke") {
        return ld::RunSmokeTests();
    }
    if (argc > 1 && std::string(argv[1]) == "--capture") {
        const char* output = argc > 2 ? argv[2] : "linear_drive_capture.png";
        return ld::RunCapture(output);
    }

    ld::Engine engine;
    return engine.Run();
}
