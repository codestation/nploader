NPloader (DLC loader) - codestation

This plugin allows direct reading and loading of DLC (EDATA/PGD/SPRX) files in
their decrypted form, so you don't need to reactivate your account and/or switch
between them while using your purchased DLC from different regions. You can mix
decrypted/encrypted files without problem too, the plugin takes care of that.

Instructions:

Copy the plugin in the seplugins folder and enable it in game.txt using the following
line: ms0:/seplugins/nploader.prx 1

Tested on CFW from 5.00 to 6.60 (Pro CFW is recommended).

New in 0.8:
Now you can place your DLC folders in a custom location (useful if you don't want to fill
up your xmb menu with content that you aren't gonna launch). To do this create a file named
nploader.ini (or use the file provided) and add/edit the path where you are gonna move
your DLC folders. Example: ms0:/PSP/GAME/DLC
Note: remember to add a newline to the end of the file.

Important: the redirection is only for DLC files, if you have another content that is using
the DLC folder (e.g.: game updates), then these files must remain in that directory
(e.g.: the *ARC patch files from the LBP update).

Known issues:
Do not mix with the noDRM implementation of Pro CFW. If you are using one of them
then disable the other.

Changelog:
v0.9: Fixed bug with redirection on np_rename.
      Added support for newer games that load at 0x08900000.
v0.8: Added support for custom DLC location.
      Added support for games that lauch additional user modules that uses
      the npdrm api (this fixes the Buzz! games).
v0.7: Proper fix to detect the correct user module of the game.
      Skip opnssmp module when is loaded.
      Released source code.
v0.6: Added all the incarnations of the Prometheus loaded to the module blacklist
      so the plugin doesn't make the mistake of hooking to those. This fix will 
      ensure that the plugin will work with different patched versions of the games.
      A proper fix (removing the blacklist and identify the game module properly) 
      is gonna to be included in a future release.
v0.5-r1: removed pspSdk* obsolete functions
v0.5: Implemented the rest of the npdrm API so now works with games like GEB.
      Added support for loading decrypted sprx files.
v0.4: Fixed black screen with GoW: GoS. Probably this fixes other games with 
      black screen when using this plugin.
v0.3: Support for m33 and inferno UMD ISO mode.
v0.2: Unified plugins in one (nploader.prx) and fixed issue with aLoader.
v0.1: Initial release.

Contact: identi.ca/Twitter @codestation
