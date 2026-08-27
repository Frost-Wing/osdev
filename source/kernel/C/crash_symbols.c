#include <crash_symbols.h>

__attribute__((weak)) const CrashSymbol wing_crash_symbols[] = {0};
__attribute__((weak)) const uint32 wing_crash_symbol_count = 0;

bool crash_symbols_resolve(uint64 rip, CrashSymbolResult *out) {
    if (!out)
        return false;

    out->found = false;
    out->rip = rip;
    out->function = "unknown";
    out->file = "unknown";
    out->line = 0;
    out->snippet = NULL;
    out->snippet_count = 0;

    for (uint32 i = 0; i < wing_crash_symbol_count; i++) {
        const CrashSymbol *sym = &wing_crash_symbols[i];
        if (rip >= sym->start && rip < sym->end) {
            out->found = true;
            out->function = sym->function ? sym->function : "unknown";
            out->file = sym->file ? sym->file : "unknown";
            out->line = sym->line;
            out->snippet = sym->snippet;
            out->snippet_count = sym->snippet_count;
            return true;
        }
    }

    return false;
}
