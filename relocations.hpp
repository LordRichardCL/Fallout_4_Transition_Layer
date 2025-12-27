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

// NEW: FormID lookup function type.
// Adjust the name/signature if your target differs.
class TESForm;
using LookupFormByID_t =
TESForm * (*)(std::uint32_t formID);

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

    // NEW: LookupFormByID (offset placeholder!)
    //
    // IMPORTANT:
    // - Replace 0x00000000 with the REAL offset for your chosen
    //   FormID lookup function (from 1.10.163).
    // - Until you do, the hook installer will see 0 and skip installing.
    inline RelocAddr<LookupFormByID_t> LookupFormByID(0x003C2F90);

}
