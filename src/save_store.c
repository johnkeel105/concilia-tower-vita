#include "save_store.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#ifdef __vita__
#include <psp2/io/dirent.h>
#else
#include <dirent.h>
#endif
int CT_SaveDirectoryCreate(const char *root)
{
    if(mkdir(root,0777)==0) return 0;
    struct stat st;return errno==EEXIST && stat(root,&st)==0 && S_ISDIR(st.st_mode)?0:-1;
}
int CT_SavePathBuild(const char *root,const char *name,char *out,size_t size)
{
    if(!root || !*root || !name || !*name || name[0]=='.' || strstr(name,"..")) return -1;
    for(const unsigned char *p=(const unsigned char*)name;*p;p++)
        if(!isalnum(*p) && *p!='_' && *p!='-' && *p!='.') return -1;
    int n=snprintf(out,size,"%s/%s",root,name);return n>=0 && (size_t)n<size?0:-1;
}
int CT_SavePathWithin(const char *root,const char *path)
{
    if(!root || !path) return 0;
    size_t n=strlen(root);
    if(strncmp(path,root,n) || path[n]!='/') return 0;
    char check[512];return CT_SavePathBuild(root,path+n+1,check,sizeof(check))==0 && !strcmp(check,path);
}
static int sav_name(const char *s)
{ size_t n=strlen(s);return n>4 && !strcmp(s+n-4,".sav"); }
int CT_SaveList(const char *root,char names[][CT_SAVE_NAME_MAX],int capacity)
{
    int count=0;
#ifdef __vita__
    SceUID dir=sceIoDopen(root);if(dir<0) return -1;
    SceIoDirent ent;memset(&ent,0,sizeof(ent));
    while(sceIoDread(dir,&ent)>0) {
        char path[512];
        if(count<capacity && sav_name(ent.d_name) && strlen(ent.d_name)<CT_SAVE_NAME_MAX &&
           CT_SavePathBuild(root,ent.d_name,path,sizeof(path))==0 && !(ent.d_stat.st_mode&SCE_S_IFDIR))
            strcpy(names[count++],ent.d_name);
        memset(&ent,0,sizeof(ent));
    }
    sceIoDclose(dir);
#else
    DIR *dir=opendir(root);if(!dir) return -1;struct dirent *ent;
    while((ent=readdir(dir))) {
        char path[512];struct stat st;
        if(count<capacity && sav_name(ent->d_name) && strlen(ent->d_name)<CT_SAVE_NAME_MAX &&
           CT_SavePathBuild(root,ent->d_name,path,sizeof(path))==0 && stat(path,&st)==0 && S_ISREG(st.st_mode))
            strcpy(names[count++],ent->d_name);
    }
    closedir(dir);
#endif
    for(int i=0;i<count;i++) for(int j=i+1;j<count;j++) if(strcmp(names[i],names[j])>0) {
        char tmp[CT_SAVE_NAME_MAX];strcpy(tmp,names[i]);strcpy(names[i],names[j]);strcpy(names[j],tmp);
    }
    return count;
}
