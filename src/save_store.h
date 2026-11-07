#ifndef CT_SAVE_STORE_H
#define CT_SAVE_STORE_H
#include <stddef.h>
#define CT_VITA_SAVE_DIR "ux0:data/concilia-tower"
#define CT_SAVE_NAME_MAX 96
int CT_SaveDirectoryCreate(const char *root);
int CT_SavePathBuild(const char *root,const char *name,char *out,size_t size);
int CT_SavePathWithin(const char *root,const char *path);
int CT_SaveList(const char *root,char names[][CT_SAVE_NAME_MAX],int capacity);
#endif
