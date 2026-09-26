#include <crash_symbols.h>

__attribute__((weak)) const CrashSymbol wing_crash_symbols[] = {0};
__attribute__((weak)) const uint32 wing_crash_symbol_count = 0;

// gen_crash_symbols.py emits wing_crash_symbols[] in strictly ascending
// order of `start` (it walks the DWARF line table in address order), so
// we can binary search for the entry whose range contains `rip` instead
// of scanning linearly. This keeps panic-handler resolution fast even
// as the kernel and its symbol table grow into the thousands of entries.
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

    if (wing_crash_symbol_count == 0)
        return false;

    // Find the first index whose start > rip (upper bound), then step
    // back one: that's the last symbol whose start <= rip.
    uint32 lo = 0, hi = wing_crash_symbol_count;
    while (lo < hi) {
        uint32 mid = lo + (hi - lo) / 2;
        if (wing_crash_symbols[mid].start <= rip)
            lo = mid + 1;
        else
            hi = mid;
    }

    if (lo == 0)
        return false; // rip is before the first known symbol

    const CrashSymbol *sym = &wing_crash_symbols[lo - 1];
    if (rip < sym->start || rip >= sym->end)
        return false; // rip falls in a gap between symbol ranges

    out->found = true;
    out->function = sym->function ? sym->function : "unknown";
    out->file = sym->file ? sym->file : "unknown";
    out->line = sym->line;
    out->snippet = sym->snippet;
    out->snippet_count = sym->snippet_count;
    return true;
}