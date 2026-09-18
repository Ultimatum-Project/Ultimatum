#ifndef ZU4_CLOUD_ACCOUNTS_H
#define ZU4_CLOUD_ACCOUNTS_H
#include "adventure_transfer.h"
#ifdef __cplusplus
extern "C" {
#endif
// Input is a bounded snapshot of saved slots. Download returns a portable package
// and destination token to the paused engine for validation/atomic publication.
void zu4_cloud_accounts_show(const char *slotsJSON, size_t count, Zu4AdventureTransferResult result, void *context);
void zu4_cloud_accounts_sync(const char *slotsJSON, size_t count, Zu4AdventureTransferResult result, void *context);
int zu4_cloud_accounts_is_visible(void);
#ifdef __cplusplus
}
#endif
#endif
