#pragma once

#include <string>
#include <vector>
#include <cstdint>

#include "mapping.hpp"   // ModuleDescriptor
#include "records.hpp"   // RawRecord, RecordPayload

// Discover BA2 archives for a module (optional asset mounting support)
std::vector<std::string> discover_ba2s(const std::string& moduleName);

// Scan plugin metadata (TES4 header, ESL flag, FE slot)
bool scan_plugin_metadata(const std::string& moduleName, ModuleDescriptor& out);

// Scan plugin records (KYWD / WEAP / ARMO / LVLI / etc.).
// Overload 1: legacy form, does not propagate worldspace flag.
// Overload 2: preferred form, allows scanner to set module.containsWorldspace.
std::vector<RawRecord> scan_plugin_records(const std::string& moduleName);
std::vector<RawRecord> scan_plugin_records(const std::string& moduleName, ModuleDescriptor& module);
