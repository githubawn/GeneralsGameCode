/*
**	Command & Conquer Generals(tm)
**	Copyright 2025 Electronic Arts Inc.
*/

#ifndef _WEB_GAME_ENGINE_H_
#define _WEB_GAME_ENGINE_H_

#include "Common/GameEngine.h"

class WebGameEngine : public GameEngine {
public:
    WebGameEngine();
    virtual ~WebGameEngine();

    virtual void init();
    virtual void update();
    virtual void run();

protected:
    virtual LocalFileSystem *createLocalFileSystem();
    virtual ArchiveFileSystem *createArchiveFileSystem();
    virtual GameLogic *createGameLogic();
    virtual GameClient *createGameClient();
    virtual ModuleFactory *createModuleFactory();
    virtual ThingFactory *createThingFactory();
    virtual FunctionLexicon *createFunctionLexicon();
    virtual Radar *createRadar();
    virtual WebBrowser *createWebBrowser();
    virtual ParticleSystemManager* createParticleSystemManager();
    virtual AudioManager *createAudioManager();
};

#endif
