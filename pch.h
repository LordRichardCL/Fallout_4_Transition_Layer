#ifndef PCH_H
#define PCH_H

// ======================================================
// Disable Boost auto-linking (prevents LNK1104 errors)
// ======================================================
#define BOOST_ALL_NO_LIB


// ======================================================
// Standard Library
// ======================================================
#include <cstdint>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>
#include <memory>
#include <utility>
#include <array>
#include <string_view>
#include <cassert>
#include <algorithm>
#include <execution>
#include <iostream>
#include <fstream>
#include <unordered_map>

using namespace std::literals;


// ======================================================
// Windows API
// ======================================================
#include <Windows.h>


// ======================================================
// F4SE Required Types (fixes UInt8/UInt16/UInt32/UInt64)
// ======================================================
#include "F4SE_Types.h"


// F4SE Required Types
#include "F4SE_Types.h"

// If the F4SE source tree does not provide _MESSAGE,
// provide a no-op version so F4SE headers compile.
#ifndef _MESSAGE
#define _MESSAGE(...) do { /* F4SE _MESSAGE stub */ } while (0)
#endif

// F4SE (Old‑Gen) Plugin API
#include <f4se_common/Utilities.h>
#include <f4se_common/Relocation.h>
#include <f4se_common/SafeWrite.h>
#include <f4se_common/BranchTrampoline.h>

#include <f4se/PluginAPI.h>
#include <f4se/GameAPI.h>




// ======================================================
// External Libraries
// ======================================================
#include <fmt/core.h>
#include <boost/filesystem.hpp>

#endif // PCH_H
