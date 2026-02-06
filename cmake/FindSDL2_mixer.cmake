
# Locate SDL2_mixer library
# This module defines
# SDL2_MIXER_LIBRARY, the name of the library to link against
# SDL2_MIXER_FOUND, if false, do not try to link to SDL2_mixer
# SDL2_MIXER_INCLUDE_DIR, where to find SDL_mixer.h
#
# $SDL2DIR is an environment variable that would
# correspond to the ./configure --prefix=$SDL2DIR
# used in building SDL2.
# l.e.galup  9-20-02
#
# Modified by Eric Wing.
# Added code to assist with automated building by using environmental variables
# and providing a more controlled/consistent search behavior.
# Added new modifications to recognize OS X frameworks and
# additional Unix paths (FreeBSD, etc).
# Also corrected the header search path to follow "proper" SDL guidelines.
# Added a search for SDL2_mixer which is needed by some platforms.
# Added a search for SDL2_mixer which is needed by some platforms.
#
# For backward compatiblity the upper case versions of the variables are also defined
# SDL2_MIXER_LIBRARY, the name of the library to link against
# SDL2_MIXER_FOUND, if false, do not try to link to SDL2_mixer
# SDL2_MIXER_INCLUDE_DIR, where to find SDL_mixer.h
#
# $SDL2DIR is an environment variable that would
# correspond to the ./configure --prefix=$SDL2DIR
# used in building SDL2.
#
# Created by Eric Wing.
# This was influenced by the FindSDL.cmake module.

#=============================================================================
# Copyright 2005-2009 Kitware, Inc.
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================
# (To distribute this file outside of CMake, substitute the full
#  License text for the above reference.)

FIND_PATH(SDL2_MIXER_INCLUDE_DIR SDL_mixer.h
  HINTS
  $ENV{SDL2MIXERDIR}
  $ENV{SDL2DIR}
  PATH_SUFFIXES include/SDL2 include
  PATHS
  ~/Library/Frameworks
  /Library/Frameworks
  /usr/local/include/SDL2
  /usr/include/SDL2
  /sw/include/SDL2 # Fink
  /opt/local/include/SDL2 # DarwinPorts
  /opt/csw/include/SDL2 # Blastwave
  /opt/include/SDL2
)

FIND_LIBRARY(SDL2_MIXER_LIBRARY
  NAMES SDL2_mixer
  HINTS
  $ENV{SDL2MIXERDIR}
  $ENV{SDL2DIR}
  PATH_SUFFIXES lib64 lib
  PATHS
  ~/Library/Frameworks
  /Library/Frameworks
  /usr/local
  /usr
  /sw
  /opt/local
  /opt/csw
  /opt
)

SET(SDL2_MIXER_FOUND "NO")
IF(SDL2_MIXER_LIBRARY AND SDL2_MIXER_INCLUDE_DIR)
  SET(SDL2_MIXER_FOUND "YES")
ENDIF(SDL2_MIXER_LIBRARY AND SDL2_MIXER_INCLUDE_DIR)
