DXX-Redux
=========

Descent 1&2 source port based on [DXX-Retro](https://github.com/CDarrow/DXX-Retro).

This has been built/tested against Debian Linux 14 but should work on other platforms.

This has been tested with a PSVR2 Headset on the Monado Drivers.

Status
-------------------------
VR Cutscene support - Complete
VR Gameplay - Complete
VR Menu Support - Complete
Multiplayer VR Messages - Not Tested

Use
-------------------------
To launch with VR Support run: ./d2x-redux -vr

It is highly recommended you calibrate your SteamVR headset by placing it on the ground and calibrating the play area with a floor distance of zero.


Building in Visual Studio
-------------------------

- [Install vcpkg](https://vcpkg.io/en/getting-started.html)

- Set the VCPKG_ROOT environment variable to the vcpkg folder with Start > search Environment >
  Environment Variables > New...

- Open the d1 or d2 folder in Visual Studio with 'Open a local folder' or Open > CMake...

- Wait until CMake generation finishes

- Select Startup Item main/d1x-redux.exe

- Optionally select Debug > Debug and Launch Settings for d1x-redux and add
  below the line starting with "name":
  `"args": [ "-hogdir", "c:\game-data-folder" ]`

Building in msys2 / Linux
-------------------------

- Install the required packages

  - msys2 (use the 'MSYS2 MINGW64' entry in the Start menu)

    `pacman -S git mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake
    mingw-w64-x86_64-physfs mingw-w64-x86_64-SDL mingw-w64-x86_64-SDL_mixer
    mingw-w64-x86_64-libpng mingw-w64-x86_64-glew`

  - Debian/Ubuntu

    `apt install build-essential git cmake libphysfs-dev libsdl2-dev libsdl2-mixer-dev libpng-dev
    libglew-dev libopenvr-dev`

  - Fedora

    `dnf install make gcc-c++ git cmake physfs-devel sdl2-devel libsdl2-mixer-devel libpng-devel
    glew-devel`

  - Arch Linux

    `pacman -S base-devel git cmake physfs sdl2 sdl2-mixer libpng glew`

- Get the source code from github

  `git clone https://github.com/dxx-redux/dxx-redux`

- Enter the dxx-redux directory

  `cd dxx-redux`

- Enter the the d1 or d2 directory

  `cd d1`

- Set the build options

  `cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo`

  (see `cmake -B build -L` for more options)

- Build the code

  `cmake --build build`

  (add `-j4` to use 4 cores)
