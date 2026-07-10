/* std::filesystem::create_directories EPERM investigation, round 2 --
   testing whether the leading "./" or the host: mount specifically is the
   trigger, and whether plain mkdir() (not std::filesystem) succeeds on the
   identical path. */

#include <debug.h>
#include <unistd.h>
#include <filesystem>
#include <string>
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>

static void TryCreate(FILE *f, const char *label, const char *dir)
{
    std::error_code ec;
    bool result = std::filesystem::create_directories(dir, ec);
    fprintf(f, "[%s] std::filesystem::create_directories('%s') result=%d ec=%d (%s)\n",
        label, dir, (int)result, ec.value(), ec.message().c_str());

    errno = 0;
    int mkr = mkdir(dir, 0755);
    fprintf(f, "[%s] plain mkdir('%s') = %d errno=%d (%s)\n",
        label, dir, mkr, errno, strerror(errno));
}

int main(int argc, char *argv[])
{
    init_scr();
    scr_printf("PS2 std::filesystem bringup round 2\n");

    FILE *f = fopen("host:fs_result.txt", "w");
    if (f == nullptr) {
        scr_printf("fopen FAILED\n");
        for (;;) { sleep(1); }
    }

    TryCreate(f, "relative-dot", "./TestDirA/");
    TryCreate(f, "relative-plain", "TestDirB/");
    TryCreate(f, "host-prefixed", "host:TestDirC/");

    fflush(f);
    fclose(f);

    scr_printf("Done.\n");
    for (;;) {
        sleep(1);
    }

    return 0;
}
