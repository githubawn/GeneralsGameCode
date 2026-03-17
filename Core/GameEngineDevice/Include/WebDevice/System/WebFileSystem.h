/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
*/

#ifndef _WEB_FILE_SYSTEM_H_
#define _WEB_FILE_SYSTEM_H_

#include "Common/LocalFileSystem.h"

class WebFileSystem : public LocalFileSystem {
public:
    WebFileSystem();
    virtual ~WebFileSystem();

    virtual void init();
    virtual void reset();
    virtual void update();

    void preloadAssets();
    bool isAssetsReady();
    void syncFS(bool store);
};

#endif
