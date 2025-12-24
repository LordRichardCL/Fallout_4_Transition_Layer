#pragma once

#include "mapping.hpp"
#include "csv_loader.hpp"   // NEW: for CSVSlot definition
#include <vector>

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
    const std::vector<CSVSlot>& csvSlots   // UPDATED: slot-level mapping
);
