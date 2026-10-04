/**
 * @file crash_diagnostics.h
 * @brief Crash-diagnosis data types and analysis interfaces.
 */
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

/**
 * @brief Analyze an interrupt state and produce a crash diagnosis.
 *
 * @param int_no Interrupt number.
 * @param error_code Error code supplied by the processor.
 * @param cr2 Faulting address from CR2.
 * @param frame Saved interrupt frame.
 * @param symbol Resolved instruction symbol, if available.
 * @param out Receives the diagnosis.
 */
void crash_diagnostics_analyze(uint64 int_no, uint64 error_code, uint64 cr2, const InterruptFrame *frame, const CrashSymbolResult *symbol, CrashDiagnosis *out);

/** @brief Return the display string for a crash-confidence level. */
cstring crash_confidence_string(CrashConfidence confidence);

#endif
