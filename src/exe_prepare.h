/* Header validation and safe KWAJ normalization, shared by host tests/Vita. */
#ifndef CT_EXE_PREPARE_H
#define CT_EXE_PREPARE_H
/* Returns 0 for valid MZ/NE input, 1 if expanded to output, -1 on failure.
 * A compressed input at output is preserved as output.kwaj.bak[.N]. */
int ct_prepare_exe(const char *input, const char *output, void (*notice)(const char *));
#endif
