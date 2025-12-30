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
#include <fstream>

// Injection subsystem context
static InjectionContext g_injectionContext;

void InitInjectionContext(
    const SlotDescriptor& slot,
    const std::vector<ModuleDescriptor>& modules)
{
    g_injectionContext.slot = &slot;
    g_injectionContext.modules = &modules;

    logf("Injection subsystem initialized: %zu modules, slot fileIndex=0x%02X",
        modules.size(), slot.fileIndex);
}

std::string ExplainFormIDRewrite(uint32_t originalFormID, uint32_t rewrittenFormID)
{
    if (originalFormID == rewrittenFormID)
        return "FormID was not rewritten (no mapping applied).";

    return "FormID was rewritten from " +
        std::to_string(originalFormID) + " to " +
        std::to_string(rewrittenFormID) +
        " based on module formIdMap mapping into the dummy slot.";
}

namespace
{
    static std::string to_hex(uint32_t value)
    {
        char buf[9];
        buf[0] = 0;
        std::sprintf(buf, "%08X", value);
        return std::string(buf);
    }

    struct DecodedFormID
    {
        uint32_t original;
        uint8_t pluginIndex;
        uint32_t localID;

        bool isESL;
        uint16_t eslSlot;

        bool isLocal;
    };

    static DecodedFormID DecodeFormID(uint32_t formID)
    {
        DecodedFormID out;
        out.original = formID;

        out.pluginIndex = uint8_t((formID >> 24) & 0xFF);
        out.localID = formID & 0x00FFFFFFu;

        out.isLocal = (out.pluginIndex == 0x00);
        out.isESL = false;
        out.eslSlot = 0;

        if (out.pluginIndex == 0xFE)
        {
            out.isESL = true;
            out.eslSlot = uint16_t((out.localID >> 12) & 0x0FFFu);
            out.localID = out.localID & 0x0FFFu;
        }

        return out;
    }

    static const ModuleDescriptor* FindModuleForDecodedID(const DecodedFormID& id)
    {
        if (!g_injectionContext.modules)
            return 0;

        const std::vector<ModuleDescriptor>& mods = *g_injectionContext.modules;

        for (std::size_t i = 0; i < mods.size(); ++i)
        {
            const ModuleDescriptor& m = mods[i];

            if (m.containsWorldspace)
                continue;

            if (m.isESL && id.isESL && id.pluginIndex == 0xFE && id.eslSlot == m.eslSlot)
                return &m;

            if (!m.isESL && !id.isESL && id.pluginIndex == m.originalFileIndex)
                return &m;
        }

        return 0;
    }

    static bool IsFormIDInSlot(const DecodedFormID& id)
    {
        if (!g_injectionContext.modules)
            return false;

        const std::vector<ModuleDescriptor>& mods = *g_injectionContext.modules;

        for (std::size_t i = 0; i < mods.size(); ++i)
        {
            const ModuleDescriptor& m = mods[i];

            if (m.containsWorldspace)
                continue;

            if (!m.isESL && !id.isESL && id.pluginIndex == m.originalFileIndex)
                return true;

            if (m.isESL && id.isESL && id.eslSlot == m.eslSlot)
                return true;
        }

        return false;
    }

    static void ReportMissingMapping(const ModuleDescriptor* mod, uint32_t localKey)
    {
        static std::unordered_set<unsigned long long> reported;

        unsigned long long key =
            (unsigned long long)(reinterpret_cast<std::uintptr_t>(mod)) << 32 |
            (unsigned long long)localKey;

        if (reported.find(key) != reported.end())
            return;

        reported.insert(key);

        logf("WARNING: Missing mapping for module=%s localKey=%06X",
            mod->name.c_str(), localKey);
    }

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

        if (!isESL)
        {
            if (hi == 0x00000000u)
            {
                std::unordered_map<std::uint32_t, std::uint32_t>::const_iterator it =
                    formIdMap.find(local);
                if (it != formIdMap.end())
                    return it->second;
            }
            return refFormID;
        }

