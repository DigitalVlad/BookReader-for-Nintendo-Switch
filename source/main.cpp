#include "app/App.hpp"
#include <cstdio>
#include <exception>
#ifdef __SWITCH__
#include <switch.h>
#endif
int main(int, char**) {
    try {
        br::App app;
        return app.run();
    } catch (const std::exception& e) {
        fprintf(stderr, "BookReader: %s\n", e.what());
#ifdef __SWITCH__
        consoleInit(nullptr);
        printf("BookReader could not start:\n%s\n\nPress + to exit.\n", e.what());
        PadState p;
        padConfigureInput(1, HidNpadStyleSet_NpadStandard);
        padInitializeDefault(&p);
        while (appletMainLoop()) {
            padUpdate(&p);
            if (padGetButtonsDown(&p) & HidNpadButton_Plus)
                break;
            consoleUpdate(nullptr);
        }
        consoleExit(nullptr);
#endif
        return 1;
    }
}
