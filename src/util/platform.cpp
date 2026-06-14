/*
 * wf-shell platform identification.
 *
 * The ONE place where the OS is detected.  All other code calls
 * wf_platform_name() without any #ifdef pollution.
 *
 * Detection is done at compile time via meson's host_machine.system(),
 * passed as a preprocessor macro by src/util/meson.build.
 */

#include "platform.hpp"

/*
 * WFS_PLATFORM_LINUX    — defined by meson on Linux
 * WFS_PLATFORM_FREEBSD  — defined by meson on FreeBSD
 * WFS_PLATFORM_OPENBSD  — defined by meson on OpenBSD
 * WFS_PLATFORM_NETBSD   — defined by meson on NetBSD
 *
 * Unknown platforms fall through to "unknown".
 */

const char *wf_platform_name(void) {
#if defined(WFS_PLATFORM_FREEBSD)
	return "freebsd";
#elif defined(WFS_PLATFORM_OPENBSD)
	return "openbsd";
#elif defined(WFS_PLATFORM_NETBSD)
	return "netbsd";
#elif defined(WFS_PLATFORM_LINUX)
	return "linux";
#else
	return "unknown";
#endif
}
