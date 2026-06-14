#pragma once

/**
 * wf-shell platform identification.
 *
 * All OS-specific detection lives here.  Callers use wlr_platform_name()
 * without any #ifdef pollution.
 */

/**
 * Return the OS name in lowercase, e.g. "linux", "freebsd", "openbsd".
 * Detected at compile time via meson's host_machine.system().
 */
const char *wf_platform_name(void);
