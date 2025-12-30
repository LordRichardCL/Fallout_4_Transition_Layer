#pragma once

#include <vector>
#include <cstdint>
#include <string>

#include "mapping.hpp"      // SlotDescriptor, ModuleDescriptor
#include "csv_loader.hpp"   // CSVSlot

// Mount BA2 archives for the mods in this slot.
bool mount_archives(SlotDescriptor& slot);

// Build form ID maps for this slot.
bool build_form_maps(SlotDescriptor& slot);

// Inject records for this slot using CSV mapping.
bool inject_records(
    const SlotDescriptor& slot,
    const std::vector<CSVSlot>& csvSlots
);

// Injection subsystem context
struct InjectionContext
{
    const SlotDescriptor* slot = 0;
    const std::vector<ModuleDescriptor>* modules = 0;
};

// Initialize injection context.
void InitInjectionContext(
    const SlotDescriptor& slot,
    const std::vector<ModuleDescriptor>& modules
);

// Resolve and possibly rewrite a FormID.
uint32_t ResolveAndRewriteFormID(uint32_t formID);

// Explain why a FormID was rewritten (or not).
std::string ExplainFormIDRewrite(uint32_t originalFormID, uint32_t rewrittenFormID);
