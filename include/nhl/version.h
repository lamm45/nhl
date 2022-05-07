#ifndef NHL_VERSION_H_
#define NHL_VERSION_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Library version. */
typedef struct NhlVersion {
    unsigned major;
    unsigned minor;
    unsigned patch;
    const char *suffix;
} NhlVersion;

/* Return the library version. */
const NhlVersion *nhl_version(void);

/* Return the library version as a string. */
const char *nhl_version_string(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* NHL_VERSION_H_ */
