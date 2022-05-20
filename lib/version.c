#include <nhl/version.h>

#include <stdio.h>

#define NHL_VERSION_MAJOR 0
#define NHL_VERSION_MINOR 0
#define NHL_VERSION_PATCH 1
#define NHL_VERSION_SUFFIX ""

const NhlVersion *nhl_version(void) {
    static const NhlVersion version = {
        NHL_VERSION_MAJOR,
        NHL_VERSION_MINOR,
        NHL_VERSION_PATCH,
        NHL_VERSION_SUFFIX
    };
    return &version;
}

const char *nhl_version_string(void) {
    static char version[3*9 + sizeof(NHL_VERSION_SUFFIX)] = {'\0'};
    if (!*version) {
        const NhlVersion *ver = nhl_version();
        sprintf(version, "%u.%u.%u%s", ver->major, ver->major, ver->patch, ver->suffix);
    }
    return version;
}
