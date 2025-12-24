#include "pch.h"

#include "injector.hpp"
#include "mapping.hpp"
#include "log.hpp"

// F4SE headers
#include "f4se/PluginAPI.h"
#include "f4se/Interfaces.h"
#include "f4se_common/Types.h"

#include <unordered_map>

static SlotMap gSlots;

extern "C" {

    __declspec(dllexport)
        bool F4SEPlugin_Query(const F4SE::QueryInterface* f4se, F4SE::PluginInfo* info) {
        info->infoVersion = F4SE::PluginInfo::kInfoVersion;
        info->name = "aSW Multiplexer";
        info->version = 1;

        if (f4se->IsEditor()) {
            logf("aSW Multiplexer: Not loading in editor.");
            return false;
        }

        logf("aSW Multiplexer: Query successful.");
        return true;
    }

    __declspec(dllexport)
        bool F4SEPlugin_Load(const F4SE::LoadInterface* f4se) {
        logf("aSW Multiplexer loading...");

        if (!load_mapping("Data\\config\\loadorder_mapped_filtered_clean.csv", gSlots)) {
            logf("ERROR: Failed to load mapping CSV");
            return false;
        }

        logf("Loaded {} slots from mapping", gSlots.size());

        for (auto& [name, slot] : gSlots) {
            logf("Processing slot {} (index {:02X})", slot.dummyName, slot.fileIndex);

            if (!mount_archives(slot)) {
                logf("ERROR: Failed to mount archives for {}", slot.dummyName);
                continue;
            }

            if (!build_form_maps(slot)) {
                logf("ERROR: Failed to build form maps for {}", slot.dummyName);
                continue;
            }

            if (!inject_records(slot)) {
                logf("ERROR: Failed to inject records for {}", slot.dummyName);
                continue;
            }

            logf("Slot {} processed successfully", slot.dummyName);
        }

        logf("aSW Multiplexer initialized successfully.");
        return true;
    }
}
