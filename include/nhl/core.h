#ifndef NHL_CORE_H_
#define NHL_CORE_H_

#ifdef __cplusplus
extern "C" {
#endif


/* Main handle. This is an opaque type which can be initialized by nhl_init(). */
typedef struct Nhl Nhl;

/* Initialization parameters. */
typedef struct NhlInitParams {
    /* Path to the (possibly non-existing) cache file. Must reside in a writable directory.
     * Can also be NULL, in which case file cache is disabled. */
    char *cache_file;

    /* If nonzero, no contents are read from external sources except from the cache. */
    int offline;

    /* If nonzero, diagnostic messages can be printed to standard output and standard error. */
    int verbose;

    /* Maximum age (in seconds) of various content types in cache. */
    int schedule_max_age;
    int game_live_max_age;
    int game_final_max_age;
    int team_max_age;
    int player_max_age;
    int league_max_age;
    int meta_max_age;

    /* CURRENTLY NOT USED. */
    char *dump_folder;
} NhlInitParams;

/* Assign default values to the param object. */
void nhl_default_params(NhlInitParams *params);

/* Return a newly initialized handle. If params is NULL, default parameters are assumed. Otherwise,
 * the handle takes ownership of the members in params. */
Nhl *nhl_init(const NhlInitParams *params);

/* Re-initialize an existing handle with (possibly) new parameters. */
void nhl_reset(Nhl *nhl, const NhlInitParams *params);

/* Release resources acquired by nhl_init() or nhl_reset().
 * The handle must not be used after call to this. */
void nhl_close(Nhl *nhl);


/* Return values used by various functions can be combinations of these. */
typedef enum NhlStatus {
    NHL_DOWNLOAD_OK          = 1 << 0 ,
    NHL_DOWNLOAD_SKIPPED     = 1 << 1 ,
    NHL_DOWNLOAD_ERROR       = 1 << 2 ,
    NHL_CACHE_READ_OK        = 1 << 3 ,
    NHL_CACHE_READ_EXPIRED   = 1 << 4 ,
    NHL_CACHE_READ_NOT_FOUND = 1 << 5 ,
    NHL_CACHE_READ_ERROR     = 1 << 6 ,
    NHL_CACHE_WRITE_OK       = 1 << 7 ,
    NHL_CACHE_WRITE_ERROR    = 1 << 8 ,
    NHL_INVALID_REQUEST      = 1 << 9
} NhlStatus;

/* Query level defines the amount of recursion in various function calls. */
typedef enum NhlQueryLevel {
    NHL_QUERY_MINIMAL     = 0 ,
    NHL_QUERY_BASIC       = 1 << 0 ,
    NHL_QUERY_GAMEDETAILS = 1 << 1 ,
    NHL_QUERY_GOALS       = 1 << 2 ,
    NHL_QUERY_PLAYERS     = 1 << 3 ,
    NHL_QUERY_FULL        = 0xFFFF
} NhlQueryLevel;

/* Open a transaction and return an integer that must be given when the transaction is closed by
 * nhl_finish(). Manually opening transactions is never necessary, but may increase pefrormance if
 * multiple calls for data extraction functions are made between calls to nhl_prepare() and
 * nhl_finish(). */
int nhl_prepare(Nhl *nhl);

/* Finish a transaction. */
void nhl_finish(Nhl *nhl, int start);


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* NHL_CORE_H_ */
