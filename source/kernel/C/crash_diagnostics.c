#include <crash_diagnostics.h>
#include <strings.h>

static bool address_near_null(uint64 address) {
    return address <= 0x1000;
}

static bool source_has_pointer_access(cstring s) {
    if (!s)
        return false;
    return strstr(s, "->") || strstr(s, "*") || strstr(s, "[");
}

static cstring crashing_line(const CrashSymbolResult *symbol) {
    if (!symbol || !symbol->snippet)
        return NULL;
    for (uint32 i = 0; i < symbol->snippet_count; i++) {
        if (symbol->snippet[i].line == symbol->line)
            return symbol->snippet[i].text;
    }
    return NULL;
}

cstring crash_confidence_string(CrashConfidence confidence) {
    switch (confidence) {
        case CRASH_CONFIDENCE_HIGH:
            return "HIGH";
        case CRASH_CONFIDENCE_MEDIUM:
            return "MEDIUM";
        case CRASH_CONFIDENCE_LOW:
            return "LOW";
        default:
            return "UNKNOWN";
    }
}

void crash_diagnostics_analyze(uint64 int_no, uint64 error_code, uint64 cr2, const InterruptFrame *frame, const CrashSymbolResult *symbol, CrashDiagnosis *out) {
    (void)frame;
    if (!out)
        return;

    out->what_happened = "The CPU raised an exception while executing kernel code.";
    out->likely_cause = "The available crash state is not enough to identify a specific cause.";
    out->what_to_fix = "Inspect the crashing source line and the code that runs immediately before it. Check pointer values, assumptions, and recent memory writes.";
    out->source_hint = "Use the source location above as the first place to inspect.";
    out->confidence = CRASH_CONFIDENCE_UNKNOWN;

    cstring line = crashing_line(symbol);
    bool pointerish = source_has_pointer_access(line);

    if (int_no == 14) {
        bool present = (error_code & 0x1) != 0;
        bool write = (error_code & 0x2) != 0;
        bool user = (error_code & 0x4) != 0;
        bool reserved = (error_code & 0x8) != 0;
        bool fetch = (error_code & 0x10) != 0;

        if (address_near_null(cr2)) {
            out->what_happened = write ?
                "Kernel code attempted to write to a virtual address very close to NULL." :
                (fetch ? "Kernel code attempted to execute instructions from an address very close to NULL." :
                         "Kernel code attempted to read from a virtual address very close to NULL.");
            out->likely_cause = pointerish ?
                "Possible NULL pointer dereference. The source line contains pointer-like access, and the CPU fault address is close to NULL, which commonly means a NULL structure pointer plus a member offset." :
                "Possible NULL pointer dereference. The CPU fault address is close to NULL, but the source line does not clearly identify which value was invalid.";
            out->what_to_fix = pointerish ?
                "Check every pointer dereference on the crashing line. Then inspect where those pointers were assigned and whether any helper can return NULL. If NULL is valid, handle it before dereferencing." :
                "Trace the address used by the crashing instruction. Check whether a NULL pointer or small integer was used as a pointer before this exception.";
            out->source_hint = "First inspect the crashing line and the assignments immediately above it.";
            out->confidence = pointerish ? CRASH_CONFIDENCE_HIGH : CRASH_CONFIDENCE_MEDIUM;
            return;
        }

        if (reserved) {
            out->what_happened = "The CPU detected reserved bits set in a paging structure while translating a virtual address.";
            out->likely_cause = "A page-table entry is probably malformed or corrupted.";
            out->what_to_fix = "Inspect the page-table entry for the faulting address. Verify that only architecturally valid bits are set for this CPU mode.";
            out->confidence = CRASH_CONFIDENCE_HIGH;
            return;
        }

        if (fetch) {
            out->what_happened = "Kernel code attempted to execute instructions from a virtual address that is not executable or not mapped.";
            out->likely_cause = "Possible bad function pointer, corrupted return address, jump to unmapped memory, or NX page violation.";
            out->what_to_fix = "Check indirect calls, function pointers, return addresses, and page permissions near the crashing location.";
            out->confidence = CRASH_CONFIDENCE_MEDIUM;
            return;
        }

        if (!present) {
            out->what_happened = write ?
                (user ? "User-mode code attempted to write to a virtual address whose page is not present." : "Kernel code attempted to write to a virtual address whose page is not mapped.") :
                (user ? "User-mode code attempted to read from a virtual address whose page is not present." : "Kernel code attempted to read from a virtual address whose page is not mapped.");
            out->likely_cause = "The address was not mapped at the time of access. This may be an invalid pointer, missing page-table mapping, use-after-free, or corrupted address.";
            out->what_to_fix = pointerish ?
                "Check the pointer used on the crashing source line. Verify where it was assigned, whether its page should be mapped, and whether it could have been freed or corrupted earlier." :
                "Verify the virtual address is intentionally mapped before this code runs. If it comes from a pointer, trace where that pointer was assigned.";
            out->confidence = CRASH_CONFIDENCE_LOW;
            return;
        }

        out->what_happened = write ? "Kernel code attempted a write that violated page permissions." : "Kernel code attempted a read that violated page permissions.";
        out->likely_cause = "The page exists, but its permissions do not allow this access.";
        out->what_to_fix = "Check the page-table flags for the faulting address and make sure the access type matches the mapping permissions.";
        out->confidence = CRASH_CONFIDENCE_MEDIUM;
        return;
    }

    if (int_no == 13) {
        out->what_happened = "The CPU raised a General Protection Fault.";
        out->likely_cause = "Possible invalid descriptor or segment state, privilege violation, non-canonical address, or invalid CPU state. The error code may identify a selector, but it does not prove one specific C expression caused it.";
        out->what_to_fix = "Inspect descriptor-table setup, privilege transitions, interrupt/syscall state, and any pointer or control-register operation near the crashing line.";
        out->confidence = CRASH_CONFIDENCE_LOW;
    } else if (int_no == 6) {
        out->what_happened = "The CPU encountered an instruction it could not execute.";
        out->likely_cause = "Possible corrupted code, bad function pointer, jump to invalid memory, or CPU-incompatible instruction generation.";
        out->what_to_fix = "Check indirect branches and function pointers near the crashing location. Also verify compiler CPU flags if this happens on real hardware.";
        out->confidence = CRASH_CONFIDENCE_MEDIUM;
    } else if (int_no == 0) {
        out->what_happened = "The CPU attempted an invalid division operation.";
        out->likely_cause = "A divide or modulo operation likely used zero as the divisor, or produced a quotient too large for the destination.";
        out->what_to_fix = "Inspect divisions and modulo operations on or immediately before the crashing source line. Validate the divisor before executing the operation.";
        out->confidence = CRASH_CONFIDENCE_MEDIUM;
    } else if (int_no == 8) {
        out->what_happened = "The CPU raised a Double Fault. Another exception likely occurred while the CPU was trying to handle an earlier exception.";
        out->likely_cause = "Possible stack, IDT/TSS, interrupt-handler, or page-table problem during exception delivery.";
        out->what_to_fix = "Inspect the kernel stack, IST/TSS setup if used, IDT entries, and any recent changes to exception handling or paging.";
        out->confidence = CRASH_CONFIDENCE_LOW;
    } else if (int_no == 12) {
        out->what_happened = "The CPU detected a stack-segment fault.";
        out->likely_cause = "Possible invalid stack selector, stack limit/state issue, or corrupted stack transition.";
        out->what_to_fix = "Inspect stack setup, TSS/privilege transitions, and code that changes RSP or segment state.";
        out->confidence = CRASH_CONFIDENCE_MEDIUM;
    }
}
