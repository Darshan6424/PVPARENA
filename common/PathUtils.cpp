#include "PathUtils.h"

#ifdef _WIN32
    #include <windows.h>
#else
    #include <unistd.h>
    #include <limits.h>
#endif

namespace paths {

std::string executableDir() {
#ifdef _WIN32
    char buffer[MAX_PATH];
    DWORD len = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    if (len == 0 || len == MAX_PATH) return "./";
    std::string full(buffer, len);
    size_t slash = full.find_last_of("\\/");
    if (slash == std::string::npos) return "./";
    return full.substr(0, slash + 1);
#else
    char buffer[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len <= 0) return "./";
    buffer[len] = '\0';
    std::string full(buffer);
    size_t slash = full.find_last_of('/');
    if (slash == std::string::npos) return "./";
    return full.substr(0, slash + 1);
#endif
}

} // namespace paths
