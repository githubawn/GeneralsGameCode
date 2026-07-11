/* clock_gettime investigation -- does ps2sdk's CLOCK_BOOTTIME (id 7) actually
   work, or does it silently fail/zero out like it does on Switch/Emscripten?
   This directly tests the suspected root cause of "menu never navigates /
   game logic never progresses" on PS2: time_compat.h's timeGetTime() calls
   clock_gettime(CLOCK_BOOTTIME, &ts) on any platform that doesn't hit the
   __EMSCRIPTEN__/__SWITCH__ special case, and __PS2__ doesn't hit it either. */

#include <debug.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "../../Dependencies/Utility/Utility/time_compat.h"

static void TryClock(FILE *f, const char *label, clockid_t id)
{
    struct timespec ts;
    memset(&ts, 0xAA, sizeof(ts));  // poison so we can tell if it was untouched
    errno = 0;
    int rc = clock_gettime(id, &ts);
    fprintf(f, "[%s] clock_gettime(id=%d) rc=%d errno=%d sec=%ld nsec=%ld\n",
        label, (int)id, rc, errno, (long)ts.tv_sec, (long)ts.tv_nsec);
    fflush(f);
}

int main(int argc, char *argv[])
{
    init_scr();
    scr_printf("clock_gettime bringup starting...\n");

    FILE *f = fopen("host:clock_diag.txt", "w");
    if (f == nullptr) {
        scr_printf("FAILED to open host:clock_diag.txt\n");
        for (;;) { sleep(1); }
    }

    fprintf(f, "CLOCK_BOOTTIME=%d CLOCK_MONOTONIC=%d CLOCK_REALTIME=%d\n",
        (int)CLOCK_BOOTTIME, (int)CLOCK_MONOTONIC, (int)CLOCK_REALTIME);

    // Call each three times, a few ms apart, to see if the value actually
    // advances (a real clock) vs staying frozen at 0 (a broken/unsupported id).
    for (int i = 0; i < 3; ++i) {
        TryClock(f, "BOOTTIME", CLOCK_BOOTTIME);
        TryClock(f, "MONOTONIC", CLOCK_MONOTONIC);
        TryClock(f, "REALTIME", CLOCK_REALTIME);
        unsigned int tgt = timeGetTime();
        unsigned int gtc = GetTickCount();
        fprintf(f, "[compat] timeGetTime()=%u GetTickCount()=%u\n", tgt, gtc);
        fprintf(f, "---\n");
        for (volatile long j = 0; j < 20000000; ++j) {} // crude delay
    }

    fclose(f);
    scr_printf("Done, wrote host:clock_diag.txt\n");

    for (;;) {
        sleep(1);
    }
    return 0;
}
