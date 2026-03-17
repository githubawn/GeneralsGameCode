/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
*/

#include <SDL2/SDL.h>
#include "WebDevice/Common/WebGameEngine.h"
#include <emscripten.h>
#include <iostream>

#include "Common/GameEngine.h"
#include "Common/Debug.h"
#include "WebDevice/GameClient/WebInput.h"

SDL_Window* g_Window = nullptr;
SDL_GLContext g_GLContext = nullptr;

int main(int argc, char* argv[]) {
    std::cout << "Starting Generals Zero Hour (Web Port)..." << std::endl;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER) != 0) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    // Set WebGL attributes via SDL
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);

    g_Window = SDL_CreateWindow("Generals Zero Hour",
                                SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                800, 600,
                                SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    if (!g_Window) {
        std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    g_GLContext = SDL_GL_CreateContext(g_Window);
    
    // Initialize the engine
    WebGameEngine* engine = new WebGameEngine();
    engine->init();
    
    // Setup event bridge
    auto event_bridge = []() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
                // Simplified mapper: for now just pass keys directly if they match common ASCII
                // In a real build, we'll need a full SDL_Scancode to DIK map.
                WebKeyboard::enqueueKeyEvent(event.key.keysym.scancode, event.type == SDL_KEYDOWN);
            } else if (event.type == SDL_MOUSEMOTION) {
                MouseIO m;
                m.pos.x = event.motion.x;
                m.pos.y = event.motion.y;
                m.deltaPos.x = event.motion.xrel;
                m.deltaPos.y = event.motion.yrel;
                m.time = SDL_GetTicks();
                WebMouse::enqueueMouseEvent(m);
            } else if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
                MouseIO m;
                SDL_GetMouseState(&m.pos.x, &m.pos.y);
                m.leftState = (event.button.button == SDL_BUTTON_LEFT) ? (event.type == SDL_MOUSEBUTTONDOWN ? MBS_Down : MBS_Up) : MBS_None;
                m.rightState = (event.button.button == SDL_BUTTON_RIGHT) ? (event.type == SDL_MOUSEBUTTONDOWN ? MBS_Down : MBS_Up) : MBS_None;
                m.middleState = (event.button.button == SDL_BUTTON_MIDDLE) ? (event.type == SDL_MOUSEBUTTONDOWN ? MBS_Down : MBS_Up) : MBS_None;
                m.time = SDL_GetTicks();
                WebMouse::enqueueMouseEvent(m);
            }
        }
    };

    // Replace engine->run() with emscripten loop call that includes event bridge
    emscripten_set_main_loop_arg([](void* arg) {
        auto* engine = (WebGameEngine*)arg;
        
        // Pump events
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
                // Basic mapping: SDL Scancode to DIK (mostly same for 1:1 common keys)
                WebKeyboard::enqueueKeyEvent(event.key.keysym.scancode, event.type == SDL_KEYDOWN);
            } else if (event.type == SDL_MOUSEMOTION) {
                MouseIO m;
                m.pos.x = event.motion.x;
                m.pos.y = event.motion.y;
                m.deltaPos.x = event.motion.xrel;
                m.deltaPos.y = event.motion.yrel;
                m.time = SDL_GetTicks();
                WebMouse::enqueueMouseEvent(m);
            } else if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
                MouseIO m;
                SDL_GetMouseState(&m.pos.x, &m.pos.y);
                m.leftState = (event.button.button == SDL_BUTTON_LEFT) ? (event.type == SDL_MOUSEBUTTONDOWN ? MBS_Down : MBS_Up) : MBS_None;
                m.rightState = (event.button.button == SDL_BUTTON_RIGHT) ? (event.type == SDL_MOUSEBUTTONDOWN ? MBS_Down : MBS_Up) : MBS_None;
                m.middleState = (event.button.button == SDL_BUTTON_MIDDLE) ? (event.type == SDL_MOUSEBUTTONDOWN ? MBS_Down : MBS_Up) : MBS_None;
                m.time = SDL_GetTicks();
                WebMouse::enqueueMouseEvent(m);
            }
        }

        engine->update();
    }, engine, 0, 1);

    return 0;
}