        if (hi == 0x00000000u)
        {
            const std::uint32_t compact = local & 0x00000FFFu;
            std::unordered_map<std::uint32_t, std::uint32_t>::const_iterator it =
                formIdMap.find(compact);
            if (it != formIdMap.end())
                return it->second;
            return refFormID;
        }

        if (hi == 0xFE000000u)
        {
            const std::uint16_t slot = (std::uint16_t)((local >> 12) & 0x0FFFu);
            const std::uint32_t compact = local & 0x00000FFFu;

            if (slot == eslSlot)
            {
                std::unordered_map<std::uint32_t, std::uint32_t>::const_iterator it =
                    formIdMap.find(compact);
                if (it != formIdMap.end())
                    return it->second;
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

        for (std::size_t i = 0; i < payload.lvliEntries.size(); ++i)
        {
            const RecordPayload::LvliEntry& entry = payload.lvliEntries[i];
            std::uint32_t remapped = remap_lvli_ref(entry.formID, formIdMap, isESL, eslSlot);
            if (remapped != entry.formID && g_eslDebug)
            {
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
        for (std::size_t i = 0; i < slots.size(); ++i)
        {
            const CSVSlot& slot = slots[i];
            for (std::size_t j = 0; j < slot.sourceMods.size(); ++j)
            {
                const std::string& mod = slot.sourceMods[j];
                if (_stricmp(mod.c_str(), pluginName.c_str()) == 0)
                    return &slot;
            }
        }
        return 0;
    }
}

// Runtime FormID rewrite
uint32_t ResolveAndRewriteFormID(uint32_t formID)
{
    if (!g_enableRuntimeRewrite)
        return formID;

    if (!g_injectionContext.slot || !g_injectionContext.modules)
        return formID;

    DecodedFormID decoded = DecodeFormID(formID);

    if (!IsFormIDInSlot(decoded))
        return formID;

    const ModuleDescriptor* mod = FindModuleForDecodedID(decoded);
    if (!mod)
        return formID;

    uint32_t localKey = mod->isESL ?
        (decoded.localID & 0x00000FFFu) :
        (decoded.localID & 0x00FFFFFFu);

    std::unordered_map<std::uint32_t, std::uint32_t>::const_iterator it =
        mod->formIdMap.find(localKey);
    if (it == mod->formIdMap.end())
    {
        ReportMissingMapping(mod, localKey);
        return formID;
    }

    uint32_t targetFormID = it->second;

    if (g_eslDebug)
    {
        logf("Rewrite: module=%s original=%08X localKey=%06X target=%08X",
            mod->name.c_str(),
            formID,
            localKey,
            targetFormID);
    }

    return targetFormID;
}

// Mount BA2 archives
bool mount_archives(SlotDescriptor& slot)
{
    for (std::size_t i = 0; i < slot.modules.size(); ++i)
    {
        ModuleDescriptor& m = slot.modules[i];
        m.ba2Paths = discover_ba2s(m.name);
        for (std::size_t j = 0; j < m.ba2Paths.size(); ++j)
        {
            logf("Mount BA2 (stub, no actual mount): %s", m.ba2Paths[j].c_str());
        }
    }
    return true;
}

// Build form maps
bool build_form_maps(SlotDescriptor& slot)
{
    std::uint32_t subBase = 0x000100u;

    log_progress("Building form maps", 0, (int)slot.modules.size());

    for (std::size_t mi = 0; mi < slot.modules.size(); ++mi)
    {
        ModuleDescriptor& m = slot.modules[mi];

        if (m.containsWorldspace)
        {
            logf("Skipping form map build for '%s' (contains worldspace records).", m.name.c_str());
            log_progress("Building form maps", (int)(mi + 1), (int)slot.modules.size());
            continue;
        }

        std::vector<RawRecord> recs = scan_plugin_records(m.name, m);

        for (std::size_t ri = 0; ri < recs.size(); ++ri)
        {
            const RawRecord& r = recs[ri];

            const std::uint32_t localKey = m.isESL ?
                (r.localFormID & 0x00000FFFu) :
                (r.localFormID & 0x00FFFFFFu);

            const std::uint32_t target = compose_formid(slot.fileIndex, subBase + localKey);

            std::pair<std::unordered_map<std::uint32_t, std::uint32_t>::iterator, bool> ins =
                m.formIdMap.insert(std::make_pair(localKey, target));
            if (!ins.second && g_eslDebug)
            {
                logf("WARNING: Duplicate local key %06X in module '%s' (ESL=%s)",
                    localKey,
                    m.name.c_str(),
                    m.isESL ? "YES" : "NO");
            }
        }

        logf("Form map built for %s: %zu entries", m.name.c_str(), m.formIdMap.size());
        log_progress("Building form maps", (int)(mi + 1), (int)slot.modules.size());

        subBase += 0x000400u;
    }

    return true;
}

// Inject records
bool inject_records(
    const SlotDescriptor& slot,
    const std::vector<CSVSlot>& csvSlots)
{
    log_progress("Injecting modules", 0, (int)slot.modules.size());

    for (std::size_t mi = 0; mi < slot.modules.size(); ++mi)
    {
        const ModuleDescriptor& m = slot.modules[mi];

        if (m.containsWorldspace)
        {
            logf("Skipping injection for '%s' (contains worldspace records).", m.name.c_str());
            log_progress("Injecting modules", (int)(mi + 1), (int)slot.modules.size());
            continue;
        }

        const CSVSlot* csvSlot = find_slot_for_plugin(csvSlots, m.name);
        if (!csvSlot)
        {
            logf("WARNING: Plugin '%s' not found in CSV - skipping.", m.name.c_str());
            log_progress("Injecting modules", (int)(mi + 1), (int)slot.modules.size());
            continue;
        }

        std::vector<RawRecord> recs = scan_plugin_records(m.name, const_cast<ModuleDescriptor&>(m));
        if (recs.empty())
        {
            logf("No records to inject for %s", m.name.c_str());
            log_progress("Injecting modules", (int)(mi + 1), (int)slot.modules.size());
            continue;
        }

        logf("Injecting %zu records for %s (stub)", recs.size(), m.name.c_str());
        log_progress("Injecting " + m.name, 0, (int)recs.size());

        std::size_t injected = 0;

        for (std::size_t i = 0; i < recs.size(); ++i)
        {
            const RawRecord& r = recs[i];

            const std::uint32_t localKey = m.isESL ?
                (r.localFormID & 0x00000FFFu) :
                (r.localFormID & 0x00FFFFFFu);

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

            log_progress("Injecting " + m.name, (int)(i + 1), (int)recs.size());
        }

        logf("Stub-injected %zu forms for %s", injected, m.name.c_str());
        log_progress("Injecting modules", (int)(mi + 1), (int)slot.modules.size());
    }

    // SkippedModules.txt
    if (g_writeSkippedModules)
    {
        const char* outPath = "Data\\F4SE\\Plugins\\Multiplexer\\SkippedModules.txt";
        std::ofstream out(outPath, std::ios::out | std::ios::trunc);

        if (out.is_open())
        {
            out << "=== Multiplexer: Skipped Modules (Worldspace Detected) ===\n\n";

            bool any = false;
            for (std::size_t i = 0; i < slot.modules.size(); ++i)
            {
                const ModuleDescriptor& m = slot.modules[i];
                if (m.containsWorldspace)
                {
                    any = true;
                    out << m.name << "\n";
                }
            }

            if (!any)
                out << "(none)\n";

            out.close();
            logf("SkippedModules.txt written (%s)", any ? "entries present" : "no skipped modules");
        }
        else
        {
            logf("ERROR: Could not write SkippedModules.txt");
        }
    }

    return true;
}
