#ifndef ULTIMATUM_WEB_MENU_H
#define ULTIMATUM_WEB_MENU_H

bool webMenuAvailable();
bool webDebugToolsEnabled();
void webOpenMenu();
int webStorageRevision();
int webLibraryRequest();
int webAdventureRequest();
int webSaveRevision();
int webSaveCheckpoint(bool automatic = false);
bool webSaveAutomatic();
int webRecoveryDownloadRequest();
const unsigned char *webRecoveryZip();
int webRecoveryZipSize();
bool webApplyGraphics(int videoType);
void webShowGemMap();
const unsigned char *webGemMapPixels();
int webGemMapWidth();
int webGemMapHeight();

#endif
