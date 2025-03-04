## Tyrian Archipelago

Archipelago Client for Tyrian.

The corresponding APWorld can be found at https://github.com/KScl/TyrianAPWorld.

### Setup

After downloading APTyrian, you will need to download the data files for Tyrian
and extract them to a subfolder called "data". Tyrian 2.1 data files will only
work for seeds with "enable_tyrian_2000_support" turned OFF, whereas Tyrian 2000
data files will work for all seeds.

* [Tyrian 2.1](https://www.camanis.net/tyrian/tyrian21.zip)
* [Tyrian 2000](https://www.camanis.net/tyrian/tyrian2000.zip)

### Important Notes

Connecting to an Archipelago server can be done through the in-game menus, or
alternatively through the command line with the following option. Password may
be omitted if there isn't one.
* `--connect="<slot_name>:<password>@archipelago.gg:<port_number>"`

If you have a single player .aptyrian file, drag and drop it over the game
window while in the "Select Game Mode" menu to start playing it. The option
within that menu to play a local game will eventually open a file dialog, but
that has yet to be implemented.

Future roadmap:
* Ship Specs screen (showing received item progress, twiddles, other stats)
* Chaos twiddle randomizer (new actions, new inputs)

