#include <stdio.h>
#include <windows.h>
#include "misc.h"
#include "ntapi.h"
#include "pipe.h"
#include "utf8.h"

const char *g_pipe_name;

static int _pipe_utf8x(char **out, unsigned short x)
{
    unsigned char buf[3];
    int len = utf8_encode(x, buf);
    if(*out != NULL) {
        memcpy(*out, buf, len);
        *out += len;
    }
    return len;
}

static int _pipe_ascii(char **out, const char *s, int len)
{
    int ret = 0;
    while (len-- != 0) {
        ret += _pipe_utf8x(out, *(unsigned char *) s++);
    }
    return ret;
}

static int _pipe_unicode(char **out, const wchar_t *s, int len)
{
    int ret = 0;
    while (len-- != 0) {
        ret += _pipe_utf8x(out, *(unsigned short *) s++);
    }
    return ret;
}

static int _pipe_sprintf(char *out, const char *fmt, va_list args)
{
    int ret = 0;
    while (*fmt != 0) {
        if(*fmt != '%') {
            ret += _pipe_utf8x(&out, *fmt++);
            continue;
        }
        if(*++fmt == 'z') {
            const char *s = va_arg(args, const char *);
            if(s == NULL) return -1;

            ret += _pipe_ascii(&out, s, strlen(s));
        }
        else if(*fmt == 'Z') {
            const wchar_t *s = va_arg(args, const wchar_t *);
            if(s == NULL) return -1;

            ret += _pipe_unicode(&out, s, lstrlenW(s));
        }
        else if(*fmt == 's') {
            int len = va_arg(args, int);
            const char *s = va_arg(args, const char *);
            if(s == NULL) return -1;

            ret += _pipe_ascii(&out, s, len < 0 ? (int) strlen(s) : len);
        }
        else if(*fmt == 'S') {
            int len = va_arg(args, int);
            const wchar_t *s = va_arg(args, const wchar_t *);
            if(s == NULL) return -1;

            ret += _pipe_unicode(&out, s, len < 0 ? (int) lstrlenW(s) : len);
        }
        else if(*fmt == 'o') {
            UNICODE_STRING *str = va_arg(args, UNICODE_STRING *);
            if(str == NULL) return -1;

            ret += _pipe_unicode(&out, str->Buffer,
                str->Length / sizeof(wchar_t));
        }
        else if(*fmt == 'O') {
            OBJECT_ATTRIBUTES *obj = va_arg(args, OBJECT_ATTRIBUTES *);
            if(obj == NULL || obj->ObjectName == NULL) return -1;

            wchar_t path[MAX_PATH]; int length;
            length = path_from_object_attributes(obj, path,  MAX_PATH);

            length = ensure_absolute_path(path, path, length);

            ret += _pipe_unicode(&out, path, length);
        }
        else if(*fmt == 'd') {
            char s[32];
            sprintf(s, "%d", va_arg(args, int));
            ret += _pipe_ascii(&out, s, strlen(s));
        }
        else if(*fmt == 'x') {
            char s[16];
            sprintf(s, "%x", va_arg(args, int));
            ret += _pipe_ascii(&out, s, strlen(s));
        }
        fmt++;
    }
    return ret;
}

int pipe(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int len = _pipe_sprintf(NULL, fmt, args);
    if(len > 0) {
        char buf[len + 1];
        _pipe_sprintf(buf, fmt, args);
        va_end(args);

        if(CallNamedPipe(g_pipe_name, buf, len, buf, len,
                (unsigned long *) &len, PIPE_MAX_TIMEOUT) == 0) {
            return 0;
        }
    }
    return -1;
}

int pipe2(void *out, int *outlen, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int len = _pipe_sprintf(NULL, fmt, args);
    if(len > 0) {
        char buf[len + 1];
        _pipe_sprintf(buf, fmt, args);
        va_end(args);

        if(CallNamedPipe(g_pipe_name, buf, len, out, *outlen,
                (unsigned long *) outlen, PIPE_MAX_TIMEOUT) == 0) {
            return 0;
        }
    }
    return -1;
}
