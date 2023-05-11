/**
 * ==================================================
 * SerialUART Project
 * David Refoua <David@Refoua.me>
 * ==================================================
 * 
 * Filename: os_macros.h
 * Version:  b1.1 improved
 * 
 * Description: This header file is used to correctly detect 
 *     different platforms and operating systems thus loading 
 *     the OS-specific low level serial port APIs.
 * 
 */

#ifndef __OS_MACROS_H__
#define __OS_MACROS_H__

// Detect the OS and compiler using predefined macros
// To port to other operating systems, read the wiki:
// https://sourceforge.net/p/predef/wiki/OperatingSystems/
// https://stackoverflow.com/a/22622250/1454514

// Windows OSes
#if (defined _WIN32 || defined _WIN64 || defined __WIN__ || defined __CYGWIN__)
#define OS_WIN
#endif

// Linux-based OSes
#if (defined linux || defined __linux || defined __linux__ || defined __gnu_linux__ || defined __CYGWIN__)
#define OS_LINUX
#endif

// BSD-based OSes
#if (defined __FreeBSD__ || defined __NetBSD__ || defined __OpenBSD__)
#define OS_BSD
#endif

// Apple OSes
#if (defined macintosh || defined __APPLE__ || defined __APPLE_CC__)
#define OS_APPLE
#endif

// Unix-like OSes
#if (defined _POSIX_VERSION || defined __unix__)
#define OS_UNIX
#endif

// Android OS
#if (defined android || defined __ANDROID__ || defined ANDROID)
#define OS_ANDROID
#endif

// Solaris OS
#if (defined sun || defined __sun)
#define OS_SOLARIS
#endif

// AIX OS
#if (defined _AIX)
#define OS_AIX
#endif

// Now check for POSIX-compatible OSes
#if (defined OS_LINUX || defined OS_BSD || defined OS_UNIX || defined OS_APPLE || defined OS_SOLARIS)
#define OS_POSIX
#endif

#endif // __OS_MACROS_H__
