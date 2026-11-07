/* Host smoke harness. Supply your own input EXE and writable output path.
 * gcc -Isrc -Isrc/vendor/mspack tests/test_exe_boot.c src/exe_prepare.c
 *   src/ne_resource.c src/kwaj_expand.c src/vendor/mspack/{kwajd,mszipd,lzssd,system}.c
 *   -o /tmp/ct-exe-boot-test
 */
#include "exe_prepare.h"
#include "ne_resource.h"
#include <stdio.h>
#include <stdlib.h>
static void notice(const char *s) { puts(s); }
int main(int argc, char **argv)
{
    if (argc != 3) return 2;
    int rc = ct_prepare_exe(argv[1], argv[2], notice);
    if (rc < 0) return 1;
    NEResourceTable table;
    if (ne_load(&table, rc ? argv[2] : argv[1])) return 1;
    int total = 0;
    for (int i = 0; i < table.type_count; i++) total += table.types[i].count;
    printf("PASS: prepared=%d types=%d resources=%d\n", rc, table.type_count, total);
    ne_free(&table);
    return 0;
}
