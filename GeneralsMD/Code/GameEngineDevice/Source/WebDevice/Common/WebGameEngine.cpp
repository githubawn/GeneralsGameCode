/*
**	Command & Conquer Generals(tm)
**	Copyright 2025 Electronic Arts Inc.
*/

#include "WebDevice/Common/WebGameEngine.h"
#include "Common/FileSystem.h"
#include "WebDevice/System/WebFileSystem.h"
#include "StdDevice/Common/StdBIGFileSystem.h"
#include <iostream>
#include "Common/GameEngine.h"
#include <emscripten.h>

WebGameEngine* g_WebGameEngine = nullptr;

void web_main_loop() {
    if (g_WebGameEngine) {
        if (TheLocalFileSystem && !((WebFileSystem*)TheLocalFileSystem)->isAssetsReady()) {
            static bool logged = false;
            if (!logged) {
                std::cout << "Waiting for assets to preload..." << std::endl;
                logged = true;
            }
            return;
        }
        g_WebGameEngine->update();
    }
}

WebGameEngine::WebGameEngine() : GameEngine() {
    g_WebGameEngine = this;
}

WebGameEngine::~WebGameEngine() {
    g_WebGameEngine = nullptr;
}

#include "WebDevice/GameClient/WebInput.h"
#include "GameClient/Keyboard.h"
#include "GameClient/Mouse.h"

void WebGameEngine::init() {
    std::cout << "WebGameEngine::init()" << std::endl;
    
    // Initialize Filesystem
    TheLocalFileSystem = new WebFileSystem();
    TheArchiveFileSystem = new StdBIGFileSystem();
    TheFileSystem = new FileSystem();
    
    TheFileSystem->init();
    
    // Initialize Web-specific input devices
    if (!TheKeyboard) TheKeyboard = new WebKeyboard();
    if (!TheMouse) TheMouse = new WebMouse();
    
    TheKeyboard->init();
    TheMouse->init();
}

void WebGameEngine::update() {
    GameEngine::update();
    // Browser specifically doesn't need Sleep(5) like Win32 does
}

void WebGameEngine::run() {
    emscripten_set_main_loop(web_main_loop, 0, 1);
}

LocalFileSystem *WebGameEngine::createLocalFileSystem() {
    return new WebFileSystem();
}

ArchiveFileSystem *WebGameEngine::createArchiveFileSystem() {
    return new StdBIGFileSystem();
}

GameLogic *WebGameEngine::createGameLogic() {
    return nullptr; // TODO
}

GameClient *WebGameEngine::createGameClient() {
    return nullptr; // TODO
}

ModuleFactory *WebGameEngine::createModuleFactory() {
    return nullptr; // TODO
}

ThingFactory *WebGameEngine::createThingFactory() {
    return nullptr; // TODO
}

FunctionLexicon *WebGameEngine::createFunctionLexicon() {
    return nullptr; // TODO
}

Radar *WebGameEngine::createRadar() {
    return nullptr; // TODO
}

WebBrowser *WebGameEngine::createWebBrowser() {
    return nullptr; // TODO
}

ParticleSystemManager* WebGameEngine::createParticleSystemManager() {
    return nullptr; // TODO
}

AudioManager *WebGameEngine::createAudioManager() {
    return nullptr; // TODO
}
