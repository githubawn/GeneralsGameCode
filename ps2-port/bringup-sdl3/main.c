/* SDL3-for-PS2 smoke test -- see docs/ps2-port-plan.md, Phase 1/4. Proves
   upstream SDL3's PS2 video/joystick backend initializes on real EE/GS
   hardware (via PCSX2) before any GeneralsMD SDL3 backend code targets it. */

#include <SDL3/SDL.h>
#include <debug.h>

int main(int argc, char *argv[])
{
    init_scr();
    scr_printf("SDL3 PS2 bringup\n");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        scr_printf("SDL_Init failed: %s\n", SDL_GetError());
        for (;;) {}
    }

    scr_printf("SDL_Init OK.\n");

    for (;;) {
        SDL_Delay(1000);
    }

    SDL_Quit();
    return 0;
}
