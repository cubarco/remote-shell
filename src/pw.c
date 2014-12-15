/**
 * Printf wrapper
 */
#include <stdio.h>

#define COL             "\033[0m"
#define COL_BRED        "\033[1;31m"
#define COL_BGREEN      "\033[1;32m"
#define COL_BBROWN      "\033[2;33m"
#define COL_BBLUE       "\033[1;34m"
#define COL_BPURPLE     "\033[1;35m"
#define COL_BCYAN       "\033[1;36m"
#define COL_BWHITE      "\033[1;37m"

/**
 * Debug
 */
#ifdef DEBUG
        #define dprintf(format, args...) do { \
                printf(COL_BBLUE "[DEBUG] " COL); \
                printf(format, ## args); \
        } while (0)
#else
        #define dprintf(format, args...)
#endif

/**
 * Information
 */
#define iprintf(format, args...) do { \
            printf(COL_BGREEN "[INFO] " COL ); \
            printf(format, ## args); \
        } while (0)

/**
 * Warning
 */
#define wprintf(format, args...) do { \
            printf(COL_BBROWN "[WARNING] " COL); \
            printf(format, ## args); \
        } while (0)

/**
 * Error
 */
#define eprintf(format, args...) do { \
            fprintf(stderr, COL_BPURPLE "[ERROR] " COL); \
            fprintf(stderr, format, ## args); \
        } while (0)
