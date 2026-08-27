#ifndef CRASH_DIAGNOSTICS_H
#define CRASH_DIAGNOSTICS_H

#include <basics.h>
#include <crash_symbols.h>
#include <isr.h>

typedef enum CrashConfidence {
    CRASH_CONFIDENCE_UNKNOWN = 0,
    CRASH_CONFIDENCE_LOW,
    CRASH_CONFIDENCE_MEDIUM,
    CRASH_CONFIDENCE_HIGH,
} CrashConfidence;

typedef struct CrashDiagnosis {
    cstring what_happened;
    cstring likely_cause;
    cstring what_to_fix;
    cstring source_hint;
    CrashConfidence confidence;
} CrashDiagnosis;

void crash_diagnostics_analyze(uint64 int_no, uint64 error_code, uint64 cr2, const InterruptFrame *frame, const CrashSymbolResult *symbol, CrashDiagnosis *out);
cstring crash_confidence_string(CrashConfidence confidence);

#endif
