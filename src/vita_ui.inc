/* Vita-only focus registry: uses the actual click-handler geometry, not a
 * separate scaled coordinate system. No allocation/upload in this path. */
static SDL_Rect ui_targets[256];
static int ui_target_action[256],ui_count,ui_surface,ui_last_surface,ui_focus_index;
static int ui_mouse_x=-1,ui_mouse_y=-1;
static void handle_event(SDL_Event *);
static void handle_event_native(SDL_Event *);
static void ui_add(SDL_Rect r,int action)
{
    if(ui_count>=256 || r.w<=0 || r.h<=0 || r.x<0 || r.y<0 || r.x+r.w>960 || r.y+r.h>544) return;
    ui_targets[ui_count]=r;ui_target_action[ui_count++]=action;
}
static void ui_box(int x,int y,int w,int h) { ui_add((SDL_Rect){x,y,w,h},0); }
static SDL_Rect ui_panel_rect(CT_UIPanel p,SDL_Rect r)
{
    int x=p.dst.x+(int)ceil((r.x-p.src.x)*(double)p.dst.w/p.src.w-0.5);
    int y=p.dst.y+(int)ceil((r.y-p.src.y)*(double)p.dst.h/p.src.h-0.5);
    int right=p.dst.x+(int)ceil((r.x+r.w-p.src.x)*(double)p.dst.w/p.src.w-0.5);
    int bottom=p.dst.y+(int)ceil((r.y+r.h-p.src.y)*(double)p.dst.h/p.src.h-0.5);
    return (SDL_Rect){x,y,right-x,bottom-y};
}
static int vita_transient_ui(void)
{
    return manual_open || save_dialog_mode || game.new_tower_confirm || game.quit_confirm || game.route_confirm || game.notice_modal || game.disaster_modal ||
        game.name_edit_open || game.movie_dlg_open || game.find_open || game.menu_open>=0 || game.tool_popup>=0 ||
        game.person_open || game.inspect_open || game.fin_open || game.elv_open || game.show_masspricing || game.show_tuning || game.show_stats;
}
static int vita_ui_collect(void)
{
    ui_count=0;ui_surface=0;int x,y;
    if(manual_open) {
        ui_surface=200+manual_section;
        for(int i=0;i<MANUAL_BTN_COUNT;i++) ui_add(manual_button(i),0);
    } else if(save_dialog_mode) {
        ui_surface=100+save_dialog_mode+save_dialog_page*3;SDL_Rect r=save_dialog_rect();
        for(int row=0;row<8 && save_dialog_page*8+row<save_dialog_count;row++) ui_box(r.x+14,r.y+72+row*29,r.w-28,26);
        if(save_dialog_page) ui_box(r.x+14,r.y+338,76,26);
        if((save_dialog_page+1)*8<save_dialog_count) ui_box(r.x+104,r.y+338,64,26);
        ui_box(r.x+r.w-94,r.y+338,80,26);
    } else if(game.new_tower_confirm) { ui_surface=11;for(int i=0;i<3;i++) ui_add(newt_btn(i),0); }
    else if(game.quit_confirm) { ui_surface=1;for(int i=0;i<3;i++) ui_add(quitc_btn(i),0); }
    else if(game.route_confirm) { ui_surface=2;for(int i=0;i<2;i++) ui_add(route_btn_rect(i),0); }
    else if(game.notice_modal) { ui_surface=3;ui_add(notice_btn_rect(),0); }
    else if(game.disaster_modal) { ui_surface=4;for(int i=0;i<2;i++) ui_add(disaster_btn_rect(i),0); }
    else if(game.name_edit_open) { ui_surface=5;SDL_Rect r=name_dlg_rect();ui_add(name_dlg_ok(r),0);ui_add(name_dlg_del(r),0);ui_add(name_dlg_cancel(r),0);ui_add(name_dlg_edit(r),0); }
    else if(game.movie_dlg_open) {
        ui_surface=6;SDL_Rect r=movie_dlg_rect();
        if(game.tower.money>=MOVIE_COST_HIT) ui_add(movie_btn_hit(r),0);
        if(game.tower.money>=MOVIE_COST_ORDINARY) ui_add(movie_btn_ord(r),0);
        ui_add(movie_btn_cancel(r),0);
    } else if(game.find_open) {
        ui_surface=7;FindRow rows[FIND_ROWS];int n=find_build_rows(rows);SDL_Rect r;find_window_rect(n,&r);
        int ly=r.y+WIN_TITLEBAR_H+4,by=ly+(n?n:1)*FIND_ROW_H+10;
        ui_box(r.x+8,by,56,20);
        if(game.find_sel>=0 && game.find_sel<n) { ui_box(r.x+72,by,70,20);ui_box(r.x+150,by,60,20); }
        for(int i=0;i<n;i++) ui_box(r.x+7,ly+1+i*FIND_ROW_H,r.w-14,FIND_ROW_H);
    } else if(game.menu_open>=0) {
        ui_surface=20+game.menu_open;int w,h;get_top_menu_rect(game.menu_open,&x,&y,&w,&h);y+=MENU_BAR_H+2;
        const TopMenu *m=&top_menus[game.menu_open];
        for(int i=0;i<m->count;i++) { if(m->items[i].label && (m->items[i].build_type==ITEM_NONE || item_unlocked(m->items[i].build_type))) ui_box(x+2,y,176,18);y+=m->items[i].label?18:6; }
    } else if(game.tool_popup>=0) {
        ui_surface=50+game.tool_popup;int n=tool_sub_visible_count(game.tool_popup);
        for(int j=0;j<n;j++) {tool_sub_rect(game.tool_popup,j,&x,&y);ui_add(ui_panel_rect(tool_panel(),(SDL_Rect){x,y,32,32}),0);}
    } else if(game.person_open) {
        ui_surface=8;SDL_Rect r=person_popup_rect();ui_add(person_btn_ok(r),0);ui_add(person_btn_name(r),0);
    } else if(game.inspect_open) {
        ui_surface=9;Tenant *t=tower_tenant(&game.tower,game.inspect_tid);
        if(t) {TiLayout L;ti_build(t,&L);x=game.inspect_x;y=game.inspect_y;
#define UIR(r) (SDL_Rect){x+(r).x,y+(r).y,(r).w,(r).h}
            if(game.rent_dd_open) {ui_surface=10;for(int i=0;i<4;i++) ui_add(UIR(L.price_items[i]),0);}
            else {
                ui_add(UIR(L.ok_btn),0);
                if(L.has_rename) ui_add(UIR(L.rename_btn),0);
                if(L.has_newmovie) ui_add(UIR(L.newmovie_btn),0);
                if(L.has_price && !L.price_locked) ui_add(UIR(L.price_box),0);
                int n=L.occ_n<L.occ_cap1?L.occ_n:L.occ_cap1;
                for(int i=0;i<n;i++) ui_box(x+L.occ_row.x+4+i*PSTRIP_PITCH,y+L.occ_row.y+(L.occ_row.h-PSTRIP_H)/2,PSTRIP_PITCH,PSTRIP_H);
                if(L.occ_row2.w>0) for(int i=n;i<L.occ_n;i++) ui_box(x+L.occ_row2.x+4+(i-n)*PSTRIP_PITCH,y+L.occ_row2.y+(L.occ_row2.h-PSTRIP_H)/2,PSTRIP_PITCH,PSTRIP_H);
            }
#undef UIR
        }
    } else if(game.fin_open) { ui_surface=11;ui_add((SDL_Rect){game.fin_x+130,game.fin_y+WIN_TITLEBAR_H+325,94,23},0); }
    else if(game.elv_open) {
        ui_surface=game.elv_mode_pop?13:12;x=game.elv_x;y=game.elv_y+WIN_TITLEBAR_H;
        if(game.elv_mode_pop) {
            int px=SDL_max(4,SDL_min(ELV_PCELL_X0+game.elv_period*ELV_PCELL_PITCH-30,ELV_DLG_W-86));
            for(int i=0;i<3;i++) ui_box(x+px,y+ELV_PCELL_Y+ELV_PCELL_H+2+i*22,82,22);
        } else {
            ui_box(x+115,y+398,80,22);ui_box(x+10,y+398,80,22);
            ui_box(x+29,y+4,70,14);ui_box(x+102,y+4,67,14);
            for(int i=0;i<6;i++) ui_box(x+ELV_PCELL_X0+i*ELV_PCELL_PITCH,y+ELV_PCELL_Y,ELV_PCELL_W,ELV_PCELL_H);
            ui_box(x+64,y+93,12,10);ui_box(x+64,y+103,12,11);ui_box(x+64,y+148,12,10);ui_box(x+64,y+158,12,11);
            ui_box(x+161,y+206,11,10);ui_box(x+161,y+216,11,10);
            int si=elv_dialog_shaft();
            if(si>=0) {ElevatorShaft *s=&game.sim.people.shafts[si];
                if(!game.elv_edit_mode) for(int row=0;row<15;row++) {
                    int f=s->lo+game.elv_scroll+14-row;if(f>s->hi) continue;
                    for(int col=0;col<=s->num_cars && col<9;col++) ui_box(x+18+col*13,y+195+row*13,13,13);
                }
                if(elv_max_scroll(s)>0) for(int i=0;i<15;i++) ui_box(x+152,y+195+i*13,6,13);
                uint16_t ids[10];int n=elv_riders_collect(s,ids,10);
                for(int i=0;i<n;i++) ui_box(x+8+i*PSTRIP_PITCH,y+ELV_DLG_H+5,PSTRIP_PITCH,PSTRIP_H);
            }
        }
    } else if(game.show_masspricing) {
        ui_surface=14;mp_window_origin(&x,&y);int top=y+WIN_TITLEBAR_H;
        ui_box(x+MP_W-78,top+44+MP_ROWS*MP_ROW_H+4,70,20);
        ui_box(x+96,top+4,104,18);ui_box(x+208,top+4,92,18);
        for(int row=0;row<MP_ROWS;row++) for(int col=0;col<4;col++) ui_box(x+120+col*60,top+44+row*MP_ROW_H+2,56,MP_ROW_H-5);
    } else if(game.show_tuning) {
        ui_surface=150+tune_page;tune_window_origin(&x,&y);int top=y+WIN_TITLEBAR_H+22;
        ui_add((SDL_Rect){x+3,y+3,13,12},1);
        for(int row=0;row<tune_visible_rows();row++) {ui_box(x+TUNE_W-52,top+row*TUNE_ROW_H,18,17);ui_box(x+TUNE_W-26,top+row*TUNE_ROW_H,18,17);}
        ui_box(x+8,top+tune_visible_rows()*TUNE_ROW_H+4,142,20);
        if(tune_page>0) ui_box(x+170,top+tune_visible_rows()*TUNE_ROW_H+4,65,20);
        if(tune_first_row()+20<TUNE_ROWS) ui_box(x+250,top+tune_visible_rows()*TUNE_ROW_H+4,65,20);
    } else if(game.show_stats) {ui_surface=16;stats_window_origin(&x,&y);ui_add((SDL_Rect){x+3,y+3,13,12},1);}
    else if(game.win_toolbar && CT_UIPanelHit(tool_panel(),game.mouse_x,game.mouse_y)) {
        ui_surface=17;x=game.tool_x;y=game.tool_y+WIN_TITLEBAR_H;
        ui_add(ui_panel_rect(tool_panel(),(SDL_Rect){x+(TOOL_WIN_W-SPEED_BTN_W)/2,y+8,SPEED_BTN_W,SPEED_BTN_H}),0);
        int tx=x+(TOOL_WIN_W-3*21)/2;
        for(int i=0;i<3;i++) ui_add(ui_panel_rect(tool_panel(),(SDL_Rect){tx+i*23,y+36,21,21}),0);
        for(int i=0;i<TOOL_BTN_COUNT;i++) if(tool_visible_slot(i)>=0) {int bx,by;tool_button_rect(i,&bx,&by);ui_add(ui_panel_rect(tool_panel(),(SDL_Rect){bx,by,32,32}),0);}
        ui_add(ui_panel_rect(tool_panel(),(SDL_Rect){x+3,game.tool_y+3,13,12}),1);
    } else if(game.win_map && point_in_rect(game.mouse_x,game.mouse_y,(SDL_Rect){game.map_x,game.map_y,MAP_WIN_W,MAP_WIN_H})) {
        ui_surface=18;x=game.map_x+4;y=game.map_y+WIN_TITLEBAR_H+4;int mw=MAP_WIN_W-8,mh=MAP_WIN_H-WIN_TITLEBAR_H-26;
        for(int i=0;i<4;i++) if(i<3 || game.tower.star_rating>=2 || game.sim.mode==MODE_SANDBOX) ui_box(x+i*(mw/4),y+mh+2,mw/4-2,MAP_TAB_H);
        ui_add((SDL_Rect){game.map_x+3,game.map_y+3,13,12},1);
    }
    if(ui_surface!=ui_last_surface) {
        ui_focus_index=0;ui_last_surface=ui_surface;
        for(int i=0;i<ui_count;i++) if(point_in_rect(game.mouse_x,game.mouse_y,ui_targets[i])) {ui_focus_index=i;break;}
    }
    if(ui_focus_index>=ui_count) ui_focus_index=0;
    return ui_count>0;
}
static void vita_ui_close_target(void)
{
    if(ui_surface==17) game.win_toolbar=0;
    else if(ui_surface==18) game.win_map=0;
    else vita_cancel_surface();
}
/* Highlight-only focus: D-pad focus and Circle/Cross confirm never move the
 * virtual pointer. A confirm synthesizes hover+click at the focused control,
 * then restores the game's pointer state, so e.g. the Inspect cursor stays
 * exactly where it was when the window opened (v0.14). */
