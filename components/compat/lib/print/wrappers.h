/** Compatibility shim for upstream Flipper external apps.
 *
 * Upstream firmware links the CLI through stdio __wrap_* functions provided by
 * lib/print/. This ESP32 port doesn't use that --wrap mechanism, but some apps
 * (e.g. the authenticator's CLI plugin glue) include this header transitively
 * without actually calling the wrappers. Provide the declarations so those
 * translation units compile; the symbols are never referenced on this port.
 *
 * @file wrappers.h
 */

#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

int __wrap_printf(const char* format, ...);
int __wrap_vsnprintf(char* str, size_t size, const char* format, va_list args);
int __wrap_puts(const char* str);
int __wrap_putchar(int ch);
int __wrap_putc(int ch, FILE* stream);
int __wrap_snprintf(char* str, size_t size, const char* format, ...);
int __wrap_fflush(FILE* stream);

int __wrap_fgetc(FILE* stream);
int __wrap_getc(FILE* stream);
int __wrap_getchar(void);
char* __wrap_fgets(char* str, size_t n, FILE* stream);
int __wrap_ungetc(int ch, FILE* stream);

#ifdef __cplusplus
}
#endif
