#!/usr/bin/env node
// TheSuperHackers @feature githubawn 28/06/2026 Automated two-player smoke test for the
// WebAssembly LAN multiplayer build. Drives two independent headless-Chrome instances through
// the full flow — main menu -> MULTIPLAYER -> NETWORK -> host creates a game / joiner selects
// + joins + accepts -> host starts -> both load into the match — and screenshots each step.
// This exercises the WebSocket UDP relay (relay.py) and, crucially, the game-start load
// barrier in GameLogic::tryStartNewGame (which on the web yields via emscripten_sleep/Asyncify
// so the peers' "load complete" datagrams can be delivered). If both t_*_ingame.png show a
// rendered base, the LAN start path works end to end.
//
// Prerequisites (start these first, from the repo root):
//   python serve.py        # HTTP asset server   on :8000  (serves build/<preset>/web + game data)
//   python relay.py        # WebSocket UDP relay  on :8090
//   ...and a built web target at build/<preset>/web/Release/index.html served at /.
//
// Usage:
//   node scripts/web/lan-two-player-test.js
//
// Env overrides:
//   GGC_CHROME   path to a Chrome/Chromium executable (default: Windows Chrome install)
//   GGC_URL      base game URL                          (default: http://localhost:8000)
//   GGC_OUT      screenshot output directory            (default: ./lan-test-out)
//
// Requires puppeteer-core to be resolvable (npm i -g puppeteer-core, or run from a dir that
// has it installed). The menu is a WebGL canvas with no DOM widgets, so navigation uses
// calibrated pixel coordinates for a fixed 1024x768 viewport — keep that viewport size.

const fs = require('fs');
const path = require('path');
const puppeteer = require('puppeteer-core');

const CHROME = process.env.GGC_CHROME || 'C:/Program Files/Google/Chrome/Application/chrome.exe';
const URL = process.env.GGC_URL || 'http://localhost:8000';
const OUT = process.env.GGC_OUT || path.join(process.cwd(), 'lan-test-out');

const sleep = ms => new Promise(r => setTimeout(r, ms));
fs.mkdirSync(OUT, { recursive: true });
const shot = (p, name) => p.screenshot({ path: path.join(OUT, name) });

// Chrome flags that keep BOTH instances running at full rAF rate. Background tabs/windows are
// normally throttled to ~1fps, which freezes the emscripten main loop and breaks input timing.
const FLAGS = [
  '--no-sandbox', '--disable-dev-shm-usage', '--use-gl=swiftshader', '--window-size=1024,768',
  '--disable-background-timer-throttling', '--disable-backgrounding-occluded-windows',
  '--disable-renderer-backgrounding', '--disable-features=CalculateNativeWinOcclusion',
];

// Calibrated click points for the 1024x768 LAN menus.
const C = {
  multiplayer: [840, 205],   // main menu: MULTIPLAYER, then NETWORK (same spot on the submenu)
  createGame:  [165, 678],   // LAN lobby: CREATE GAME button
  joinGame:    [390, 678],   // LAN lobby: JOIN GAME button
  gameRow:     [400, 180],   // LAN lobby: first row of the middle "Games" list
  bottomLeft:  [120, 690],   // GAME OPTIONS: PLAY GAME (host) / ACCEPT (joiner)
};

// Bring a fresh instance up to the LAN lobby. `ip` (1..8) selects the loopback identity via the
// ?ip=N URL param; it also suffixes the player name (emscripten<N>) so the two same-origin tabs
// don't collide on a duplicate name.
async function toLobby(browser, ip, tag) {
  const p = await browser.newPage();
  await p.setViewport({ width: 1024, height: 768 });
  p.on('pageerror', e => console.log(`[${tag} pageerr]`, String(e).slice(0, 200)));
  await p.goto(`${URL}/?ip=${ip}`, { waitUntil: 'domcontentloaded', timeout: 60000 });
  // Wait for the loading overlay to clear (assets downloaded + engine booted).
  for (let i = 0; i < 480; i++) {
    const hidden = await p.$eval('#overlay', e => e.classList.contains('hidden')).catch(() => false);
    if (hidden) break;
    await sleep(250);
  }
  await sleep(11000);                       // let the shell map + menu settle
  await p.mouse.click(...C.multiplayer); await sleep(3500);   // MULTIPLAYER
  await p.mouse.click(...C.multiplayer); await sleep(7000);   // NETWORK -> LAN lobby
  return p;
}

(async () => {
  const bHost = await puppeteer.launch({ executablePath: CHROME, headless: 'new', args: FLAGS });
  const bJoin = await puppeteer.launch({ executablePath: CHROME, headless: 'new', args: FLAGS });
  try {
    const A = await toLobby(bHost, 1, 'HOST');
    const B = await toLobby(bJoin, 2, 'JOIN');
    console.log('both in LAN lobby');

    await A.mouse.click(...C.createGame); await sleep(6000);  // host creates the game
    await shot(A, 't_host_options.png');
    console.log('host created game');

    await sleep(4000);                                        // let the game appear in B's list
    await B.mouse.click(...C.gameRow); await sleep(1200);     // select the game row
    await shot(B, 't_join_selected.png');
    await B.mouse.click(...C.joinGame); await sleep(7000);    // JOIN GAME
    await shot(B, 't_join_joined.png');
    console.log('joiner joined');

    await B.mouse.click(...C.bottomLeft); await sleep(3000);  // joiner ACCEPTs (ready-up)
    console.log('joiner accepted');

    await A.mouse.click(...C.bottomLeft); await sleep(2000);  // host starts
    console.log('host pressed PLAY GAME');

    for (let s = 0; s < 7; s++) { await sleep(5000); console.log(`  loading... t+${(s + 1) * 5}s`); }
    await shot(A, 't_host_ingame.png');
    await shot(B, 't_join_ingame.png');
    console.log(`done — screenshots in ${OUT}`);
    console.log('PASS criteria: t_host_ingame.png and t_join_ingame.png both show a rendered base.');
  } finally {
    await bHost.close();
    await bJoin.close();
  }
})().catch(e => { console.log('fatal', String(e).slice(0, 400)); process.exit(1); });
