#pragma once

#include <string>
#include <vector>
#include <cstdint>

#include "mapping.hpp"   // For ModuleDescriptor
#include "records.hpp"   // For RawRecord, RecordPayload
// ============================================================================
// Discover BA2 archives for a module (optional asset mounting support)
// ============================================================================
std::vector<std::string> discover_ba2s(const std::string& moduleName);

// ============================================================================
// Scan plugin metadata (TES4 header, ESL flag, FE slot)
// ============================================================================
bool scan_plugin_metadata(const std::string& moduleName, ModuleDescriptor& out);

// ============================================================================
// Scan plugin records (KYWD / WEAP / ARMO / LVLI)
// Returns a vector of RawRecord (defined in records.hpp)
// ============================================================================
std::vector<RawRecord> scan_plugin_records(const std::string& moduleName);
