# RealityEngine-Public
Temporary public repo for RealityEngine my cross platform C11 based engine focussed on VR development.

DISCLAIMER: At this time RealityEngine is not intended for public use.  It's souly built to cover my needs for my Oculus Quest game Air Hockey Arcade: www.airhockeyarcade.com

I call it an engine but it is still very limited, containing many partially implemented or thought out ideas.  I'm making a version of it publically visible for educational purposes if anyone is interested and also as an example of my personal work for potential employers to view.

As such I have not attached an open source license to this repository, all code outside of 'external' remains the copyright of Guilty Dog Productions Ltd.  I do plan to Open Source this project in time and hopefully make it more useful to other developers but there is still a lot of work to do to clean it up. It's possible there is a small amount of code throughout the project that isn't completely mine.  For example, my main 'blinnphong' shaders are based on shader code from 'leanopengl.com' and ironically no longer implement **blinnphong** but **pbr** lighting.  I will properly attribute these files soon.

## Building

1. If you don't have it already, this project requires at least Visual Studio 2019 although I use Clang on both Windows and Android not the MSVC compiler.
2. Ensure the 'Desktop development with C++' workload is installed especially 'C++ Clang tools for Windows'
3. Also ensure the 'Mobile development with C++' workload is installed.  You just need the stuff for Android, you don't need the iOS tools etc.
4. Run 'setenv.bat' from a Command Prompt.  This will temporarily set an environment variable referenced in the premake5.lua script and temporarily append the path to my slightly customised build of premake5.exe to your %PATH%.  These changes will disappear when you close this Command Prompt.
5. Generate the Visual Studio project files by typing 'premake5 --oculus vs2019'. You can close the Command Prompt now, unless you like that sort of thing like I do :D
6. Open 'RealityEngine.sln' with Visual Studio 2019.  RealityEngine.sln will be created in the root folder.
7. To build for PC (Rift): set 'launcher' as the startup project and 'x64' as the architecture and hit 'F7'.
8. To build for Quest: set 'REQuest' as the startup project and 'ARM64' as the architecture and hit 'F7'.
9. Launch with 'F5' or 'Ctrl+F5'.  Running with the debugger on Quest considerably slows startup time.

**Issue:** I found out that using my 'setenv.bat' to set the RE_SDK_PATH environment variable temporarily, doesn't work corrently as the environment variable isn't visible to Visual Studio.  To work around this, without permenantly setting the environment variable you can run the 'x64 Native Tools Command Prompt for VS 2019' (found in the Visual Studio 2019 folder on the Start Menu) instead of a regular command prompt. Change to the project root folder, perform steps 4 & 5 above as normal and then open the solution with the command 'devenv RealityEngine.sln' 

**Note:** I've embeded a simple test project 'game' which is based on a chunk of the game framework for Air Hockey Arcade with the core Air Hockey code removed.  This gets built alongside the core engine and launched in step 9 above.  Assuming I've not been an idiot and forgotten something, it should run as is, but if you would like to run with the Oculus Platform features / Avatars you'll need to create or use existing APP_IDs on the Rift / Quest stores.  It should be obvious where to add them at the top of 'game.c'
 
## Keys:

* **ESC** - Quit
* **SPACE** - Toggle between the standard game view and a free roaming camera.  This is currently just a debug mode for me to zip around the world without wearing the HMD. Use **W, A, S & D** or **Arrow Keys** to move as in an FPS and the mouse to steer.

**I'll add more to this document over time.**
