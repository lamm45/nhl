#ifndef NHL_MEM_H_
#define NHL_MEM_H_

/* Maximum length (without the terminating null) of a string representation of an int.
 * See, e.g., https://stackoverflow.com/a/32871108 */
#define NHL_INTSTR_LEN ((CHAR_BIT * sizeof(int) - 1) * 10/33 + 2)

/* ANSI C compatible version of strdup(). Input must be null-terminated. Release with free(). */
char *nhl_copy_string(const char *str);

#endif /* NHL_MEM_H_ */
