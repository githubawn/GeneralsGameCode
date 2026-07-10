/* TheSuperHackers @build githubawn 10/07/2026 PS2-only declaration for the
   real ioctl() defined in ps2_ioctl_compat.c. Force-included into
   GameSpySDK's own translation units (which call ioctl(sock, FIONBIO, ...)
   directly and don't see our engine's compat headers) via
   cmake/gamespy.cmake -- see docs/ps2-port-plan.md. */
#pragma once

#if defined(__PS2__)
#ifdef __cplusplus
extern "C" {
#endif
int ioctl(int fd, unsigned long request, ...);
#ifdef __cplusplus
}
#endif
#endif
