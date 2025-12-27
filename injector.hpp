#pragma once

#include <vector>
#include <cstdint>
#include <string>

#include "mapping.hpp"      // for SlotDescriptor, ModuleDescriptor
#include "csv_loader.hpp"   // for CSVSlot


// ============================================================================
// Existing injector API (unchanged)
// ============================================================================

// Mount BA2 archives for the mods in this slot.
// Returns true on success, false on failure.
bool mount_archives(SlotDescriptor& slot);

// Build form ID maps for this slot.
// Returns true on success, false on failure.
bool build_form_maps(SlotDescriptor& slot);

// Inject cloned or created records into the runtime for this slot,
// using the CSV slot mapping to determine which dummy plugin each
// source plugin should be routed into.
// Returns true on success, false on failure.
bool inject_records(
    const SlotDescriptor& slot,
    const std::vector<CSVSlot>& csvSlots
);

// ============================================================================
// Injection subsystem (NEW)
// ============================================================================

// Holds pointers to the slot + module data needed for runtime redirection.
struct InjectionContext
{
    const SlotDescriptor* slot = nullptr;
    const std::vector<ModuleDescriptor>* modules = nullptr;
};

// Initialize the injection subsystem.
// Must be called AFTER mapping + scanning + visibility snapshot.
void InitInjectionContext(
    const SlotDescriptor& slot,
    const std::vector<ModuleDescriptor>& modules
);

// Resolve and possibly rewrite a FormID based on slot.cfg + module mapping.
// For now (Chunk 1), this is a no-op that returns the original ID.
uint32_t ResolveAndRewriteFormID(uint32_t formID);

// Explain why a FormID was rewritten (or not).
// For now (Chunk 1), returns a simple placeholder string.
std::string ExplainFormIDRewrite(uint32_t originalFormID, uint32_t rewrittenFormID);
