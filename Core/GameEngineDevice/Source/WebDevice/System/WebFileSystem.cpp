/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
*/

#include "WebDevice/System/WebFileSystem.h"
#include <emscripten.h>
#include <iostream>

WebFileSystem::WebFileSystem() : LocalFileSystem() {
    EM_ASM({
        window._assetsReady = false;
    });
}

WebFileSystem::~WebFileSystem() {
}

void WebFileSystem::init() {
    LocalFileSystem::init();
    
    // Mount IDBFS for persistent data (Options.ini, SaveGames)
    EM_ASM({
        FS.mkdir('/persistent');
        FS.mount(IDBFS, {}, '/persistent');
        FS.syncfs(true, function (err) {
            if (err) console.error('IDBFS sync failed:', err);
            else console.log('IDBFS synced from IndexedDB');
        });
    });

    preloadAssets();
}

void WebFileSystem::preloadAssets() {
    // Discovery: Fetch /assets/ directory index and parse for .big files
    EM_ASM({
        window._assetsReady = false;
        async function discoverAndFetch() {
            try {
                const response = await fetch('/assets/');
                const html = await response.text();
                
                // Extremely simple regex to find .big files in an HTML directory listing
                const regex = /href="([^"]+\.big)"/g;
                let match;
                const filesToFetch = [];
                
                while ((match = regex.exec(html)) !== null) {
                    filesToFetch.push(match[1]);
                }
                
                console.log("Discovered assets:", filesToFetch);
                
                const promises = filesToFetch.map(async (file) => {
                    console.log("Fetching asset:", file);
                    const assetResponse = await fetch('/assets/' + file);
                    const buffer = await assetResponse.arrayBuffer();
                    const data = new Uint8Array(buffer);
                    FS.writeFile("/" + file, data);
                });
                
                await Promise.all(promises);
                console.log("All discovered assets preloaded.");
                window._assetsReady = true;
            } catch (e) {
                console.error("Asset discovery/fetch failed:", e);
                // Even on failure, mark as ready to unblock engine (it will error later)
                window._assetsReady = true; 
            }
        }
        discoverAndFetch();
    });
}

bool WebFileSystem::isAssetsReady() {
    return EM_ASM_INT({
        return window._assetsReady ? 1 : 0;
    });
}
