#pragma once
// Assets have to be found relative to the executable, not the working
// directory - VS Code, Explorer and a plain terminal all launch it with a
// different one.

#include <string>

namespace paths {

// Directory holding the running executable, with a trailing slash.
// "./" if it can't be worked out.
std::string executableDir();

} // namespace paths
