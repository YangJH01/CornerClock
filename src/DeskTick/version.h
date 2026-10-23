// version.h — the version, in one place.
//
// Included by DeskTick.rc (the VERSIONINFO block that gives the exe its identity)
// and by about.cpp (the line the About box shows). They must agree: an exe
// whose properties say one thing and whose About box says another is exactly
// the kind of small inconsistency a scanner has no way to forgive, and a user
// reporting a bug has no way to describe.
//
// XML cannot include this, so two copies are kept by hand: Version= in
// the assemblyIdentity in DeskTick.manifest (the desktop build).
//
// RC only understands #define, so this file must stay free of anything else —
// no #pragma once (harmless but pointless here), no types, no C++.

#define VER_MAJOR       1
#define VER_MINOR       0
#define VER_PATCH       3
#define VER_BUILD       6

#define VER_NUMBER      1,0,3,6
#define VER_STRING      "1.0.3.6"
#define VER_DISPLAY     "v1.0.3.6 CornerClock"

#define VER_COMPANY     "CornerClock contributors (unofficial fork)"
#define VER_PRODUCT     "CornerClock"
#define VER_DESCRIPTION "CornerClock desktop clock widget"
// DeskTick.rc only. The About box builds its own line from VER_COMPANY plus a
// translated "All rights reserved." — the holder must not go through a
// translator, and this whole string as one key would make nineteen of them
// retype it. VERSIONINFO stays US English on purpose, so this stays as it is.
#define VER_COPYRIGHT   "\xA9 RYF Tools. All rights reserved."
#define VER_FILENAME    "CornerClock.exe"
