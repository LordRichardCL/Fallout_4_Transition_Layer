⭐ CSV Builder for Fallout 4 Modding
Load Order Virtualization for the aSW Multiplexer Runtime
csvbuilder.exe analyzes your plugin load order, detects record categories, and automatically groups your mods into the correct dummy slots used by the aSW Multiplexer.
This replaces all manual chunking and guarantees a clean, deterministic mapping every time.

📂 Setup
- Place csvbuilder.exe anywhere you like
(example: E:\Fallout4 Modding Support\csvbuilder\)
- Create or export a file named loadorder.txt in the same folder.
- One plugin per line
- No paths, just filenames
- Example:
Unofficial Fallout 4 Patch.esp
RaiderOverhaul.esp
ProjectValkyrie.esp
Depravity.esp
SimSettlements.esp
WeaponsOfFate.esp
- (Optional) Create csvbuilder.ini to customize behavior.
If missing, defaults are used.
- (Optional) Create protected_plugins.json if you want to mark plugins that should never be multiplexed.
- Correct path:
Fallout 4/Data/F4SE/Plugins/Multiplexer/extern/protected_plugins.json
▶️ Running the Tool- Double‑click csvbuilder.exe
- The tool will:
- Locate your Fallout 4 installation
- Read loadorder.txt
- Analyze each plugin
- Detect categories (Weapons, Armor, Keywords, Leveled Lists)
- Detect worldspace‑touching plugins
- Detect mixed‑record plugins
- Apply protected plugin rules
- Generate a clean CSV + slot.cfg
- The window pauses at the end so you can read any messages.
📄 Output FilesAll output is written to:Fallout 4/Data/F4SE/Plugins/Multiplexer/
You will find:✔ loadorder_mapped_filtered_clean.csvThe final mapping used by the Multiplexer runtime.✔ slot.cfgThe runtime routing table (dummy slots → real plugins).✔ csvbuilder.logDetailed log of:- category detection
- grouping decisions
- alias resolution
- protected plugin handling
- mixed record detection
- worldspace detection
✔ worldspace_skipped.txtList of plugins that contain worldspace/cell/navmesh records
(these cannot be multiplexed).✔ worldspace_diagnostics.txtDetailed explanation of why each worldspace plugin was skipped.✔ mixed_records.txtPlugins that contain multiple safe record categories
(e.g., WEAP + ARMO).
These are still multiplexed normally, but logged for transparency.✔ extern/protected_plugins.jsonUser‑maintained list of plugins that must never be multiplexed.
This file is optional.⚙️ Configuration (csvbuilder.ini)Only four settings affect the tool:[General]

GroupSize=200
IgnoreDisabled=1
StrictValidation=1
LogDetails=1
GroupSizeNumber of real plugins per dummy slot.
Capacity per category = GroupSize × 5.Example:
GroupSize = 200 → 1000 mods per categoryIgnoreDisabledSkip lines starting with # in loadorder.txt.StrictValidationAbort on errors (recommended).LogDetailsEnable detailed logging.⚠️ Important BehaviorWorldspace Plugins Are Never MultiplexedPlugins containing:- WRLD
- CELL
- LAND
- NAVM
- REFR
- ACHR
…are automatically skipped and logged.Protected Plugins Are Never MultiplexedIf listed in:Fallout 4/Data/F4SE/Plugins/Multiplexer/extern/protected_plugins.json
…they remain untouched.Mixed‑Record Plugins Are AllowedIf a plugin touches multiple safe categories (e.g., WEAP + ARMO),
it is still multiplexed normally, but logged for transparency.Unlimited Load Order SupportThe tool can handle hundreds or thousands of plugins.
Scalability is controlled by GroupSize.✅ Workflow Summary- Export your plugin list → loadorder.txt
- Place it next to csvbuilder.exe
- (Optional) Configure csvbuilder.ini
- (Optional) Add protected plugins to:
Fallout 4/Data/F4SE/Plugins/Multiplexer/extern/protected_plugins.json


- Run the tool
- Review any warnings (worldspace, mixed, protected)
- The tool writes all output to:
Fallout 4/Data/F4SE/Plugins/Multiplexer/
- Launch Fallout 4 via F4SE
- Check aSWMultiplexer.log for final routin
