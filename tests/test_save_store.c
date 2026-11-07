#define _GNU_SOURCE
#include "save_store.h"
#include "game.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
int main(void) {
 char root[]="/tmp/ct-save-store-XXXXXX",path[512],names[8][CT_SAVE_NAME_MAX];
 assert(mkdtemp(root));assert(CT_SaveDirectoryCreate(root)==0);
 const char *bad[]={"../bad.sav","app0:bad.sav","/tmp/bad.sav","foo/bar.sav",".hidden","bad..sav",""};
 for(size_t i=0;i<sizeof(bad)/sizeof(*bad);i++) assert(CT_SavePathBuild(root,bad[i],path,sizeof(path))<0);
 assert(CT_SavePathBuild(root,"tower-01.sav",path,sizeof(path))==0);
 assert(CT_SavePathWithin(root,path));assert(!CT_SavePathWithin(root,"relative.sav"));
 assert(!CT_SavePathWithin(root,"app0:relative.sav"));assert(!CT_SavePathWithin(root,"/"));
 Tower *tw=calloc(1,sizeof(*tw));GameSim *sim=calloc(1,sizeof(*sim));assert(tw&&sim);
 tower_init(tw);game_init(sim);tw->money=1234567;sim->tick=1234;TUNING.capacity_express=7;
 assert(game_save(sim,tw,path)==0);tw->money=7654321;assert(game_save(sim,tw,path)==0);
 tw->money=0;sim->tick=0;tuning_reset();assert(game_load(sim,tw,path)==0);
 assert(tw->money==7654321 && sim->tick==1234 && TUNING.capacity_express==7);
 assert(CT_SaveList(root,names,8)==1 && !strcmp(names[0],"tower-01.sav"));
 assert(game_load(sim,tw,"/no/such/file.sav")<0);
 unlink(path);char bak[520];for(int i=1;i<=3;i++) {snprintf(bak,sizeof(bak),"%s.%d",path,i);unlink(bak);}
 rmdir(root);free(sim);free(tw);puts("save-store: safe absolute paths, listing, backups, real save/load roundtrip OK");
}
