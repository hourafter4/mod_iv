#pragma once
// mod_iv.log next to GTAIV.exe. A handful of one-line milestones, each flushed
// and closed straight away so the file survives a crash: when the game dies
// with no error, the last line written says how far the .asi got.
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static void
mod_iv_log (const char *fmt, ...)
{
    char path[MAX_PATH];
    if (!GetModuleFileNameA (NULL, path, MAX_PATH))
        return;
    char *slash = strrchr (path, '\\');
    if (!slash)
        return;
    slash[1] = '\0';
    strncat (path, "mod_iv.log", MAX_PATH - strlen (path) - 1);

    FILE *f = fopen (path, "a");
    if (!f)
        return;

    va_list ap;
    va_start (ap, fmt);
    vfprintf (f, fmt, ap);
    va_end (ap);
    fputc ('\n', f);
    fclose (f);
}
