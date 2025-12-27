#pragma once

#include <cstdint>

// Forward declaration; you already have this in your F4SE / game headers.
class TESForm;

// Initialize runtime hooks that use ResolveAndRewriteFormID.
// Call this AFTER InitInjectionContext.
bool InitRuntimeHooks();
