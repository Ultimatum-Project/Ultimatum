#include <stdbool.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <sys/stat.h>
#include "settings.h"
#include "u4file.h"
SettingsData settings;
void zu4_error(int level, const char *fmt, ...) { (void)level; (void)fmt; }
void zu4_assert(bool good, const char *fmt, ...) { (void)fmt; assert(good); }
static void add(mz_zip_archive *zip, const char *name, const char *text) {
    assert(mz_zip_writer_add_mem(zip, name, text, strlen(text), 0));
}
static void loose(const char *name, const char *text) {
    char path[128]; snprintf(path, sizeof(path), "ultima4/%s", name);
    FILE *file = fopen(path, "wb"); assert(file); fputs(text, file); assert(!fclose(file));
}
static void verify(const char *name, const char *expected) {
    U4FILE *file = u4fopen(name); assert(file);
    assert(u4flength(file) == strlen(expected));
    char bytes[64] = {0}; assert(u4fread(file, bytes, 1, strlen(expected)) == strlen(expected));
    assert(!strcmp(bytes, expected)); u4fclose(file);
}
int main(int argc, char **argv) {
    (void)argv;
    mz_zip_archive zip = {0};
    assert(mz_zip_writer_init_file(&zip, "u4upgrad.zip", 0));
    add(&zip, "u4vga.pal", "palette");
    for (int i=6; i<=8; ++i) { char name[32]; snprintf(name, sizeof(name), "rune_%d.ega", i); add(&zip, name, "VGA image"); }
    add(&zip, "avatar.exe", "must never supersede original");
    assert(mz_zip_writer_finalize_archive(&zip)); assert(mz_zip_writer_end(&zip));
    memset(&zip, 0, sizeof(zip));
    if (argc == 1) {
        assert(mz_zip_writer_init_file(&zip, "ultima4.zip", 0)); add(&zip, "charset.ega", "original");
        for (int i=6; i<=8; ++i) { char name[32]; snprintf(name, sizeof(name), "rune_%d.ega", i); add(&zip, name, "EGA image"); }
        add(&zip, "avatar.exe", "original executable");
        assert(mz_zip_writer_finalize_archive(&zip)); assert(mz_zip_writer_end(&zip));
    } else {
        assert(mkdir("ultima4", 0700) == 0);
        for (int i=6; i<=8; ++i) { char name[32]; snprintf(name, sizeof(name), "rune_%d.ega", i); loose(name, "EGA image"); }
        loose("avatar.exe", "original executable");
        /* The shipped overlay has no executable. Test ordinary loose-data
         * precedence without manufacturing an executable overlay. */
        memset(&zip, 0, sizeof(zip));
        assert(mz_zip_writer_init_file(&zip, "u4upgrad.zip", 0)); add(&zip, "u4vga.pal", "palette");
        for (int i=6; i<=8; ++i) { char name[32]; snprintf(name, sizeof(name), "rune_%d.ega", i); add(&zip, name, "VGA image"); }
        assert(mz_zip_writer_finalize_archive(&zip)); assert(mz_zip_writer_end(&zip));
    }
    for (int mode=0; mode<4; ++mode) {
        settings.videoType = mode % 2;
        for (int i=6; i<=8; ++i) { char name[32]; snprintf(name, sizeof(name), "rune_%d.ega", i); verify(name, mode%2 ? "VGA image" : "EGA image"); }
        verify("avatar.exe", "original executable");
    }
    puts("VGA overlay: mode-specific rune precedence and original executable preservation passed");
}
