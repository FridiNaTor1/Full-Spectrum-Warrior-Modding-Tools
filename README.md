# This is a all-in-one tool for modding Full Spectrum Warrior
This tool is written in Python 3 and PyQt5, so those are your dependencies. If you got them installed just run: `python fsw_mod_kit.py`.

## Native builds (Windows & Linux)
A portable C++20 implementation mirrors the full Python toolset: DLL tweaks, PAK mission failure removal, "999 grenades/M203/smoke" ammo updates, descriptor editing, game rules editing, and `Resolution.cfg` writing. Both a CLI and Qt GUI are built from the same core logic.

### Building with CMake
```bash
cmake -S . -B build
cmake --build build --config Release
```

On Windows, run the same commands in a Visual Studio Developer Prompt to produce both `fsw_mod_kit_cli.exe` and `fsw_mod_kit_gui.exe`.

### CLI usage
```bash
./build/fsw_mod_kit_cli patch-no-downs /path/to/FSW.dll
./build/fsw_mod_kit_cli patch-openspy /path/to/FSW.dll
./build/fsw_mod_kit_cli patch-no-mission-failures /path/to/Chapters
./build/fsw_mod_kit_cli patch-ammo-999 /path/to/Chapters
./build/fsw_mod_kit_cli write-resolution /game/install 1920 1080
```

### GUI usage
Launch the Qt GUI from the build directory:
```bash
./build/fsw_mod_kit_gui
```
Select your install directory, then click the patch buttons or adjust the resolution fields before writing `Resolution.cfg`. The GUI also exposes the Descriptor Editor and Game Rules Editor under the "Editors" section to match the Python workflow.

# Features
A string modification to FSW.dll to redriect GameSpy to OpenSpy, restoring Online Functionality.

A binary modification to FSW.dll to allow to infinite downed soldiers, a side affect is that your soldiers' skull counter wont a
ppear, and wont drain. This is honestly my favorite feature as it very fun to play around with.

A modification to all the level .PAK files that increases how many Grenades and M203 you have to 999, The game itself can only d
isplay 99 as max, but it value is really 999.

A modification to all the level .PAK files that deactives all mission failure triggers, excluding having more than one downed so
ldier, that one you need the binary patch to FSW.dll to allow infinte downed soldiers.

A quick and easy tool to set a custom Resolution, this one only creates or modifies the game's Resolution.cfg.

A tool to modify level descriptors. This one is a bit experimental, as there was no extra space where this one is stored inside
the level .PAK files, that you can't increase any of the strings or values, they need to be either shorter or the same length!

A tool to modify Game Rules. This one is very interesting as a lot of aspects in the game is stored here, color of the hud, curs
er size, max Ammo and grenades, etc. Check it out! This one you can increase values by a lot, as where this is stored in the lev
els .PAK archives, has a lot of NULs ahead, which can be safely overwritten. The tool is programed to give you a fail if you act
ually exceed the limit, but good luck on that!
# Compatibility
So far this tool I have only tested it on Linux, and the PC version of the game that released on Steam. But feel free to try it
on other PC versions, like the original disc version, I feel like everything might work except for binary modification to allow
infinite downed soldiers, if the sequence it will be looking for doesn't exist!
I am looking into support modding the PS2 version of the game aswell, as a review copy of the game that had .ndata symbols on it
, helped a lot to figure out how to patch the downed soldiers thingy, when I was exploring in Ghidra, comparing the PC counterpa
rt that doesn't have symbols. I am curious tho if anyone could try this with the OG Xbox version, as that one also had .PAK leve
l files, perhaps the Tools that modify them will work! The PS2 version uses BOLT archives, which I will need to explore closer.
# Future Plans
A Localisation editor, should be self explanitory!

A Sound files extractor/editor. Not sure if I will find a way to edit the audio of the game, but I do see a way to get them extr
acted, I am planning on adding this very soon!

A Shell editor, where you can edit the the menus, as most of them are stored in what looks to be a CSV format inside the .PAK/.p
ak files. This one is not high priority!

A more detailed Color editor. Right now within the Game Rules you can change some Color values, to change Color of hud elements.
 When I have fully tested all of them, I am planning on making a tool just for this purpose, and have pictures, that show what y
ou will actually be editing in the game. This one is something I am very interested in adding soon-ish.

A detailed hud editor. This one would most likley come after the Color editor, unless I would release both at the same time, thi
s one would have in game screenshots and show the hud element you would be editing, and get this! Actually able to move! You can
 move the position of the hud elements to your hearts content if you know what you are doing with the Game Rules editor, but I w
ould like this be easier for everyone!