static void vita_ui_click_at(int x,int y)
{
    int ox=game.mouse_x,oy=game.mouse_y;
    SDL_Event e;SDL_zero(e);
    e.type=SDL_MOUSEMOTION;e.motion.x=x;e.motion.y=y;e.motion.xrel=x-ox;e.motion.yrel=y-oy;handle_event_native(&e);
    SDL_zero(e);e.type=SDL_MOUSEBUTTONDOWN;e.button.button=SDL_BUTTON_LEFT;e.button.state=SDL_PRESSED;e.button.clicks=1;e.button.x=x;e.button.y=y;
    handle_event_native(&e);e.type=SDL_MOUSEBUTTONUP;e.button.state=SDL_RELEASED;handle_event_native(&e);
    /* Restore position only (no motion event: that could re-hover/switch a
     * menu under the old pointer). The drawn pointer itself never moved. */
    game.mouse_x=ox;game.mouse_y=oy;game.menu_bar_hover=menu_bar_hit_test(ox,oy);
}
static void vita_ui_confirm(void)
{
    if(!vita_ui_collect()) return;
    if(ui_target_action[ui_focus_index]) { vita_ui_close_target();return; }
    SDL_Rect r=ui_targets[ui_focus_index];
    vita_ui_click_at(r.x+r.w/2,r.y+r.h/2);
}
static void vita_ui_navigate(int dir)
{
    if(!vita_ui_collect()) return;
    if(game.menu_open>=0 && dir<2) {
        game.menu_open=(game.menu_open+(dir==0?-1:1)+TOP_MENU_COUNT)%TOP_MENU_COUNT;game.menu_hover=-1;vita_ui_collect();
    } else ui_focus_index=CT_UIFocusNext(ui_targets,ui_count,ui_focus_index,dir);
    /* No pointer warp: the yellow focus box is the only indicator. */
}
static void vita_ui_focus_draw(void)
{
    if(!vita_ui_collect()) return;
    SDL_Rect r=ui_targets[ui_focus_index];
    SDL_SetRenderDrawColor(game.renderer,255,225,0,255);SDL_RenderDrawRect(game.renderer,&r);
    r.x++;r.y++;r.w-=2;r.h-=2;SDL_SetRenderDrawColor(game.renderer,0,40,150,255);SDL_RenderDrawRect(game.renderer,&r);
}
static void vita_grid_nudge(int dir)
{
    int wx,wy;
    CT_ViewSnapStep(&world_view,game.mouse_x,game.mouse_y,(int)game.cam_fx,(int)game.cam_fy,dir,CELL_W,CELL_H,&wx,&wy);
    wx=SDL_max(CELL_W/2,SDL_min(wx,(TOWER_WIDTH-1)*CELL_W+CELL_W/2));
    wy=SDL_max(-TOWER_TOP_FLOOR*CELL_H+CELL_H/2,SDL_min(wy,-TOWER_MIN_FLOOR*CELL_H+CELL_H/2));
    int x,y;CT_ViewScreenPoint(&world_view,wx,wy,(int)game.cam_fx,(int)game.cam_fy,&x,&y);
    if(x<0) game.cam_fx+=x/world_view.zoom;
    if(x>=960) game.cam_fx+=(x-959)/world_view.zoom;
    if(y<0) game.cam_fy+=y/world_view.zoom;
    if(y>=544) game.cam_fy+=(y-543)/world_view.zoom;
    clamp_camera();CT_ViewScreenPoint(&world_view,wx,wy,(int)game.cam_fx,(int)game.cam_fy,&x,&y);
    vita_pointer_move(x,y,handle_event_native);
}
static void handle_event(SDL_Event *ev)
{
    int active=vita_ui_collect();
    if(ev->type==SDL_KEYDOWN) {
        SDL_Keycode key=ev->key.keysym.sym;
        if(manual_open) {
            if(key==SDLK_F16) {manual_set_section(manual_section-1);return;}
            if(key==SDLK_F17) {manual_set_section(manual_section+1);return;}
            if(key==SDLK_F18) {manual_scroll_by(-1);return;}
            if(key==SDLK_F19) {manual_scroll_by(1);return;}
            if(key==SDLK_F13 || key==SDLK_F14) {manual_close();return;}
        }
        if(key>=SDLK_F16 && key<=SDLK_F19) {if(active) vita_ui_navigate(key-SDLK_F16);else if(game.build_type!=ITEM_NONE) vita_grid_nudge(key-SDLK_F16);return;}
        if(key==SDLK_F13) {if(vita_transient_ui()) vita_ui_confirm();else select_action_tool(1,0);return;}
        if(key==SDLK_F14 && vita_transient_ui()) {vita_cancel_surface();return;}
        if(key==SDLK_RETURN && active) {vita_ui_confirm();return;}
    }
    if(active && (ev->type==SDL_MOUSEMOTION || ev->type==SDL_MOUSEBUTTONDOWN)) {
        int x=ev->type==SDL_MOUSEMOTION?ev->motion.x:ev->button.x,y=ev->type==SDL_MOUSEMOTION?ev->motion.y:ev->button.y;
        if(x!=ui_mouse_x || y!=ui_mouse_y || ev->type==SDL_MOUSEBUTTONDOWN) {
            ui_mouse_x=x;ui_mouse_y=y;
            for(int i=0;i<ui_count;i++) if(point_in_rect(x,y,ui_targets[i])) {
                ui_focus_index=i;
                if(ev->type==SDL_MOUSEBUTTONDOWN && ev->button.button==SDL_BUTTON_LEFT && ui_target_action[i]) {vita_ui_close_target();return;}
                break;
            }
        }
    }
    handle_event_native(ev);
}
