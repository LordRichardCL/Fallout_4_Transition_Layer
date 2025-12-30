#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <cstdint>

#define NOMINMAX
#include <windows.h>

#include <regex>
#include <unordered_set>
#include <unordered_map>
#include <map>
#include <algorithm>
#include <cmath>

// ------------------------------------------------------------
// Helpers: string utilities
// ------------------------------------------------------------
static inline std::string Trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

static inline std::string ToLower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
        [](unsigned char c) { return static_cast<char>(::tolower(c)); });
    return out;
}

// ------------------------------------------------------------
// Helper: sanitize strings for safe cfg/CSV output
// ------------------------------------------------------------
static inline std::string Sanitize(const std::string& input) {
    std::string out;
    out.reserve(input.size());

    for (unsigned char c : input) {
        if (c == '\0') continue;
        if (c < 32 && c != '\t' && c != '\n') continue;
        if (c > 126) {
            out.push_back('_');
            continue;
        }
        out.push_back(c);
    }

    return out;
}

// ------------------------------------------------------------
// Helper: CSV escape (double quotes)
// ------------------------------------------------------------
static inline std::string CsvEscape(const std::string& s) {
    std::string out = s;
    size_t pos = 0;
    while ((pos = out.find('"', pos)) != std::string::npos) {
        out.insert(pos, "\"");
        pos += 2;
    }
    return out;
}

// ------------------------------------------------------------
// Config / INI
// ------------------------------------------------------------
struct Config {
    int  groupSize = 0;
    bool ignoreDisabled = true;
    bool strictValidation = true;
    bool logDetails = true;
    int  maxSlots = 20; // hard cap: 20 dummy plugins
};

Config LoadConfig(std::ofstream& log) {
    Config cfg;
    std::ifstream ini("csvbuilder.ini");
    if (!ini) {
        log << "INI: csvbuilder.ini not found. Using defaults.\n";
        return cfg;
    }

    log << "INI: Loading csvbuilder.ini\n";

    std::string line;
    while (std::getline(ini, line)) {
        line = Trim(line);

        if (line.empty() || line[0] == ';' || line[0] == '#')
            continue;
        if (line[0] == '[')
            continue;

        auto pos = line.find('=');
        if (pos == std::string::npos)
            continue;

        std::string key = ToLower(Trim(line.substr(0, pos)));
        std::string value = Trim(line.substr(pos + 1));

        if (key == "groupsize") {
            cfg.groupSize = std::stoi(value);
        }
        else if (key == "ignoredisabled") {
            cfg.ignoreDisabled = (value == "1" || ToLower(value) == "true");
        }
        else if (key == "strictvalidation") {
            cfg.strictValidation = (value == "1" || ToLower(value) == "true");
        }
        else if (key == "logdetails") {
            cfg.logDetails = (value == "1" || ToLower(value) == "true");
        }
        else if (key == "maxslots") {
            cfg.maxSlots = std::stoi(value);
        }
    }

    if (cfg.maxSlots <= 0 || cfg.maxSlots > 20)
        cfg.maxSlots = 20;

    log << "INI: Config loaded - GroupSize=" << cfg.groupSize
        << ", IgnoreDisabled=" << (cfg.ignoreDisabled ? "YES" : "NO")
        << ", StrictValidation=" << (cfg.strictValidation ? "YES" : "NO")
        << ", LogDetails=" << (cfg.logDetails ? "YES" : "NO")
        << ", MaxSlots=" << cfg.maxSlots
        << "\n";

    return cfg;
}

