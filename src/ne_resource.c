/* ne_resource.c - Windows NE executable resource extractor */
#include "ne_resource.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#ifdef __vita__
#include "vita_platform.h"
#define NE_ERROR(...) do { vita_boot_log(__VA_ARGS__); fprintf(stderr, __VA_ARGS__); } while (0)
#define NE_TRACE(...) vita_boot_log(__VA_ARGS__)
#else
#define NE_ERROR(...) fprintf(stderr, __VA_ARGS__)
#define NE_TRACE(...) ((void)0)
#endif

static void list_init(NEResourceList *list)
{
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

static NEResource *list_add(NEResourceList *list)
{
    if (list->count >= list->capacity) {
        int capacity = list->capacity ? list->capacity * 2 : 16;
        NEResource *items = realloc(list->items, sizeof(NEResource) * capacity);
        if (!items) return NULL;
        list->items = items;
        list->capacity = capacity;
    }
    NEResource *r = &list->items[list->count++];
    memset(r, 0, sizeof(*r));
    return r;
}

static NEResourceList *table_get_type(NEResourceTable *table, uint16_t type)
{
    /* Find existing */
    for (int i = 0; i < table->type_count; i++) {
        if (table->type_ids[i] == type)
            return &table->types[i];
    }
    /* Add new */
    if (table->type_count >= table->type_capacity) {
        int capacity = table->type_capacity ? table->type_capacity * 2 : 16;
        NEResourceList *types = realloc(table->types, sizeof(NEResourceList) * capacity);
        if (!types) return NULL;
        table->types = types;
        uint16_t *ids = realloc(table->type_ids, sizeof(uint16_t) * capacity);
        if (!ids) return NULL;
        table->type_ids = ids;
        table->type_capacity = capacity;
    }
    int idx = table->type_count++;
    table->type_ids[idx] = type;
    list_init(&table->types[idx]);
    return &table->types[idx];
}

int ne_load(NEResourceTable *table, const char *path)
{
    memset(table, 0, sizeof(*table));

    FILE *f = fopen(path, "rb");
    if (!f) {
        NE_ERROR("ne_load: cannot open %s\n", path);
        return -1;
    }

    if (fseek(f, 0, SEEK_END) != 0) goto fail;
    long file_size = ftell(f);
    if (file_size < 64) goto fail;
    NE_TRACE("ne_load: %s size=%ld", path, file_size);

    /* MZ magic, then offset to the second-stage header */
    uint16_t mz;
    fseek(f, 0, SEEK_SET);
    if (fread(&mz, 2, 1, f) != 1 || mz != 0x5A4D) {
        NE_ERROR("ne_load: %s is not a Windows executable (no MZ header)\n", path);
        fclose(f);
        return -1;
    }
    uint32_t off_ne;
    fseek(f, 0x3C, SEEK_SET);
    if (fread(&off_ne, 4, 1, f) != 1) goto fail;
    if ((uint64_t)off_ne + 0x40 > (uint64_t)file_size) goto fail;

    /* NE magic — a PE here means a 32-bit program (installer, wrong file) */
    uint16_t ne_magic;
    fseek(f, off_ne, SEEK_SET);
    if (fread(&ne_magic, 2, 1, f) != 1) goto fail;
    if (ne_magic != 0x454E) {
        NE_ERROR("ne_load: %s is not a 16-bit NE executable (magic 0x%04x%s)\n",
                path, ne_magic, ne_magic == 0x4550 ? " = PE/32-bit" : "");
        fclose(f);
        return -1;
    }

    /* Read resource table offset and logical sector alignment shift */
    uint16_t off_rt, lsa;
    fseek(f, off_ne + 0x24, SEEK_SET);
    if (fread(&off_rt, 2, 1, f) != 1) goto fail;
    /* NE resource offsets use the shift stored AT the resource table,
     * not the unrelated segment alignment word at NE+0x32. */
    if ((uint64_t)off_ne + off_rt + 2 > (uint64_t)file_size) goto fail;
    fseek(f, off_ne + off_rt, SEEK_SET);
    if (fread(&lsa, 2, 1, f) != 1) goto fail;
    NE_TRACE("ne_load: NE offset=%lu resource table=%u resource alignment=%u",
             (unsigned long)off_ne, off_rt, lsa);
    if (off_rt == 0 || lsa > 16) {
        NE_ERROR("ne_load: %s has no resource table\n", path);
        fclose(f);
        return -1;
    }
    
    /* Parse resource table */
    fseek(f, off_ne + off_rt + 2, SEEK_SET);

    for (;;) {
        uint16_t type, count;
        if (fread(&type, 2, 1, f) != 1) goto fail;
        if (type == 0) break;
        if (fread(&count, 2, 1, f) != 1) goto fail;
        if ((uint64_t)ftell(f) + 4 + (uint64_t)count * 12 > (uint64_t)file_size) goto fail;


        /* Some SimTower pressings identify the custom resource types by
         * NAME (a string offset — high bit clear) instead of ordinal IDs
         * 0xFF01..0xFF0B (OpenSkyscraper issue #5). Same types, same
         * order; resolve the name back to the canonical ordinal. */
        if (!(type & 0x8000)) {
            static const struct { const char *name; uint16_t id; } named[] = {
                {"ALRT",0xFF01},{"CGPK",0xFF02},{"CLUT",0xFF03},
                {"DTMP",0xFF04},{"PART",0xFF05},{"STRL",0xFF06},
                {"TABL",0xFF07},{"TABM",0xFF08},{"TEXT",0xFF09},
                {"WAVE",0xFF0A},{"YEN",0xFF0B},
            };
            long backup = ftell(f);
            uint8_t nlen; char name[16] = {0};
            fseek(f, off_ne + off_rt + type, SEEK_SET);
            if (fread(&nlen, 1, 1, f) == 1 && nlen < sizeof(name))
                if (fread(name, 1, nlen, f) != nlen) name[0] = 0;
            fseek(f, backup, SEEK_SET);
            uint16_t mapped = 0;
            for (size_t n = 0; n < sizeof(named)/sizeof(named[0]); n++)
                if (strcmp(name, named[n].name) == 0) { mapped = named[n].id; break; }
            if (!mapped) {
                /* Unknown named type: skip its entries entirely. */
                fseek(f, 4 + (long)count * 12, SEEK_CUR);
                continue;
            }
            type = mapped;
        }

        NE_TRACE("ne_load: resource type=0x%04x count=%u", type, count);
        NEResourceList *list = table_get_type(table, type);
        if (!list) goto oom;
        
        /* Skip 4 reserved bytes */
        fseek(f, 4, SEEK_CUR);
        
        for (int i = 0; i < count; i++) {
            uint16_t offset_aligned, length_aligned;
            if (fread(&offset_aligned, 2, 1, f) != 1) goto fail;
            if (fread(&length_aligned, 2, 1, f) != 1) goto fail;
            
            uint32_t offset = (uint32_t)offset_aligned << lsa;
            uint32_t length = (uint32_t)length_aligned << lsa;

            /* Skip 2 bytes (flags) */
            fseek(f, 2, SEEK_CUR);

            uint16_t id;
            if (fread(&id, 2, 1, f) != 1) goto fail;

            /* Skip 4 reserved bytes */
            fseek(f, 4, SEEK_CUR);

            if (offset > INT_MAX || length > INT_MAX ||
                (uint64_t)offset + length > (uint64_t)file_size) {
                NE_ERROR("ne_load: resource type=0x%x id=0x%x runs past EOF "
                        "(corrupt or truncated file)\n", type, id);
                goto fail;
            }
            NEResource *r = list_add(list);
            if (!r) goto oom;
            r->type = type;
            r->id = id;
            r->offset = offset;
            r->length = length;
            r->data = malloc(length ? length : 1);
            if (!r->data) goto oom;
            
            /* Read resource data */
            long backup = ftell(f);
            fseek(f, offset, SEEK_SET);
            if (fread(r->data, 1, length, f) != length) {
                NE_ERROR("ne_load: short read for resource type=0x%x id=0x%x\n", type, id);
                goto fail;
            }
            fseek(f, backup, SEEK_SET);
        }
    }
    
    fclose(f);
    return 0;

oom:
    NE_ERROR("ne_load: allocation failed in %s\n", path);
    goto cleanup;
fail:
    NE_ERROR("ne_load: invalid/truncated resource table in %s\n", path);
cleanup:
    fclose(f);
    ne_free(table);
    return -1;
}

NEResourceList *ne_find_type(NEResourceTable *table, uint16_t type)
{
    for (int i = 0; i < table->type_count; i++) {
        if (table->type_ids[i] == type)
            return &table->types[i];
    }
    return NULL;
}

NEResource *ne_find(NEResourceTable *table, uint16_t type, uint16_t id)
{
    NEResourceList *list = ne_find_type(table, type);
    if (!list) return NULL;
    for (int i = 0; i < list->count; i++) {
        if (list->items[i].id == id)
            return &list->items[i];
    }
    return NULL;
}

void ne_free(NEResourceTable *table)
{
    for (int i = 0; i < table->type_count; i++) {
        NEResourceList *list = &table->types[i];
        for (int j = 0; j < list->count; j++) {
            free(list->items[j].data);
        }
        free(list->items);
    }
    free(table->types);
    free(table->type_ids);
    memset(table, 0, sizeof(*table));
}
