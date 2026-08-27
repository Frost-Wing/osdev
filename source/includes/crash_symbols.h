#ifndef CRASH_SYMBOLS_H
#define CRASH_SYMBOLS_H

#include <basics.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct CrashSourceLine {
    uint32 line;
    cstring text;
} CrashSourceLine;

typedef struct CrashSymbol {
    uint64 start;
    uint64 end;
    cstring function;
    cstring file;
    uint32 line;
    const CrashSourceLine *snippet;
    uint32 snippet_count;
} CrashSymbol;

typedef struct CrashSymbolResult {
    bool found;
    uint64 rip;
    cstring function;
    cstring file;
    uint32 line;
    const CrashSourceLine *snippet;
    uint32 snippet_count;
} CrashSymbolResult;

bool crash_symbols_resolve(uint64 rip, CrashSymbolResult *out);

#endif
