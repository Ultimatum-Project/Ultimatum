#ifndef ZU4_IOS_GAME_DATA_BOOTSTRAP_H
#define ZU4_IOS_GAME_DATA_BOOTSTRAP_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Ensures that the separately distributed Ultima IV DOS data is available.
 * On a clean install this presents a native, accessible download screen and
 * pumps the UIKit run loop until the user completes a verified download.
 */
int zu4_ios_prepare_game_data(void);

/* Returns a malloc-owned JSON description of the verified local data for the
 * account UI, or NULL when no local package is available. */
char *zu4_ios_game_data_entries_json(void);

/* Installs a package already validated by the bundled account UI into an
 * atomic private application-support directory. Returns 1 on success. */
int zu4_ios_install_game_data_package(const char *json, char *error, size_t error_size);

#ifdef __cplusplus
}
#endif

#endif
