/**
 * ==================================================
 * SerialUART Project
 * David Refoua <David@Refoua.me>
 * ==================================================
 * 
 * Filename: SerialUART.h
 * Version:  v1.3-dev
 * 
 * Description: This header file is used to include 
 *     platform specific libraries to communicate with
 *     the serial port.
 * 
 */

// ------------------------------------------------------------------
// Include required OS-specific headers and libraries
// ------------------------------------------------------------------
//
// NOTE: currently only Windows and Linux-based distros are supported.
//       contact me to add support for more platforms.
//

#ifndef __SERIALUART_H__
#define __SERIALUART_H__

#ifdef OS_WIN
// NOTE: used SDKDDKVer instead of WINVER to make it automatically defined on MinGW and Cygwin compilation.
//#define WINVER 0x0500
#include <SDKDDKVer.h>
#include <windows.h>
#endif

#ifdef OS_LINUX
#include <limits.h>   /* */
#include <errno.h>    /* Error number definitions */
#include <fcntl.h>    /* File control definitions */
#include <termios.h>  /* POSIX terminal control definitions */
#include <unistd.h>   /* UNIX standard function definitions */
#include <sys/ioctl.h>
#endif

// Platform independent headers
#include <stdio.h>    /* Standard input/output definitions */
#include <string.h>   /* String function definitions */
#include <stdlib.h>   /* */
#include <stdbool.h>  /* */
#include <time.h>     /* */

// TODO: include defined from here, for example #define EXIT_TIMEDOUT 124
// https://github.com/wertarbyte/coreutils/blob/master/src/timeout.c

#endif
