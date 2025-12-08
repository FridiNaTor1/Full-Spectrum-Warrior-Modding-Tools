# This is a all-in-one tool for modding Full Spectrum Warrior
This tool is written in Python 3 and PyQt5, so those are your dependencies. If you got them installed just run: python fsw_mod_kit.py
# Features
A string modification to FSW.dll to redriect GameSpy to OpenSpy, restoring Online Functionality.

A binary modification to FSW.dll to allow to infinite downed soldiers, a side affect is that your soldiers' skull counter wont appear, and wont drain. This is honestly my favorite feature as it very fun to play around with.

A modification to all the level .PAK files that increases how many Grenades and M203 you have to 999, The game itself can only display 99 as max, but it value is really 999.

A modification to all the level .PAK files that deactives all mission failure triggers, excluding having more than one downed soldier, that one you need the binary patch to FSW.dll to allow infinte downed soldiers.

A quick and easy tool to set a custom Resolution, this one only creates or modifies the game's Resolution.cfg.

A tool to modify level descriptors. This one is a bit experimental, as there was no extra space where this one is stored inside the level .PAK files, that you can't increase any of the strings or values, they need to be either shorter or the same length!

A tool to modify Game Rules. This one is very interesting as a lot of aspects in the game is stored here, color of the hud, curser size, max Ammo and grenades, etc. Check it out! This one you can increase values by a lot, as where this is stored in the levels .PAK archives, has a lot of NULs ahead, which can be safely overwritten. The tool is programed to give you a fail if you actually exceed the limit, but good luck on that!
# Compatibility
So far this tool I have only tested it on Linux, and the PC version of the game that released on Steam. But feel free to try it on other PC versions, like the original disc version, I feel like everything might work except for binary modification to allow infinite downed soldiers, if the sequence it will be looking for doesn't exist!
I am looking into support modding the PS2 version of the game aswell, as a review copy of the game that had .ndata symbols on it, helped a lot to figure out how to patch the downed soldiers thingy, when I was exploring in Ghidra, comparing the PC counterpart that doesn't have symbols. I am curious tho if anyone could try this with the OG Xbox version, as that one also had .PAK level files, perhaps the Tools that modify them will work! The PS2 version uses BOLT archives, which I will need to explore closer.
# Future Plans
A Localisation editor, should be self explanitory!

A Sound files extractor/editor. Not sure if I will find a way to edit the audio of the game, but I do see a way to get them extracted, I am planning on adding this very soon!

A Shell editor, where you can edit the the menus, as most of them are stored in what looks to be a CSV format inside the .PAK/.pak files. This one is not high priority!

A more detailed Color editor. Right now within the Game Rules you can change some Color values, to change Color of hud elements. When I have fully tested all of them, I am planning on making a tool just for this purpose, and have pictures, that show what you will actually be editing in the game. This one is something I am very interested in adding soon-ish.

A detailed hud editor. This one would most likley come after the Color editor, unless I would release both at the same time, this one would have in game screenshots and show the hud element you would be editing, and get this! Actually able to move! You can move the position of the hud elements to your hearts content if you know what you are doing with the Game Rules editor, but I would like this be easier for everyone!
