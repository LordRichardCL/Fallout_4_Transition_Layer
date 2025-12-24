#pragma once

#include "pch.h"
#include "F4SE_Types.h"
#include <f4se_common/Relocation.h>
#include <f4se/GameData.h>

// ============================================================================
// Function pointer types
// ============================================================================

using LookupModByName_t =
void* (*)(const char* name);

using GetLoadedModIndex_t =
UInt8(*)(const char* name);

// ============================================================================
// Relocation namespace (Old‑Gen FO4 1.10.163)
// ============================================================================
//
// These offsets are stable and correct for the last Old‑Gen runtime.
// They come directly from the F4SE 0.6.23 source tree.
//
// ============================================================================

namespace Reloc
{
    // LookupModByName @ 0x003C2F30
    inline RelocAddr<LookupModByName_t> LookupModByName(0x003C2F30);

    // GetLoadedModIndex @ 0x003C2F70
    inline RelocAddr<GetLoadedModIndex_t> GetLoadedModIndex(0x003C2F70);
}
