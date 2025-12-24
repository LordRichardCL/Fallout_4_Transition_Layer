#include "pch.h"

#include "injector.hpp"
#include "scanner.hpp"
#include "mapping.hpp"
#include "log.hpp"
#include "csv_loader.hpp"
#include "config.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>

// ============================================================================
// Internal helpers
// ============================================================================

namespace
{
    // Compose a full FormID from a dummy slot virtual ID + local form ID.
    // virtualID behaves like a synthetic file index (high byte).
    inline std::uint32_t compose_formid(std::uint32_t virtualID, std::uint32_t local)
    {
        return (virtualID << 24) | (local & 0x00FFFFFFu);
    }

    // ESL-aware LVLI reference remap.
    // - For non-ESL modules:
    //     - if hi-byte == 0, treat as local, key = full 0x00FFFFFF
    // - For ESL modules:
    //     - if hi-byte == 0, treat as local compact ID (0x000–0xFFF)
    //     - if hi-byte == 0xFE and eslSlot matches, treat as local compact ID
    inline std::uint32_t remap_lvli_ref(
        std::uint32_t refFormID,
        const std::unordered_map<std::uint32_t, std::uint32_t>& formIdMap,
        bool isESL,
        std::uint16_t eslSlot)
    {
        const std::uint32_t hi = (refFormID & 0xFF000000u);
        const std::uint32_t local = (refFormID & 0x00FFFFFFu);

        if (!isESL) {
            if (hi == 0x00000000u) {
                auto it = formIdMap.find(local);
                if (it != formIdMap.end()) {
                    return it->second;
                }
            }
            return refFormID;
        }

        // ESL path
        if (hi == 0x00000000u) {
            const std::uint32_t compact = local & 0x0FFFu;
            auto it = formIdMap.find(compact);
            if (it != formIdMap.end()) {
                return it->second;
            }
            return refFormID;
        }

        if (hi == 0xFE000000u) {
            const std::uint16_t slot = static_cast<std::uint16_t>((local >> 12) & 0x0FFFu);
            const std::uint32_t compact = local & 0x0FFFu;

            if (slot == eslSlot) {
                auto it = formIdMap.find(compact);
                if (it != formIdMap.end()) {
                    return it->second;
                }
            }
        }

        return refFormID;
    }

    // Stub: inject a single record into the runtime.
    bool inject_single_record_stub(
        std::uint32_t targetFormID,
        std::uint32_t recordType,
        const RecordPayload& payload,
        const std::unordered_map<std::uint32_t, std::uint32_t>& formIdMap,
        const std::string& moduleName,
        bool isESL,
        std::uint16_t eslSlot)
    {
        logf("Stub inject: module=%s, type=%08X, targetFormID=%08X, editorID=%s",
            moduleName.c_str(),
            recordType,
            targetFormID,
            payload.editorName.c_str());

        for (const auto& entry : payload.lvliEntries) {
            auto remapped = remap_lvli_ref(entry.formID, formIdMap, isESL, eslSlot);
            if (remapped != entry.formID && g_eslDebug) {
                logf("  LVLI entry remap (%s): %08X -> %08X",
                    isESL ? "ESL" : "STD",
                    entry.formID,
                    remapped);
            }
        }

        return true;
    }

    // Find which CSVSlot a plugin belongs to.
    const CSVSlot* find_slot_for_plugin(
        const std::vector<CSVSlot>& slots,
        const std::string& pluginName)
    {
        for (const auto& slot : slots) {
            for (const auto& mod : slot.sourceMods) {
                if (_stricmp(mod.c_str(), pluginName.c_str()) == 0) {
                    return &slot;
                }
            }
        }
        return nullptr;
    }
}

// ============================================================================
// Discover and mount BA2 archives for all modules within a slot
// ============================================================================

bool mount_archives(SlotDescriptor& slot)
{
    for (auto& m : slot.modules) {
        m.ba2Paths = discover_ba2s(m.name);
        for (const auto& a : m.ba2Paths) {
            logf("Mount BA2 (stub, no actual mount): %s", a.c_str());
        }
    }
    return true;
}

// ============================================================================
// Build per-module mapping from local form IDs to composed target IDs
// ESL-aware: keys are compact IDs (0x000–0xFFF) for ESL plugins.
// ============================================================================

