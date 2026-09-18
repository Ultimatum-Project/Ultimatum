#ifndef ZU4_ADVENTURE_TRANSFER_H
#define ZU4_ADVENTURE_TRANSFER_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
// outcome: 1 selected/shared, 0 cancelled, -1 error. Bytes live until callback
// returns. The waiting engine copies them before UIKit releases the document.
typedef void (*Zu4AdventureTransferResult)(int outcome, const unsigned char *bytes,
                                         size_t count, const char *error, void *context);
void zu4_adventure_import_show(Zu4AdventureTransferResult result, void *context);
void zu4_adventure_export_show(const char *json, size_t count, int slot,
                              Zu4AdventureTransferResult result, void *context);
int zu4_adventure_transfer_is_visible(void);
#ifdef __cplusplus
}
#endif
#endif
