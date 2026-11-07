#include "exe_prepare.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

extern int ct_kwaj_expand(const char *, const char *);

static int valid_ne(FILE *f)
{
    unsigned char h[64], magic[2];
    if (fseek(f, 0, SEEK_END)) return 0;
    long length = ftell(f);
    rewind(f);
    if (length < 64 || fread(h, 1, sizeof(h), f) != sizeof(h) ||
        h[0] != 'M' || h[1] != 'Z') return 0;
    uint32_t off = (uint32_t)h[60] | ((uint32_t)h[61] << 8) |
                   ((uint32_t)h[62] << 16) | ((uint32_t)h[63] << 24);
    if ((uint64_t)off + 64 > (uint64_t)length) return 0;
    if (fseek(f, (long)off, SEEK_SET) || fread(magic, 1, 2, f) != 2) return 0;
    return magic[0] == 'N' && magic[1] == 'E';
}

static int exists(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

int ct_prepare_exe(const char *input, const char *output, void (*notice)(const char *))
{
#define NOTICE(s) do { if (notice) notice(s); } while (0)
    unsigned char magic[4];
    FILE *f = fopen(input, "rb");
    if (!f) { NOTICE("EXE cannot be opened for reading."); return -1; }
    int kwaj = fread(magic, 1, 4, f) == 4 && !memcmp(magic, "KWAJ", 4);
    int valid = kwaj ? 0 : valid_ne(f);
    fclose(f);
    if (!kwaj) {
        NOTICE(valid ? "SIMTOWER.EXE detected: MZ + NE headers valid." :
                       "EXE detected but invalid MZ/NE headers (wrong/truncated file).");
        return valid ? 0 : -1;
    }
    NOTICE("KWAJ-compressed file detected despite .EXE name. Expanding; please wait...");
    char tmp[512], bak[512], message[600];
    if (snprintf(tmp, sizeof(tmp), "%s.tmp", output) >= (int)sizeof(tmp)) return -1;
    int rc = ct_kwaj_expand(input, tmp);
    snprintf(message, sizeof(message), "KWAJ decompressor result=%d", rc);
    NOTICE(message);
    if (rc) { remove(tmp); NOTICE("KWAJ expansion failed. Original file preserved."); return -1; }
    f = fopen(tmp, "rb");
    valid = f && valid_ne(f);
    if (f) fclose(f);
    if (!valid) { remove(tmp); NOTICE("Expanded file has invalid MZ/NE headers. Original preserved."); return -1; }
    int moved = 0;
    if (exists(output)) {
        for (int i = 0; ; i++) {
            int n = i ? snprintf(bak, sizeof(bak), "%s.kwaj.bak.%d", output, i) :
                        snprintf(bak, sizeof(bak), "%s.kwaj.bak", output);
            if (n >= (int)sizeof(bak) || i > 99) { remove(tmp); return -1; }
            if (!exists(bak)) break;
        }
        if (rename(output, bak)) { remove(tmp); NOTICE("Cannot back up original EXE. Check free space/write access."); return -1; }
        moved = 1;
    }
    if (rename(tmp, output)) {
        if (moved && rename(bak, output)) NOTICE("ERROR: rollback failed; original remains in .kwaj.bak file.");
        remove(tmp);
        NOTICE("Cannot install expanded EXE. Original preserved; check write access.");
        return -1;
    }
    if (moved) { snprintf(message, sizeof(message), "Original preserved: %s", bak); NOTICE(message); }
    NOTICE("Expanded SIMTOWER.EXE installed; MZ + NE headers valid.");
    return 1;
#undef NOTICE
}
