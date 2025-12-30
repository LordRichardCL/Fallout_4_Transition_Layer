⭐ aSWMultiplexer – Automated Slot & Weapon Multiplexer (F4SE Plugin)
Version: 1.1.0
Author: Romewolf
Requirements: Fallout 4 (Old‑Gen) + F4SE

📘 Overview
aSWMultiplexer is an advanced F4SE plugin that automatically scans your Fallout 4 load order, extracts supported records, and remaps them into category‑specific dummy ESP plugins for multiplexer‑based workflows.
It is designed for:
- Mod authors
- Patch creators
- Large modlist maintainers
- Advanced users who want automated record extraction and mapping
The plugin runs silently and safely, requiring no user interaction unless you choose to customize it via the included multiplexer.ini.

🧱 Base Collection Requirement (StoryWealth)
aSWMultiplexer is designed to operate on top of a stable base collection, such as StoryWealth.
StoryWealth provides:
- A stable worldspace
- Predictable masters
- A consistent load order
- A safe foundation for runtime rewrite
The Multiplexer does not modify or replace the base collection.
Instead, it layers additional mods on top of it, safely and dynamically.
Plugins containing worldspace records are automatically skipped to ensure the base collection remains stable.

🧩 Dummy Plugin Architecture (5 ESPs per Category)
aSWMultiplexer distributes extracted records into five dummy ESP plugins per category, allowing the system to scale safely across large modlists.
These dummy plugins are included in:
Tools\Dummy Plugins\


IMPORTANT: Users must copy these dummy ESPs into their Data folder:
Fallout 4\Data\


The Multiplexer will not function correctly unless these dummy plugins are present and active.

📦 Full Dummy Plugin List
🔫 Weapons (5 ESPs)
Dummy_Weapons_01.esp
Dummy_Weapons_02.esp
Dummy_Weapons_03.esp
Dummy_Weapons_04.esp
Dummy_Weapons_05.esp


🛡 Armor (5 ESPs)
Dummy_Armor_01.esp
Dummy_Armor_02.esp
Dummy_Armor_03.esp
Dummy_Armor_04.esp
Dummy_Armor_05.esp


🧩 Keywords (5 ESPs)
Dummy_Keywords_01.esp
Dummy_Keywords_02.esp
Dummy_Keywords_03.esp
Dummy_Keywords_04.esp
Dummy_Keywords_05.esp


📦 Leveled Lists (5 ESPs)
Dummy_LeveledLists_01.esp
Dummy_LeveledLists_02.esp
Dummy_LeveledLists_03.esp
Dummy_LeveledLists_04.esp
Dummy_LeveledLists_05.esp


This structure ensures:
- High capacity
- Predictable routing
- Clean category separation
- Safe FormID spacing
- Future‑proof scalability

🧠 Should Dummy Plugins Be ESL‑Flagged?
✔ Recommended: Do NOT ESL‑flag dummy plugins
Reasons:
- ESL plugins use compact FormIDs (0x000–0xFFF)
Dummy plugins need the full 0x000000–0xFFFFFF range for large modlists.
- ESL plugins occupy FE slots
This complicates runtime rewrite and increases collision risk.
- The Plugin already handles ESL mods correctly
ESL FormIDs are rewritten into dummy ESPs — this only works cleanly if the dummy plugins are not ESL.
- ESL‑flagged dummy plugins would drastically reduce capacity
Only ~4096 compact IDs per plugin.
- ESP dummy plugins are stable and predictable
They behave consistently across all mod managers.
⭐ Final Recommendation:
Keep dummy plugins as ESPs.
This gives maximum stability, capacity, and compatibility.

🚀 Features
• Automatic scanning of all loaded plugins
• Extracts WEAP, ARMO, KYWD, LVLI
• Full zlib support (compressed records)
• Full ESL support
• Worldspace‑safe scanning
• CSV‑driven dummy plugin routing
• Runtime FormID rewrite system
• Whitelist support
• SkippedModules.txt generation
• Lightweight, safe, and fast

📦 Installation
- Install F4SE.
- Extract this mod’s ZIP archive into your Fallout 4 directory.
Your folder structure should look like:
Fallout 4/
└─ Data/
   └─ F4SE/
      └─ Plugins/
         ├─ aSWMultiplexer.dll
         ├─ multiplexer.ini
         └─ extern/
             └─ Whitelist/
                 └─ protected_plugins.json
└─ Tools/
   └─ Dummy Plugins/
       ├─ Dummy_Weapons_01.esp
       ├─ Dummy_Weapons_02.esp
       ├─ ...
       └─ Dummy_LeveledLists_05.esp


3. Copy all dummy ESPs from:
Tools\Dummy Plugins\


…into:
Fallout 4\Data\


- Launch the game using f4se_loader.exe.

⚙ Configuration (multiplexer.ini)
Located at:
Data\F4SE\Plugins\Multiplexer\multiplexer.ini


Default contents:
[General]
bEnableDebugLogging=0
bEnableESLDebug=0
bShowConsole=0
bScanOnStartup=1
bEnableRuntimeRewrite=1
bWriteSkippedModules=1
sCSVPath=
bEnableExperimentalFeatures=0



🧪 CSV Builder Tool (csvbuilder.exe)
Located in:
Tools\csvbuilder.exe


Outputs to:
Data\F4SE\Plugins\Multiplexer\output\



📄 Logging
All logs are written to:
Documents\My Games\Fallout4\F4SE\Multiplexer.log



🗑 Uninstallation
Delete:
Data\F4SE\Plugins\Multiplexer\aSWMultiplexer.dll
Data\F4SE\Plugins\Multiplexer\multiplexer.ini
Tools\csvbuilder.exe


This plugin does not modify saves or game data.

🔧 Compatibility
• Fully compatible with ESP, ESM, and ESL plugins
• Correctly handles compact FormIDs and FE‑range references
• Safe to add or remove at any time
• Works with MO2, Vortex, and manual installs

⚠ Known Issues
• Some plugins may contain malformed or unusual subrecords
• ESL FE‑slot detection uses a deterministic pseudo‑slot
• Worldspace‑containing plugins are skipped by design

🧭 Future Plans
• Expanded record type support
• Automatic dummy plugin generation
• Optional JSON export
• Multiplexer profile system
• Advanced CSV builder automation

📜 Permissions
You may use this plugin in your own mods, patches, or tools.
Credit is appreciated and required.
