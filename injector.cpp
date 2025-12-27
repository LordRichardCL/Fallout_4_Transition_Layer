#include "pch.h"

#include "injector.hpp"
#include "scanner.hpp"
#include "mapping.hpp"
#include "log.hpp"
#include "csv_loader.hpp"
#include "config.hpp"
#include "records.hpp"
#include "diagnostics.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>


// ============================================================================
// Injection subsystem context and API (runtime redirection layer)
// ============================================================================

static InjectionContext g_injectionContext{};

void InitInjectionContext(
    const SlotDescriptor& slot,
    const std::vector<ModuleDescriptor>& modules)
{
    g_injectionContext.slot = &slot;
    g_injectionContext.modules = &modules;

    Diagnostics_RecordEvent(
        DiagnosticsEventType::Info,
        "Injection subsystem initialized: " +
        std::to_string(modules.size()) + " modules in slot fileIndex " +
        std::to_string(slot.fileIndex)
    );

    logf("Injection subsystem initialized: %zu modules, slot fileIndex=0x%02X",
        modules.size(), slot.fileIndex);
}

std::string ExplainFormIDRewrite(uint32_t originalFormID, uint32_t rewrittenFormID)
{
    if (originalFormID == rewrittenFormID) {
        return "FormID was not rewritten (no mapping applied).";
    }

    return "FormID was rewritten from " +
        std::to_string(originalFormID) + " to " +
        std::to_string(rewrittenFormID) +
        " based on module formIdMap mapping into the dummy slot.";
}
namespace
{
    // Small helper for hex formatting in diagnostics/logs.
    static std::string to_hex(uint32_t value)
    {
        char buf[9]{};
        sprintf_s(buf, "%08X", value);
        return std::string(buf);
    }

    // ========================================================================
    // FormID decoding
    // ========================================================================

    struct DecodedFormID
    {
        uint32_t original = 0;
        uint8_t pluginIndex = 0;
        uint32_t localID = 0;

        bool isESL = false;
        uint16_t eslSlot = 0;

        bool isLocal = false;
    };

    static DecodedFormID DecodeFormID(uint32_t formID)
    {
        DecodedFormID out;
        out.original = formID;

        out.pluginIndex = (formID >> 24) & 0xFF;
        out.localID = formID & 0x00FFFFFFu;

        if (out.pluginIndex == 0x00) {
            out.isLocal = true;
            return out;
        }

        if (out.pluginIndex == 0xFE) {
            out.isESL = true;
            out.eslSlot = static_cast<uint16_t>((out.localID >> 12) & 0x0FFFu);
            out.localID = out.localID & 0x0FFFu;
            return out;
        }

        return out;
    }

    // ========================================================================
    // Module lookup
    // ========================================================================

    static const ModuleDescriptor* FindModuleForDecodedID(const DecodedFormID& id)
    {
        if (!g_injectionContext.modules)
            return nullptr;

        for (const auto& m : *g_injectionContext.modules)
        {
            if (m.isESL && id.isESL && id.pluginIndex == 0xFE && id.eslSlot == m.eslSlot)
                return &m;

            if (!m.isESL && !id.isESL && id.pluginIndex == m.originalFileIndex)
                return &m;
        }

        return nullptr;
    }

    // ========================================================================
    // SAFETY LAYER — Whitelist: Only rewrite FormIDs belonging to this slot
    // ========================================================================

    static bool IsFormIDInSlot(const DecodedFormID& id)
    {
        if (!g_injectionContext.modules)
            return false;

        for (const auto& m : *g_injectionContext.modules)
        {
            if (!m.isESL && !id.isESL && id.pluginIndex == m.originalFileIndex)
                return true;

            if (m.isESL && id.isESL && id.eslSlot == m.eslSlot)
                return true;
        }

        return false;
    }

    // ========================================================================
    // SAFETY LAYER — Missing mapping detector
    // ========================================================================

    static void ReportMissingMapping(const ModuleDescriptor* mod, uint32_t localKey)
    {
        static std::unordered_set<uint64_t> reported;

        uint64_t key = (uint64_t(mod) << 32) | localKey;
        if (reported.count(key))
            return;

        reported.insert(key);

        Diagnostics_RecordEvent(
            DiagnosticsEventType::Warning,
            "Missing mapping: module=" + mod->name +
            ", localKey=0x" + to_hex(localKey)
        );

        logf("WARNING: Missing mapping for module=%s localKey=%06X",
            mod->name.c_str(), localKey);
    }

    // ========================================================================
    // Existing helpers (unchanged)
    // ========================================================================

    inline std::uint32_t compose_formid(std::uint32_t virtualID, std::uint32_t local)
    {
        return (virtualID << 24) | (local & 0x00FFFFFFu);
    }

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
} // end anonymous namespace
uint32_t ResolveAndRewriteFormID(uint32_t formID)
{
    // Toggle: allow disabling rewrite entirely
    if (!g_enableRuntimeRewrite)
        return formID;

    if (!g_injectionContext.slot || !g_injectionContext.modules)
        return formID;

    DecodedFormID decoded = DecodeFormID(formID);

    // Whitelist: only rewrite FormIDs belonging to this slot
    if (!IsFormIDInSlot(decoded))
        return formID;

    const ModuleDescriptor* mod = FindModuleForDecodedID(decoded);
    if (!mod)
        return formID;

    uint32_t localKey = mod->isESL
        ? (decoded.localID & 0x00000FFFu)
        : (decoded.localID & 0x00FFFFFFu);

    auto it = mod->formIdMap.find(localKey);
    if (it == mod->formIdMap.end()) {
        ReportMissingMapping(mod, localKey);
        return formID;
    }

    uint32_t targetFormID = it->second;

    Diagnostics_RecordEvent(
        DiagnosticsEventType::Info,
        "Rewrite: " + mod->name +
        " 0x" + to_hex(formID) +
        " -> 0x" + to_hex(targetFormID)
    );

    if (g_eslDebug) {
        logf("Rewrite: module=%s original=%08X localKey=%06X target=%08X",
            mod->name.c_str(),
            formID,
            localKey,
            targetFormID);
    }

    return targetFormID;
}
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
