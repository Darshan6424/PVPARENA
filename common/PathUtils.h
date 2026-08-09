#pragma once
// PathUtils.h
// Asset loading needs to find files relative to the .exe itself, not
// relative to whatever working directory happened to launch it (VS
// Code's CMake Tools, Explorer, and a plain command prompt all differ
// here). This resolves the executable's own folder so "assets/..."
// paths work no matter how the game was started.

#include <string>

namespace paths {

// Returns the directory containing the currently-running executable,
// with a trailing slash. Falls back to "./" if it can't be determined.
std::string executableDir();

} // namespace paths
