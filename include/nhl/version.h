#ifndef NHL_VERSION_H_
#define NHL_VERSION_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NhlVersion {
    unsigned major;
    unsigned minor;
    unsigned patch;
    const char *suffix;
} NhlVersion;

const NhlVersion *nhl_version(void);

const char *nhl_version_string(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* NHL_VERSION_H_ */