bool build_form_maps(SlotDescriptor& slot)
{
    std::uint32_t subBase = 0x000100u;

    log_progress("Building form maps", 0, static_cast<int>(slot.modules.size()));

    for (std::size_t mi = 0; mi < slot.modules.size(); ++mi) {
        auto& m = slot.modules[mi];

        if (g_eslDebug) {
            logf("Building form map for module '%s' (ESL=%s, eslSlot=%u)",
                m.name.c_str(),
                m.isESL ? "YES" : "NO",
                m.eslSlot);
        }

        const auto recs = scan_plugin_records(m.name);
        for (const auto& r : recs) {
            const std::uint32_t localKey = m.isESL
                ? (r.localFormID & 0x00000FFFu)
                : (r.localFormID & 0x00FFFFFFu);

            const auto target = compose_formid(slot.fileIndex, subBase + localKey);

            auto [it, inserted] = m.formIdMap.emplace(localKey, target);
            if (!inserted && g_eslDebug) {
                logf("WARNING: Duplicate local key %06X in module '%s' (ESL=%s)",
                    localKey,
                    m.name.c_str(),
                    m.isESL ? "YES" : "NO");
            }

            if (g_eslDebug && m.isESL) {
                logf("ESL compact key: %s:%03X -> target %08X",
                    m.name.c_str(),
                    localKey,
                    target);
            }
        }

        logf("Form map built for %s: %zu entries", m.name.c_str(), m.formIdMap.size());
        log_progress("Building form maps",
            static_cast<int>(mi + 1),
            static_cast<int>(slot.modules.size()));

        subBase += 0x000400u;
    }

    return true;
}

// ============================================================================
// Inject using plugin-level CSV routing, ESL-aware LVLI remap
// ============================================================================

bool inject_records(
    const SlotDescriptor& slot,
    const std::vector<CSVSlot>& csvSlots)
{
    log_progress("Injecting modules", 0, static_cast<int>(slot.modules.size()));

    for (std::size_t mi = 0; mi < slot.modules.size(); ++mi) {
        const auto& m = slot.modules[mi];

        const CSVSlot* csvSlot = find_slot_for_plugin(csvSlots, m.name);
        if (!csvSlot) {
            logf("WARNING: Plugin '%s' not found in CSV — skipping.", m.name.c_str());
            log_progress("Injecting modules",
                static_cast<int>(mi + 1),
                static_cast<int>(slot.modules.size()));
            continue;
        }

        if (g_eslDebug) {
            logf("Routing plugin '%s' (ESL=%s, eslSlot=%u) into dummy slot '%s' (VirtualID=%u)",
                m.name.c_str(),
                m.isESL ? "YES" : "NO",
                m.eslSlot,
                csvSlot->dummyPlugin.c_str(),
                csvSlot->virtualID);
        }

        const auto recs = scan_plugin_records(m.name);
        if (recs.empty()) {
            logf("No records to inject for %s", m.name.c_str());
            log_progress("Injecting modules",
                static_cast<int>(mi + 1),
                static_cast<int>(slot.modules.size()));
            continue;
        }

        logf("Injecting %zu records for %s (stub)", recs.size(), m.name.c_str());
        log_progress("Injecting " + m.name, 0, static_cast<int>(recs.size()));

        std::size_t injected = 0;

        for (std::size_t i = 0; i < recs.size(); ++i) {
            const auto& r = recs[i];

            const std::uint32_t localKey = m.isESL
                ? (r.localFormID & 0x00000FFFu)
                : (r.localFormID & 0x00FFFFFFu);

            const std::uint32_t targetFormID =
                compose_formid(csvSlot->virtualID, localKey);

            if (inject_single_record_stub(
                targetFormID,
                r.type,
                r.payload,
                m.formIdMap,
                m.name,
                m.isESL,
                m.eslSlot))
            {
                ++injected;
            }

            log_progress("Injecting " + m.name,
                static_cast<int>(i + 1),
                static_cast<int>(recs.size()));
        }

        logf("Stub-injected %zu forms for %s", injected, m.name.c_str());
        log_progress("Injecting modules",
            static_cast<int>(mi + 1),
            static_cast<int>(slot.modules.size()));
    }

    return true;
}
