/* Toolchain smoke test for the PS2 port -- see docs/ps2-port-plan.md, Phase 1.
   Adapted from ps2sdk's samples/debug/helloworld sample: proves the
   mips64r5900el-ps2-elf compiler, linker, and IOP RPC (sceSifInitRpc) all
   work end to end, producing an ELF that boots and prints to the TV output
   under PCSX2 (or real hardware) before any engine code is touched. */

#include <stdio.h>
#include <tamtypes.h>
#include <sifrpc.h>
#include <debug.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    sceSifInitRpc(0);
    init_scr();

    scr_printf("GeneralsGameCode PS2 bringup\n");
    scr_printf("Toolchain + link OK.\n");

    for (;;) {
        sleep(1);
    }

    return 0;
}
