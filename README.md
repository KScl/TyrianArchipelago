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

### Important Notes (alpha1)

The menu for choosing a server / slot name to connect to is currently
unfinished. You will need to start APTyrian with the command line option:
* `--connect="<slot_name>@archipelago.gg:<port_number>"`

Then, after choosing "Start Game", press ENTER on the otherwise empty screen
to start connecting to the Archipelago server.

Alternatively, if you have a single player .aptyrian file, drag and drop it onto
the game window while on the empty "Start Game" screen to start playing it.

The menu for modifying controls has not been reimplemented yet. You'll need to
start the game once, and then modify "aptyrian.cfg" in the "save" subdirectory
that is automatically created. Sorry.

Progress is automatically saved after exiting a level or on quitting the game.