// ------------------------------------------------------------
// Helper: Read registry string
// ------------------------------------------------------------
std::string ReadRegistryString(HKEY root, const char* path, const char* key) {
    HKEY hKey;
    if (RegOpenKeyExA(root, path, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return "";

    char value[1024];
    DWORD value_length = sizeof(value);
    DWORD type = 0;

    if (RegQueryValueExA(hKey, key, nullptr, &type, (LPBYTE)value, &value_length) != ERROR_SUCCESS ||
        type != REG_SZ) {
        RegCloseKey(hKey);
        return "";
    }

    RegCloseKey(hKey);
    return std::string(value);
}

// ------------------------------------------------------------
// Helper: Parse Steam libraryfolders.vdf
// ------------------------------------------------------------
std::string FindFallout4InLibraries(const std::string& steamRoot, std::ofstream& log) {
    std::filesystem::path vdfPath = std::filesystem::path(steamRoot) / "steamapps" / "libraryfolders.vdf";

    if (!std::filesystem::exists(vdfPath)) {
        log << "FindFallout4InLibraries: " << vdfPath.string() << " does not exist.\n";
        return "";
    }

    std::ifstream file(vdfPath);
    if (!file) {
        log << "FindFallout4InLibraries: Failed to open " << vdfPath.string() << "\n";
        return "";
    }

    std::string line;
    std::regex pathRegex("\"path\"\\s+\"([^\"]+)\"");
    std::smatch match;

    while (std::getline(file, line)) {
        if (std::regex_search(line, match, pathRegex)) {
            std::string libraryPath = match[1].str();
            std::filesystem::path appManifest =
                std::filesystem::path(libraryPath) / "steamapps" / "appmanifest_377160.acf";

            if (std::filesystem::exists(appManifest)) {
                log << "FindFallout4InLibraries: Found appmanifest_377160.acf in " << libraryPath << "\n";
                return (std::filesystem::path(libraryPath) / "steamapps" / "common" / "Fallout 4").string();
            }
        }
    }

    log << "FindFallout4InLibraries: Fallout 4 not found in libraryfolders.vdf\n";
    return "";
}

// ------------------------------------------------------------
// Ignore list
// ------------------------------------------------------------
static const std::unordered_set<std::string> IGNORE_LIST = {
    "ccbgsfo4098-as_pickman.esl",
    "ccbgsfo4099-as_reillysrangers.esl",
    "ccbgsfo4101-as_shi.esl",
    "ccbgsfo4103-as_tunnelsnakes.esl",
    "ccbgsfo4104-ws_bats.esl",
    "ccbgsfo4105-ws_camoblue.esl",
    "ccbgsfo4106-ws_camogreen.esl",
    "ccbgsfo4107-ws_camotan.esl",
    "ccbgsfo4108-ws_childrenofatom.esl",
    "ccbgsfo4110-ws_enclave.esl",
    "ccbgsfo4111-ws_jack-olantern.esl",
    "ccbgsfo4112-ws_pickman.esl",
    "ccbgsfo4113-ws_reillysrangers.esl",
    "ccbgsfo4114-ws_shi.esl",
    "ccbgsfo4115-x02.esl",
    "ccbgsfo4116-heavyflamer.esl",
    "ccbgsfo4117-capmerc.esl",
    "ccbgsfo4118-ws_tunnelsnakes.esl",
    "ccbgsfo4119-cyberdog.esl",
    "ccbgsfo4120-poweramorskin(pittraider).esl",
    "ccbgsfo4121-poweramorskin(airforce).esl",
    "ccbgsfo4122-poweramorskin(scorchedsierra).esl",
    "ccbgsfo4123-poweramorskin(inferno).esl",
    "ccbgsfo4124-poweramorskin(tribalhelmets).esl",
    "cccrsfo4001-pipcoa.esl",
    "cceejfo4001-decorationpack.esl",
    "cceejfo4002-nuka.esl",
    "ccfrsfo4001-handmadeshotgun.esl",
    "ccfrsfo4002-antimaterielrifle.esl",
    "ccfrsfo4003-cr75l.esl",
    "ccfsvfo4001-modularmilitarybackpack.esl",
    "ccfsvfo4002-midcenturymodern.esl",
    "ccfsvfo4003-slocum.esl",
    "ccfsvfo4007-halloween.esl",
    "ccgcafo4001-factionws01army.esl",
    "ccgcafo4002-factionws02acat.esl",
    "ccgcafo4003-factionws03bos.esl",
    "ccgcafo4004-factionws04gun.esl",
    "ccgcafo4005-factionws05hrpink.esl",
    "ccgcafo4006-factionws06hrshark.esl",
    "ccgcafo4007-factionws07hrflames.esl",
    "ccgcafo4008-factionws08inst.esl",
    "ccgcafo4009-factionws09mm.esl",
    "ccgcafo4010-factionws10rr.esl",
    "ccgcafo4011-factionws11vt.esl",
    "ccgcafo4012-factionas01acat.esl",
    "ccgcafo4013-factionas02bos.esl",
    "ccgcafo4014-factionas03gun.esl",
    "ccgcafo4015-factionas04hrpink.esl",
    "ccgcafo4016-factionas05hrshark.esl",
    "ccgcafo4017-factionas06inst.esl",
    "ccgcafo4018-factionas07mm.esl",
    "ccgcafo4019-factionas08nuk.esl",
    "ccgcafo4020-factionas09rr.esl",
    "ccgcafo4021-factionas10hrflames.esl",
    "ccgcafo4022-factionas11vt.esl",
    "ccgcafo4023-factionas12army.esl",
    "ccgcafo4024-instituteplasmaweapons.esl",
    "ccgcafo4025-pagunmm.esl",
    "ccgrcfo4001-pipgreytort.esl",
    "ccgrcfo4002-pipgreenvim.esl",
    "ccjvdfo4001-holiday.esl",
    "cckgjfo4001-bastion.esl",
    "ccotmfo4001-remnants.esl",
    "ccqdrfo4001_powerarmorai.esl",
    "ccrpsfo4001-scavenger.esl",
    "ccrzrfo4002-disintegrate.esl",
    "ccrzrfo4003-pipover.esl",
    "ccrzrfo4004-pipinst.esl",
    "ccsbjfo4001-solarflare.esl",
    "ccsbjfo4002_manwellrifle.esl",
    "ccsbjfo4003-grenade.esl",
    "ccsbjfo4004-ion.esl",
    "ccswkfo4002-pipnuka.esl",
    "ccswkfo4003-pipquan.esl",
    "ccygpfo4001-pipcruiser.esl",
    "ccrzrfo4001-tunnelsnakes.esm",
    "ccswkfo4001-astronautpowerarmor.esm",
    "cctosfo4001-neosky.esm",
    "cctosfo4002_neonflats.esm",
    "ccvltfo4001-homes.esm",
    "cczsef04001-bhouse.esm",
    "cczsefo4002-smanor.esm",
    "dlccoast.esm",
    "dlcnukaworld.esm",
    "dlcrobot.esm",
    "dlcworkshop01.esm",
    "dlcworkshop02.esm",
    "dlcworkshop03.esm",
    "fallout4.esm",
    "vchgs001fo4_ncrbeasthunter.esm",
    "bgs_varmintrifle.esp",
    "CCMerged.esl",
    "CCMerged_Sounds.esl",
    "CCMerged_Textures1.esl",
    "CCMerged_Textures2.esl"
};

// ------------------------------------------------------------
// Validation helper
// ------------------------------------------------------------
bool FailOrWarn(bool strict, const std::string& msg, std::ofstream& log) {
    log << "VALIDATION: " << msg << "\n";
    std::cerr << msg << "\n";
    return strict;
}

// ------------------------------------------------------------
// Protected plugins (protected_plugins.json)
// Very simple JSON: ["Plugin1.esp","Plugin2.esl", ...]
// ------------------------------------------------------------
std::unordered_set<std::string> LoadProtectedPlugins(const std::filesystem::path& jsonPath, std::ofstream& log) {
    std::unordered_set<std::string> protectedSet;

    std::ifstream in(jsonPath);
    if (!in) {
        log << "PROTECTED: No protected_plugins.json found at " << jsonPath.string() << " (optional).\n";
        return protectedSet;
    }

    log << "PROTECTED: Loading protected_plugins.json from " << jsonPath.string() << "\n";

    std::string content((std::istreambuf_iterator<char>(in)),
        std::istreambuf_iterator<char>());

    // Very naive JSON string extractor: finds all "..." sequences
    std::regex strRegex("\"([^\"]+)\"");
    std::smatch match;
    std::string::const_iterator searchStart(content.cbegin());

    while (std::regex_search(searchStart, content.cend(), match, strRegex)) {
        std::string plugin = Trim(match[1].str());
        std::string lower = ToLower(plugin);
        if (!lower.empty()) {
            protectedSet.insert(lower);
            log << "PROTECTED: Registered protected plugin '" << lower << "'\n";
        }
        searchStart = match.suffix().first;
    }

    log << "PROTECTED: Total protected plugins: " << protectedSet.size() << "\n";
    return protectedSet;
}
// ------------------------------------------------------------
// Category enum for plugins
// ------------------------------------------------------------
enum class PluginCategory {
    None,
    Weapons,
    Armor,
    Keywords,
    LeveledLists,
    Worldspace // used only for detection/skip
};

static inline const char* CategoryToString(PluginCategory cat) {
    switch (cat) {
    case PluginCategory::Weapons:       return "Weapons";
    case PluginCategory::Armor:         return "Armor";
    case PluginCategory::Keywords:      return "Keywords";
    case PluginCategory::LeveledLists:  return "LeveledLists";
    case PluginCategory::Worldspace:    return "Worldspace";
    default:                            return "None";
    }
}

// ------------------------------------------------------------
// Worldspace diagnostics structure
// ------------------------------------------------------------
struct WorldspaceHit {
    std::string pluginName;
    std::filesystem::path fullPath;
    std::vector<std::string> signatures;
};

// ------------------------------------------------------------
// Mixed record diagnostics structure
// ------------------------------------------------------------
struct MixedHit {
    std::string pluginName;
    std::vector<std::string> categories;
    PluginCategory primaryCategory;
};

// ------------------------------------------------------------
// Per-plugin analysis result
// ------------------------------------------------------------
struct PluginAnalysis {
    bool touchesWorldspace = false;
    bool hasWeapons = false;
    bool hasArmor = false;
    bool hasKeywords = false;
    bool hasLeveledLists = false;

    std::vector<std::string> worldspaceSigs;
};

// ------------------------------------------------------------
// Extract all safe categories touched by a plugin
// ------------------------------------------------------------
std::vector<std::string> GetAllCategories(const PluginAnalysis& a) {
    std::vector<std::string> out;
    if (a.hasWeapons)       out.push_back("Weapons");
    if (a.hasArmor)         out.push_back("Armor");
    if (a.hasKeywords)      out.push_back("Keywords");
    if (a.hasLeveledLists)  out.push_back("LeveledLists");
    return out;
}

// ------------------------------------------------------------
// ESP/ESM record scanning
// Very lightweight: walks GRUPs and REFR-like records,
// looking only at top-level signatures.
// ------------------------------------------------------------
#pragma pack(push, 1)
struct RecordHeader {
    char     sig[4];      // e.g. 'WEAP', 'ARMO', 'WRLD', 'GRUP'
    std::uint32_t dataSize;
    std::uint32_t flags;
    std::uint32_t formID;
    std::uint32_t vcInfo;
    std::uint16_t formVersion;
    std::uint16_t unknown;
};
#pragma pack(pop)

static inline bool IsWorldspaceSignature(const char sig[4]) {
    return
        (sig[0] == 'W' && sig[1] == 'R' && sig[2] == 'L' && sig[3] == 'D') || // WRLD
        (sig[0] == 'C' && sig[1] == 'E' && sig[2] == 'L' && sig[3] == 'L') || // CELL
        (sig[0] == 'L' && sig[1] == 'A' && sig[2] == 'N' && sig[3] == 'D') || // LAND
        (sig[0] == 'N' && sig[1] == 'A' && sig[2] == 'V' && sig[3] == 'M') || // NAVM
        (sig[0] == 'R' && sig[1] == 'E' && sig[2] == 'F' && sig[3] == 'R') || // REFR
        (sig[0] == 'A' && sig[1] == 'C' && sig[2] == 'H' && sig[3] == 'R');   // ACHR
}

static inline bool IsSignature(const char sig[4], const char* tag) {
    return sig[0] == tag[0] && sig[1] == tag[1] && sig[2] == tag[2] && sig[3] == tag[3];
}

PluginAnalysis AnalyzePluginRecords(const std::filesystem::path& pluginPath, std::ofstream& log) {
    PluginAnalysis result;

    std::ifstream file(pluginPath, std::ios::binary);
    if (!file) {
        log << "ANALYZE: Could not open " << pluginPath.string() << "\n";
        return result;
    }

    std::vector<char> data((std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());

    if (data.size() < sizeof(RecordHeader)) {
        log << "ANALYZE: File too small to contain records: " << pluginPath.filename().string() << "\n";
        return result;
    }

    size_t offset = 0;
    size_t fileSize = data.size();

    while (offset + sizeof(RecordHeader) <= fileSize) {
        RecordHeader* rec = reinterpret_cast<RecordHeader*>(&data[offset]);

        char sig[4] = { rec->sig[0], rec->sig[1], rec->sig[2], rec->sig[3] };

        // GRUP: skip entire group length
        if (IsSignature(sig, "GRUP")) {
            if (offset + 8 > fileSize) break;
            std::uint32_t groupSize = *reinterpret_cast<std::uint32_t*>(&data[offset + 4]);
            if (groupSize == 0 || offset + groupSize > fileSize) {
                log << "ANALYZE: Invalid GRUP size in " << pluginPath.filename().string() << "\n";
                break;
            }
            offset += groupSize;
            continue;
        }

        // Regular record
        std::uint32_t dataSize = rec->dataSize;
        std::size_t recordSize = sizeof(RecordHeader) + dataSize;

        if (recordSize == 0 || offset + recordSize > fileSize) {
            log << "ANALYZE: Invalid record size in " << pluginPath.filename().string() << "\n";
            break;
        }

        // Category detection
        if (IsWorldspaceSignature(sig)) {
            result.touchesWorldspace = true;
            result.worldspaceSigs.emplace_back(std::string(sig, sig + 4));
        }
        else if (IsSignature(sig, "WEAP")) {
            result.hasWeapons = true;
        }
        else if (IsSignature(sig, "ARMO")) {
            result.hasArmor = true;
        }
        else if (IsSignature(sig, "KYWD")) {
            result.hasKeywords = true;
        }
        else if (IsSignature(sig, "LVLI")) {
            result.hasLeveledLists = true;
        }

        offset += recordSize;

        // Early exit if everything is detected
        if (result.touchesWorldspace &&
            result.hasWeapons &&
            result.hasArmor &&
            result.hasKeywords &&
            result.hasLeveledLists) {
            break;
        }
    }

    return result;
}

// ------------------------------------------------------------
// Determine primary category for routing
// Priority: Weapons > Armor > Keywords > LeveledLists
// ------------------------------------------------------------
PluginCategory DeterminePrimaryCategory(const PluginAnalysis& a) {
    if (a.touchesWorldspace)            return PluginCategory::Worldspace;
    if (a.hasWeapons)                   return PluginCategory::Weapons;
    if (a.hasArmor)                     return PluginCategory::Armor;
    if (a.hasKeywords)                  return PluginCategory::Keywords;
    if (a.hasLeveledLists)              return PluginCategory::LeveledLists;
    return PluginCategory::None;
}

// ------------------------------------------------------------
// Dummy slot architecture: 20 fixed dummy ESPs with categories
// ------------------------------------------------------------
struct DummySlotDef {
    std::string name;
    int virtualID;
    PluginCategory category;
};

static const std::vector<DummySlotDef> DUMMY_SLOTS = {
    // Weapons (1001–1005)
    { "Dummy_Weapons_01.esp",      1001, PluginCategory::Weapons },
    { "Dummy_Weapons_02.esp",      1002, PluginCategory::Weapons },
    { "Dummy_Weapons_03.esp",      1003, PluginCategory::Weapons },
    { "Dummy_Weapons_04.esp",      1004, PluginCategory::Weapons },
    { "Dummy_Weapons_05.esp",      1005, PluginCategory::Weapons },

    // Armor (2001–2005)
    { "Dummy_Armor_01.esp",        2001, PluginCategory::Armor },
    { "Dummy_Armor_02.esp",        2002, PluginCategory::Armor },
    { "Dummy_Armor_03.esp",        2003, PluginCategory::Armor },
    { "Dummy_Armor_04.esp",        2004, PluginCategory::Armor },
    { "Dummy_Armor_05.esp",        2005, PluginCategory::Armor },

    // Keywords (3001–3005)
    { "Dummy_Keywords_01.esp",     3001, PluginCategory::Keywords },
    { "Dummy_Keywords_02.esp",     3002, PluginCategory::Keywords },
    { "Dummy_Keywords_03.esp",     3003, PluginCategory::Keywords },
    { "Dummy_Keywords_04.esp",     3004, PluginCategory::Keywords },
    { "Dummy_Keywords_05.esp",     3005, PluginCategory::Keywords },

    // Leveled Lists (4001–4005)
    { "Dummy_LeveledLists_01.esp", 4001, PluginCategory::LeveledLists },
    { "Dummy_LeveledLists_02.esp", 4002, PluginCategory::LeveledLists },
    { "Dummy_LeveledLists_03.esp", 4003, PluginCategory::LeveledLists },
    { "Dummy_LeveledLists_04.esp", 4004, PluginCategory::LeveledLists },
    { "Dummy_LeveledLists_05.esp", 4005, PluginCategory::LeveledLists }
};
// ------------------------------------------------------------
// main
// ------------------------------------------------------------
int main() {
    std::ofstream log("csvbuilder.log", std::ios::app);
    log << "\n=== CSV Builder Started ===\n";

    Config cfg = LoadConfig(log);

    std::vector<std::string> worldspaceSkipped;
    std::vector<WorldspaceHit> worldspaceHits;
    std::vector<MixedHit> mixedHits;

    //
    // STEP 1 — Locate Fallout 4 installation
    //
    std::cout << "Locating Fallout 4 installation...\n";
    log << "Locating Fallout 4 installation...\n";

    std::string steamPath = ReadRegistryString(
        HKEY_LOCAL_MACHINE,
        "SOFTWARE\\WOW6432Node\\Valve\\Steam",
        "InstallPath"
    );

    if (steamPath.empty()) {
        std::string msg = "ERROR: Could not read Steam InstallPath from registry.";
        log << msg << "\n";
        std::cerr << msg << "\nPress Enter to exit...";
        std::cin.get();
        return 1;
    }

    std::filesystem::path defaultFO4 =
        std::filesystem::path(steamPath) / "steamapps" / "common" / "Fallout 4";

    std::string falloutPath;

    if (std::filesystem::exists(defaultFO4 / "Fallout4.exe")) {
        falloutPath = defaultFO4.string();
        std::cout << "Found Fallout 4 in default Steam library.\n";
        log << "Found Fallout 4 in default Steam library: " << falloutPath << "\n";
    }
    else {
        falloutPath = FindFallout4InLibraries(steamPath, log);
        log << "DEBUG: Fallout 4 path resolved to: " << falloutPath << "\n";

        if (falloutPath.empty()) {
            std::string msg = "ERROR: Could not locate Fallout 4 installation.";
            log << msg << "\n";
            std::cerr << msg << "\nPress Enter to exit...";
            std::cin.get();
            return 1;
        }

        std::cout << "Found Fallout 4 in secondary Steam library.\n";
        log << "Found Fallout 4 in secondary Steam library: " << falloutPath << "\n";
    }

    //
    // STEP 2 — Build plugin folder path + load protected_plugins.json
    //
    std::filesystem::path pluginPath =
        std::filesystem::path(falloutPath) / "Data" / "F4SE" / "Plugins" / "Multiplexer";

    try {
        std::filesystem::create_directories(pluginPath);
    }
    catch (const std::exception& e) {
        std::string msg = std::string("ERROR: Failed to create Multiplexer folder: ") + e.what();
        log << msg << "\n";
        std::cerr << msg << "\nPress Enter to exit...";
        std::cin.get();
        return 1;
    }

    std::string outputCSV = (pluginPath / "loadorder_mapped_filtered_clean.csv").string();
    std::string outputSlotCfg = (pluginPath / "slot.cfg").string();
    std::string worldspaceListPath = (pluginPath / "worldspace_skipped.txt").string();
    std::string worldspaceDiagPath = (pluginPath / "worldspace_diagnostics.txt").string();
    std::string mixedDiagPath = (pluginPath / "mixed_records.txt").string();
    std::filesystem::path protectedJsonPath = pluginPath / "protected_plugins.json";

    std::cout << "Output directory: " << pluginPath.string() << "\n";
    log << "Output directory: " << pluginPath.string() << "\n";

    // Protected plugins
    auto protectedPlugins = LoadProtectedPlugins(protectedJsonPath, log);

    //
    // STEP 3 — Read loadorder.txt and analyze plugins
    //
    std::string input = "loadorder.txt";
    std::ifstream in(input);

    if (!in) {
        std::string msg = "ERROR: Cannot open loadorder.txt (place it next to csvbuilder.exe)";
        log << msg << "\n";
        std::cerr << msg << "\nPress Enter to exit...";
        std::cin.get();
        return 1;
    }

    struct PluginEntry {
        std::string name;
        PluginAnalysis analysis;
        PluginCategory primaryCategory = PluginCategory::None;
        bool isProtected = false;
    };

    std::vector<PluginEntry> includedPlugins;
    std::unordered_set<std::string> seenPlugins;
    bool hasDuplicates = false;

    std::string lineRaw;
    while (std::getline(in, lineRaw)) {

        std::string line = Trim(lineRaw);
        line = Sanitize(line);

        if (line.empty())
            continue;

        if (cfg.ignoreDisabled && line[0] == '#')
            continue;

        if (line[0] == '*')
            line = line.substr(1);

        line = Trim(line);
        line = Sanitize(line);

        if (line.empty())
            continue;

        // Skip dummy plugins (our own)
        if (line.rfind("Dummy_", 0) == 0)
            continue;

        std::string lower = ToLower(line);

        // Ignore list (case-insensitive)
        if (IGNORE_LIST.count(lower) > 0) {
            if (cfg.logDetails) {
                log << "IGNORE_LIST: Skipping " << line << "\n";
            }
            continue;
        }

        // Duplicate detection
        if (!seenPlugins.insert(lower).second) {
            hasDuplicates = true;
            std::string msg = "WARNING: Duplicate plugin (after normalization/sanitization) in loadorder.txt: " + line;
            log << msg << "\n";
            std::cerr << msg << "\n";
            continue;
        }

        // Protected plugins: tracked but never multiplexed
        bool isProtected = (protectedPlugins.count(lower) > 0);
        if (isProtected && cfg.logDetails) {
            log << "PROTECTED: " << line << " is protected and will not be multiplexed.\n";
        }

        // Analyze records
        std::filesystem::path pluginFile = std::filesystem::path(falloutPath) / "Data" / line;
        PluginAnalysis analysis = AnalyzePluginRecords(pluginFile, log);
        PluginCategory cat = DeterminePrimaryCategory(analysis);

        // WORLDSPACE DETECTION
        if (analysis.touchesWorldspace) {
            WorldspaceHit hit;
            hit.pluginName = line;
            hit.fullPath = pluginFile;
            hit.signatures = analysis.worldspaceSigs;

            worldspaceSkipped.push_back(line);
            worldspaceHits.push_back(hit);

            log << "WORLDSPACE: Skipping " << line << " due to signatures: ";
            for (auto& s : analysis.worldspaceSigs) log << s << " ";
            log << "\n";

            continue; // never multiplex worldspace
        }

        // MIXED RECORD DETECTION
        std::vector<std::string> allCats = GetAllCategories(analysis);
        if (allCats.size() > 1) {
            MixedHit mh;
            mh.pluginName = line;
            mh.categories = allCats;
            mh.primaryCategory = cat;
            mixedHits.push_back(mh);

            log << "MIXED: " << line << " touches multiple categories: ";
            for (auto& c : allCats) log << c << " ";
            log << "(primary=" << CategoryToString(cat) << ")\n";
        }

        // Add plugin entry
        PluginEntry entry;
        entry.name = line;
        entry.analysis = analysis;
        entry.primaryCategory = cat;
        entry.isProtected = isProtected;

        includedPlugins.push_back(entry);
    }

    //
    // STEP 3B — Write worldspace diagnostics
    //
    if (!worldspaceSkipped.empty()) {
        std::ofstream ws(worldspaceListPath);
        for (const auto& p : worldspaceSkipped) {
            ws << p << "\n";
        }

        std::cout << "\nThe following plugins contain worldspace records and were skipped:\n";
        for (const auto& p : worldspaceSkipped) {
            std::cout << "  - " << p << "\n";
        }
        std::cout << "See worldspace_skipped.txt and worldspace_diagnostics.txt in the Multiplexer folder.\n\n";
    }

    if (!worldspaceHits.empty()) {
        std::ofstream diag(worldspaceDiagPath);

        diag << "=== WORLDSPACE PLUGIN DIAGNOSTICS ===\n";
        diag << "These plugins contain worldspace/cell/navmesh records and\n";
        diag << "must remain enabled in your normal load order.\n";
        diag << "They cannot be multiplexed safely.\n\n";

        for (const auto& hit : worldspaceHits) {
            diag << "Plugin: " << hit.pluginName << "\n";
            diag << "Path:   " << hit.fullPath.string() << "\n";
            diag << "Detected worldspace signatures:\n";

            for (const auto& sig : hit.signatures) {
                diag << "    - " << sig << "\n";
            }

            diag << "\nReason:\n";
            diag << "    This plugin modifies worldspace/cell/navmesh data.\n";
            diag << "    These records cannot be safely multiplexed.\n";
            diag << "    You must enable this plugin normally in your load order.\n";
            diag << "------------------------------------------------------------\n\n";
        }
    }

    //
    // STEP 3C — Write mixed record diagnostics
    //
    if (!mixedHits.empty()) {
        std::ofstream mix(mixedDiagPath);

        mix << "=== MIXED RECORD DIAGNOSTICS ===\n";
        mix << "These plugins contain multiple safe record categories.\n";
        mix << "They were still routed into their primary category.\n\n";

        for (const auto& mh : mixedHits) {
            mix << "Plugin: " << mh.pluginName << "\n";
            mix << "Detected categories:\n";
            for (const auto& c : mh.categories) {
                mix << "    - " << c << "\n";
            }
            mix << "Primary category used: " << CategoryToString(mh.primaryCategory) << "\n";
            mix << "------------------------------------------------------------\n\n";
        }
    }

    //
    // Abort if nothing left to multiplex
    //
    if (includedPlugins.empty()) {
        std::string msg = "ERROR: No plugins found after filtering and worldspace exclusion. Nothing to do.";
        log << msg << "\n";
        std::cerr << msg << "\nPress Enter to exit...";
        std::cin.get();
        return 1;
    }

    std::cout << "Loaded " << includedPlugins.size() << " plugins for multiplexing.\n";
    log << "Loaded " << includedPlugins.size() << " plugins for multiplexing.\n";

    if (hasDuplicates && FailOrWarn(cfg.strictValidation,
        "Duplicate plugins detected after sanitization/normalization. Resolve in loadorder.txt or disable StrictValidation.",
        log)) {
        std::cerr << "StrictValidation=1, aborting due to duplicate plugins.\nPress Enter to exit...";
        std::cin.get();
        return 1;
    }
    //
        // STEP 4 — Determine grouping per category with fixed maxSlots = 20
        //
    int groupSize = cfg.groupSize;
    const int maxSlots = cfg.maxSlots; // already clamped to <= 20

    // Partition plugins by primary category (excluding protected + None)
    std::vector<std::string> weaponsMods;
    std::vector<std::string> armorMods;
    std::vector<std::string> keywordMods;
    std::vector<std::string> leveledMods;
    std::vector<std::string> protectedOnly; // protected but non-worldspace, non-multiplexed

    for (const auto& p : includedPlugins) {
        if (p.isProtected) {
            protectedOnly.push_back(p.name);
            continue;
        }

        switch (p.primaryCategory) {
        case PluginCategory::Weapons:
            weaponsMods.push_back(p.name);
            break;
        case PluginCategory::Armor:
            armorMods.push_back(p.name);
            break;
        case PluginCategory::Keywords:
            keywordMods.push_back(p.name);
            break;
        case PluginCategory::LeveledLists:
            leveledMods.push_back(p.name);
            break;
        default:
            // Category None: currently ignored for multiplexing
            log << "CATEGORY: " << p.name << " has no recognized primary category; skipping for multiplexing.\n";
            break;
        }
    }

    std::size_t totalMultiplexed =
        weaponsMods.size() + armorMods.size() + keywordMods.size() + leveledMods.size();

    if (totalMultiplexed == 0 && protectedOnly.empty()) {
        std::string msg = "ERROR: After category classification and protections, there are no plugins to multiplex or list.";
        log << msg << "\n";
        std::cerr << msg << "\nPress Enter to exit...";
        std::cin.get();
        return 1;
    }

    if (groupSize <= 0) {
        // Auto group size based on total multiplexed mods and maxSlots
        groupSize = static_cast<int>(
            std::ceil(static_cast<double>(totalMultiplexed) / std::max(1, maxSlots))
            );
        if (groupSize < 1) groupSize = 1;
        log << "Auto-grouping: totalMultiplexed=" << totalMultiplexed
            << ", maxSlots=" << maxSlots
            << ", chosen GroupSize=" << groupSize << "\n";
    }
    else {
        log << "Using configured GroupSize=" << groupSize << " (maxSlots=" << maxSlots << ")\n";
    }

    //
    // STEP 5 — Build CSV: DummySlot, Virtual_ID, Category, Mods
    //
    std::ofstream outCSV(outputCSV);
    if (!outCSV) {
        std::string msg = "ERROR: Failed to open output CSV: " + outputCSV;
        log << msg << "\n";
        std::cerr << msg << "\nPress Enter to exit...";
        std::cin.get();
        return 1;
    }

    outCSV << "\"DummySlot\",\"Virtual_ID\",\"Category\",\"Mods\"\n";

    // Alias map: plugin -> dummy file
    std::map<std::string, std::string> aliasMap;
    std::unordered_set<std::string> usedDummyFiles;

    auto assignCategoryGroup = [&](PluginCategory cat, const std::vector<std::string>& mods) {
        if (mods.empty()) return;

        // Collect dummy slots for this category
        std::vector<const DummySlotDef*> slots;
        for (const auto& s : DUMMY_SLOTS) {
            if (s.category == cat) {
                slots.push_back(&s);
            }
        }

        if (slots.empty()) {
            log << "WARNING: No dummy slots available for category " << CategoryToString(cat) << "\n";
            return;
        }

        std::size_t index = 0;
        int slotIndex = 0;

        while (index < mods.size() && slotIndex < static_cast<int>(slots.size())) {
            const DummySlotDef* slot = slots[slotIndex];
            const std::string& dummyName = slot->name;
            int virtualID = slot->virtualID;

            std::ostringstream modsList;
            bool first = true;
            int countInSlot = 0;

            while (index < mods.size() && countInSlot < groupSize) {
                std::string cleanName = Sanitize(mods[index]);
                if (!first) modsList << ", ";
                first = false;

                modsList << CsvEscape(cleanName);

                auto it = aliasMap.find(cleanName);
                if (it != aliasMap.end() && it->second != dummyName) {
                    log << "WARNING: Sanitization collision: '" << mods[index]
                        << "' and another plugin both map to key '" << cleanName
                        << "'. Existing dummy=" << it->second
                        << ", new dummy=" << dummyName << "\n";
                }

                aliasMap[cleanName] = dummyName;
                usedDummyFiles.insert(dummyName);

                ++index;
                ++countInSlot;
            }

            outCSV << "\"" << dummyName
                << "\",\"" << virtualID
                << "\",\"" << CategoryToString(cat)
                << "\",\"" << modsList.str() << "\"\n";

            if (cfg.logDetails) {
                log << dummyName << " (VirtualID=" << virtualID
                    << ", Category=" << CategoryToString(cat)
                    << ") -> " << modsList.str() << "\n";
            }

            ++slotIndex;
        }

        if (index < mods.size()) {
            std::ostringstream oss;
            oss << "WARNING: More " << CategoryToString(cat)
                << " plugins than available dummy slots for that category. Some will not be represented.";
            FailOrWarn(false, oss.str(), log);
        }
        };

    assignCategoryGroup(PluginCategory::Weapons, weaponsMods);
    assignCategoryGroup(PluginCategory::Armor, armorMods);
    assignCategoryGroup(PluginCategory::Keywords, keywordMods);
    assignCategoryGroup(PluginCategory::LeveledLists, leveledMods);

    //
    // STEP 6 — Write slot.cfg
    //
    log << "DEBUG: Writing slot.cfg to: " << outputSlotCfg << "\n";

    std::ofstream outCfg(outputSlotCfg);
    if (!outCfg) {
        std::string msg = "ERROR: Failed to open output slot.cfg: " + outputSlotCfg;
        log << msg << "\n";
        std::cerr << msg << "\nPress Enter to exit...";
        std::cin.get();
        return 1;
    }

    outCfg << "fileIndex = 0xF0\n";

    // [modules] line: all plugins involved in any way (protected + multiplexed)
    outCfg << "modules = ";
    {
        bool first = true;
        auto writeName = [&](const std::string& n) {
            if (!first) outCfg << ",";
            first = false;
            outCfg << n;
            };

        for (const auto& p : includedPlugins) {
            writeName(p.name);
        }
        for (const auto& p : worldspaceSkipped) {
            writeName(p);
        }
    }
    outCfg << "\n\n";

    outCfg << "[Slots]\n";
    for (const auto& kv : aliasMap) {
        log << "DEBUG: Writing slot entry: [" << kv.first << "] -> " << kv.second << "\n";
        outCfg << kv.first << "=" << kv.second << "\n";
    }

    log << "DEBUG: Finished writing [Slots] section.\n";

    log << "DEBUG: Writing [Aliases] section...\n";
    outCfg << "\n[Aliases]\n";
    for (const auto& kv : aliasMap) {
        outCfg << kv.first << "=" << kv.second << "\n";
    }

    log << "DEBUG: Writing [Modules] section...\n";
    outCfg << "\n[Modules]\n";

    for (const auto& slot : DUMMY_SLOTS) {
        if (usedDummyFiles.find(slot.name) != usedDummyFiles.end()) {
            outCfg << "[Module]\n";
            outCfg << "File=" << slot.name << "\n";
            outCfg << "Enabled=1\n\n";
        }
    }

    log << "DEBUG: Finished writing [Modules] section.\n";

    //
    // STEP 7 — Final validation
    //
    bool validationError = false;

    std::size_t totalNonProtected =
        weaponsMods.size() + armorMods.size() + keywordMods.size() + leveledMods.size();

    if (aliasMap.size() != totalNonProtected) {
        std::ostringstream oss;
        oss << "Alias map size (" << aliasMap.size()
            << ") does not match non-protected multiplexed plugin count ("
            << totalNonProtected << ")";
        validationError |= FailOrWarn(cfg.strictValidation, oss.str(), log);
    }

    if (totalNonProtected > 0 && usedDummyFiles.empty()) {
        validationError |= FailOrWarn(cfg.strictValidation,
            "There are multiplexed plugins but no dummy files were used. Check grouping and categories.",
            log);
    }

    if (validationError && cfg.strictValidation) {
        std::cerr << "StrictValidation=1, aborting due to validation errors.\nPress Enter to exit...";
        std::cin.get();
        return 1;
    }

    std::cout << "CSV + slot.cfg written successfully.\n";
    log << "CSV + slot.cfg written successfully.\n";

    log << "DEBUG: slot.cfg final size = "
        << std::filesystem::file_size(outputSlotCfg)
        << " bytes\n";

    std::cout << "\nDone. Press Enter to exit...";
    std::cin.get();

    return 0;
}
