Archipelago Tyrian v0.4.5-alpha4

== Setup =======================================================================

After downloading APTyrian, you will need to download the data files for Tyrian
and extract them to a subfolder called "data". Tyrian 2.1 data files will only
work for seeds with "enable_tyrian_2000_support" turned OFF, whereas Tyrian 2000
data files will work for all seeds.

Tyrian 2.1:
  https://www.camanis.net/tyrian/tyrian21.zip

Tyrian 2000:
  https://www.camanis.net/tyrian/tyrian2000.zip

== Important Notes =============================================================

Connecting to an Archipelago server can be done through the in-game menus, or
alternatively through the command line with the following option:
  --connect="<slot_name>@archipelago.gg:<port_number>"

If you have a single player .aptyrian file, drag and drop it over the game
window while in the "Select Game Mode" menu to start playing it. The option
within that menu to play a local game will eventually open a file dialog, but
that has yet to be implemented.

Progress is automatically saved after exiting a level or on quitting the game.

The following things are planned but not yet implemented:
- Logic for Episodes 3, 4, and 5
- Rebinding inputs in game; If you want to do this, you'll still have to edit
  save/aptyrian.cfg yourself for now, sorry
- "Chaos" Twiddles (full randomizer, instead of giving random vanilla twiddles)
- Boss Weaknesses (require specific weapons to beat the boss of a goal episode)
- "Ship Info" display

== Keyboard Controls ===========================================================

alt-enter      -- toggle full-screen

arrow keys     -- ship movement
space          -- fire weapons
enter          -- toggle rear weapon mode
ctrl/alt       -- fire left/right sidekick

== Links =======================================================================

Archipelago Tyrian:
  Client:  https://github.com/KScl/TyrianArchipelago
  APWorld: https://github.com/KScl/TyrianAPWorld

OpenTyrian
  Project: https://github.com/opentyrian/opentyrian
  IRC:     ircs://irc.oftc.net/#opentyrian
  Forums:  https://tyrian2k.proboards.com/board/5

