#include "pch.h"

#include "runtime_hooks.hpp"
#include "injector.hpp"      // For ResolveAndRewriteFormID / InitInjectionContext
#include "log.hpp"
#include "diagnostics.h"
#include "config.hpp"


// Include your F4SE trampoline header here.
// #include "f4se_common/BranchTrampoline.h"
// #include "f4se_common/SafeWrite.h"
// etc.

// --------------------------------------------------------------------------
// Global hook pointers
// --------------------------------------------------------------------------

// Example target signature: adjust if needed to match the real function.
using LookupFormByID_t = TESForm * (*)(std::uint32_t formID);

// Pointer to the original game function.
static LookupFormByID_t g_LookupFormByID_Original = nullptr;
// --------------------------------------------------------------------------
// Detour: wraps the game's lookup function and applies FormID rewrites
// --------------------------------------------------------------------------

// This is the function that will be called instead of the original.
// It calls ResolveAndRewriteFormID, then forwards to the original.
static TESForm* LookupFormByID_Detour(std::uint32_t formID)
{
    // Let the injection subsystem decide whether to rewrite.
    std::uint32_t rewritten = ResolveAndRewriteFormID(formID);

    if (rewritten != formID) {
        Diagnostics_RecordEvent(
            DiagnosticsEventType::Info,
            "LookupFormByID detour: rewriting FormID from 0x" +
            std::to_string(formID) + " to 0x" + std::to_string(rewritten)
        );

        if (g_eslDebug) {
            logf("LookupFormByID detour: %08X -> %08X", formID, rewritten);
        }
    }

    // Call the original function with the possibly rewritten ID.
    if (!g_LookupFormByID_Original) {
        // Safety: if somehow the original isn't set, log and bail.
        logf("ERROR: g_LookupFormByID_Original is null in detour; returning nullptr.");
        return nullptr;
    }

    return g_LookupFormByID_Original(rewritten);
}
// --------------------------------------------------------------------------
// Hook initialization
// --------------------------------------------------------------------------

// You will need to replace this with the actual address of the game's
// LookupFormByID-equivalent function, and ensure it matches the signature.
static constexpr std::uintptr_t kLookupFormByID_Address = 0x00000000; // TODO: set real address

bool InitRuntimeHooks()
{
    // Sanity: injection context must be initialized first.
    if (!/* optional: check internal context if you expose it */ true) {
        // You can add a dedicated function to query injection init state if desired.
        logf("InitRuntimeHooks: WARNING: cannot verify injection context; continuing anyway.");
    }

    if (kLookupFormByID_Address == 0x00000000) {
        logf("InitRuntimeHooks: ERROR: kLookupFormByID_Address is not set. Skipping hook install.");
        Diagnostics_RecordEvent(
            DiagnosticsEventType::Error,
            "InitRuntimeHooks: LookupFormByID address not set; hook not installed."
        );
        return false;
    }

    // Cast the game function address to our function pointer type.
    g_LookupFormByID_Original =
        reinterpret_cast<LookupFormByID_t>(kLookupFormByID_Address);

    // TODO: Use your actual trampoline / patch mechanism here.
    // The pseudocode below assumes a BranchTrampoline-style API.

    /*
    if (!g_branchTrampoline.Create(1024 * 64)) {
        logf("InitRuntimeHooks: ERROR: failed to create branch trampoline.");
        Diagnostics_RecordEvent(
            DiagnosticsEventType::Error,
            "InitRuntimeHooks: failed to create branch trampoline."
        );
        return false;
    }

    // Write the detour: patch the game function to jump to our detour.
    g_branchTrampoline.Write5Branch(
        kLookupFormByID_Address,
        reinterpret_cast<std::uintptr_t>(&LookupFormByID_Detour)
    );
    */

    // For now, just log that we "initialized" the hook, even if you
    // haven't wired the trampoline yet.
    logf("InitRuntimeHooks: LookupFormByID hook prepared at 0x%p (original addr=0x%08X)",
        g_LookupFormByID_Original,
        static_cast<std::uint32_t>(kLookupFormByID_Address));

    Diagnostics_RecordEvent(
        DiagnosticsEventType::Info,
        "InitRuntimeHooks: LookupFormByID hook initialized (trampoline wiring TBD)."
    );

    return true;
}
