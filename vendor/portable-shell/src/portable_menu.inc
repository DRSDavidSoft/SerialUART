/* Shared framed workspace primitives; exact cell widths, no terminal-edge wrap. */
static void view_color(ps_session *s,const char *code){if(s->io.flags&PS_COLOR)ansi(s,code);}
static void view_selection(ps_session *s){if(s->io.flags&PS_COLOR)view_color(s,"\033[30;43m");else ansi(s,"\033[7m");}
static void view_rule_color(ps_session *s,unsigned y,unsigned x,unsigned w,int bottom,const char *code){
    unsigned i;at(s,y,x);view_color(s,code);out(s,(s->io.flags&PS_UNICODE)?(bottom?"╰":"╭"):"+");
    for(i=0;i<w-2;++i)out(s,(s->io.flags&PS_UNICODE)?"─":"-");
    out(s,(s->io.flags&PS_UNICODE)?(bottom?"╯":"╮"):"+");reset(s);
}
static void view_rule(ps_session *s,unsigned y,unsigned x,unsigned w,int bottom){view_rule_color(s,y,x,w,bottom,"\033[31m");}
static void view_side(ps_session *s){view_color(s,"\033[31m");out(s,(s->io.flags&PS_UNICODE)?"│":"|");reset(s);}
static void menu_item_style(ps_session *s,const ps_menu_item *item,int selected){
    unsigned unavailable=(item->flags&PS_MENU_DISABLED)||(!item->run&&!item->submenu);
    if(selected&&!(s->menu_style&PS_MENU_MARKER))view_selection(s);
    else ink(s,unavailable?PS_INK_INACTIVE:selected?PS_INK_ACTION:PS_INK_TEXT);
}
static void menu_heading(ps_session *s,unsigned width_){
#if PS_MENU_CONTEXT
    unsigned i,n;
    if(s->menu_style&PS_MENU_BREADCRUMBS)for(i=0;i+1<s->menu_depth&&width_>6;++i){
        const char *title=s->menu_stack[i]->title;
        n=min(cells(title,length(title),s->io.flags),width_/3);
        ink(s,PS_INK_MUTED);field(s,title,n);out(s,(s->io.flags&PS_UNICODE)?" › ":" > ");width_-=n+3;
    }
#endif
    ink(s,PS_INK_HEADING);field(s,s->menu_stack[s->menu_depth-1]->title,width_);reset(s);
}
static int menu_notice(ps_session *s,unsigned width_){
#if PS_MENU_CONTEXT
    unsigned role=PS_INK_MUTED;
    const char *text=s->menu_notice?s->menu_notice(s->menu_notice_context,s->menu_stack[s->menu_depth-1],&role):0;
    if(text&&*text){ink(s,role);field(s,text,width_);reset(s);return 1;}
#else
    (void)s;(void)width_;
#endif
    return 0;
}
#include "portable_list.inc"
static unsigned menu_framed(ps_session *s){return (s->menu_style&PS_MENU_TABLE)&&(s->io.flags&PS_INTERACTIVE)&&s->io.rows>=8&&s->io.columns>=16;}
static void menu_draw(ps_session *s){
    const ps_menu *m=s->menu_stack[s->menu_depth-1];unsigned i,w=8,x,slots=s->io.rows-6,num=(s->menu_style&PS_MENU_NUMBERS)?5:0;
    unsigned pane=(s->menu_style&PS_MENU_DETAILS)&&s->io.columns>=60&&s->io.rows>=12,limit=pane?s->io.columns/2-3:s->io.columns-2;
    unsigned marker=(s->menu_style&PS_MENU_MARKER)?2:0;
    if(s->menu_selected>=m->count)s->menu_selected=m->count?m->count-1:0;
    for(i=0;i<m->count;++i){unsigned z=cells(m->items[i].label,length(m->items[i].label),s->io.flags)+num+6+marker;if(z>w)w=z;}
    w=min(w,limit);x=pane?2:(s->io.columns-w)/2+1;
    if(s->menu_selected<s->menu_top)s->menu_top=s->menu_selected;
    if(s->menu_selected>=s->menu_top+slots)s->menu_top=s->menu_selected-slots+1;
    if(m->count<slots)slots=m->count;
    s->menu_x=x;s->menu_width=w;s->menu_slots=slots;
    canvas_begin(s,1,w+slots*256+pane*65536);at(s,1,x);menu_heading(s,s->io.columns-x);
    at(s,2,x);menu_notice(s,s->io.columns-x);
    view_rule(s,3,x,w,0);
    if(num){at(s,3,x+6);view_color(s,"\033[31m");out(s,(s->io.flags&PS_UNICODE)?"┬":"+");reset(s);}
    for(i=0;i<slots;++i){
        unsigned idx=s->menu_top+i;const ps_menu_item *item=m->items+idx;char number[8];unsigned n;
        at(s,4+i,x);view_side(s);
        menu_item_style(s,item,idx==s->menu_selected);
        out(s," ");
        if(num){n=dec(number,idx+1);number[n]=0;field(s,number,3);out(s," ");view_side(s);menu_item_style(s,item,idx==s->menu_selected);}
        if(marker){out(s,idx==s->menu_selected?((s->io.flags&PS_UNICODE)?"▸ ":"> "):"  ");}
        field(s,item->label,w-num-6-marker);out(s,item->submenu?((s->io.flags&PS_UNICODE)?" › ":" > "):"   ");reset(s);view_side(s);
    }
    view_rule(s,4+slots,x,w,1);
    if(num){at(s,4+slots,x+6);view_color(s,"\033[31m");out(s,(s->io.flags&PS_UNICODE)?"┴":"+");reset(s);}
    if(pane){
        const char *d=0;unsigned px=x+w+3,pw=s->io.columns-px-1;
        at(s,3,px);view_color(s,"\033[1;33m");field(s,m->count?m->items[s->menu_selected].label:"Empty menu",pw);reset(s);
        if(s->menu_description&&m->count)d=s->menu_description(s->menu_description_context,m,s->menu_selected);
        if(!d)d=m->count&&m->items[s->menu_selected].submenu?"Open submenu":"Run selected action";
        at(s,5,px);field(s,d,pw);
        at(s,7,px);footer_field(s,(s->io.flags&PS_UNICODE)?"↑/↓ Select   Enter Open":"Up/Down Select  Enter Open",pw,0);
        at(s,8,px);footer_field(s,"Esc Back   Ctrl-C Close",pw,0);
    }
    if(!pane){at(s,s->io.rows,1);view_color(s,"\033[33m");
    footer_field(s,(s->io.flags&PS_UNICODE)?"↑/↓ Select  Enter Open  Esc Back  0 Close/back":"Up/Down Select  Enter Open  Esc Back",s->io.columns-1,0);reset(s);}
    if(pane)at(s,10,x+w+3);else at(s,s->io.rows-1,2);out(s,"Select: ");if(s->menu_bad)out(s,"Invalid selection");else raw(s,s->menu_input,s->menu_length);
    ansi(s,"\033[K");frame_end(s);
}
PS_API void ps_menu_set_style(ps_session *s,unsigned flags){if(!s)return;ps_menu_close(s);s->menu_style=flags;}
PS_API void ps_menu_set_description(ps_session *s,ps_menu_description_fn fn,void *ctx){if(s){s->menu_description=fn;s->menu_description_context=ctx;}}
PS_API void ps_menu_set_notice(ps_session *s,ps_menu_notice_fn fn,void *ctx){
#if PS_MENU_CONTEXT
    if(s){s->menu_notice=fn;s->menu_notice_context=ctx;}
#else
    (void)s;(void)fn;(void)ctx;
#endif
}
/* Numbered menus: caller-owned trees, bounded navigation, no OS calls. */
static int menu_valid(const ps_menu *m){
    unsigned i;
    if(!m||!m->title||m->count>999||(m->count&&!m->items))return 0;
    for(i=0;i<m->count;++i)
        if(!m->items[i].label||(m->items[i].submenu&&m->items[i].run))return 0;
    return 1;
}
PS_API unsigned ps_menu_active(const ps_session *s){return s?s->menu_depth:0;}
PS_API void ps_menu_refresh(ps_session *s){
    unsigned i,w;const ps_menu *m;
    if(!s||!s->menu_depth)return;
#if PS_MENU_LIST_VIEW
    if(menu_list(s)){if(!s->menu_screen){ansi(s,"\033[?1049h");s->menu_screen=1;}if(PS_MOUSE)ansi(s,"\033[?1002h\033[?1006h");list_draw(s);return;}
#endif
    if(menu_framed(s)){if(!s->menu_screen){ansi(s,"\033[?1049h");s->menu_screen=1;}if(PS_MOUSE)ansi(s,"\033[?1002h\033[?1006h");s->menu_length=s->menu_bad=0;menu_draw(s);return;}
    m=s->menu_stack[s->menu_depth-1];w=s->io.columns>24?s->io.columns-24:8;
    s->menu_length=s->menu_bad=0;
    ps_newline(s);ink(s,PS_INK_HEADING);field(s,m->title,min(cells(m->title,length(m->title),s->io.flags),min(w,120)));reset(s);ps_newline(s);
    if(menu_notice(s,s->io.columns?s->io.columns-1:79))ps_newline(s);
    for(i=0;i<m->count;++i){
        const ps_menu_item *item=m->items+i;
        out(s,"  ");ink(s,PS_INK_NUMBER);ps_number(s,i+1);out(s,". ");
        ink(s,(item->flags&PS_MENU_DISABLED)||(!item->run&&!item->submenu)?PS_INK_INACTIVE:PS_INK_TEXT);
        field(s,item->label,min(cells(item->label,length(item->label),s->io.flags),min(w,120)));
        if(item->submenu)out(s," >");
        if((item->flags&PS_MENU_DISABLED)||(!item->run&&!item->submenu))out(s," (unavailable)");
        reset(s);ps_newline(s);
    }
    out(s,s->menu_depth>1?"  0. Back":"  0. Close");ps_newline(s);
    if(s->io.flags&PS_INTERACTIVE){ink(s,PS_INK_HEADING);out(s,"> ");reset(s);}
}
PS_API void ps_menu_close(ps_session *s){
    if(!s)return;
    s->canvas_kind=0;
    if(s->menu_screen){if(PS_MOUSE)ansi(s,"\033[?1002l\033[?1006l");ansi(s,"\033[0m\033[?25h\033[?1049l");s->menu_screen=0;}
    s->menu_depth=s->menu_length=s->menu_bad=0;list_reset(s);
}
PS_API int ps_menu_open(ps_session *s,const ps_menu *menu){
    if(!s||ps_form_active(s)||s->exit||s->document_open||!menu_valid(menu))return 0;
    if(s->board)table_close(s);
    ps_file_picker_close(s);ps_menu_close(s);s->menu_selected=s->menu_top=0;
    if(menu_framed(s)){ansi(s,"\033[?1049h");s->menu_screen=1;}
    s->search=0;s->menu_stack[0]=menu;s->menu_depth=1;
    ps_menu_refresh(s);return 1;
}
PS_API int ps_menu_choose(ps_session *s,unsigned number){
    const ps_menu *m;const ps_menu_item *item;
    if(!s||!s->menu_depth)return 2;
    m=s->menu_stack[s->menu_depth-1];
    if(!number){s->menu_selected=s->menu_top=0;list_reset(s);if(--s->menu_depth){
#if PS_MENU_CONTEXT
        s->menu_selected=s->menu_selections[s->menu_depth-1];
#endif
        ps_menu_refresh(s);
    }else ps_menu_close(s);return 0;}
    if(number>m->count)return 2;
    item=m->items+number-1;
    if((item->flags&PS_MENU_DISABLED)||(!item->submenu&&!item->run))return 2;
    if(item->submenu){
        if(s->menu_depth==PS_MENU_DEPTH||!menu_valid(item->submenu))return 2;
#if PS_MENU_CONTEXT
        s->menu_selections[s->menu_depth-1]=(unsigned short)(number-1);
#endif
        s->menu_selected=s->menu_top=0;list_reset(s);s->menu_stack[s->menu_depth++]=item->submenu;ps_menu_refresh(s);return 0;
    }
    ps_menu_close(s);s->status=item->run(s,item->context);return s->status;
}
static void menu_key(ps_session *s,unsigned key){
#if PS_MENU_LIST_VIEW
    if(menu_list(s)){list_key(s,key);return;}
#endif
    if(menu_framed(s)){
        unsigned count=s->menu_stack[s->menu_depth-1]->count;
        if(key=='j')key=PS_KEY_DOWN;else if(key=='k')key=PS_KEY_UP;
        if(key==PS_KEY_UP||key==PS_KEY_DOWN||key==PS_KEY_HOME||key==PS_KEY_END||key==PS_KEY_PAGEUP||key==PS_KEY_PAGEDOWN){
            if(key==PS_KEY_UP&&s->menu_selected)--s->menu_selected;
            if(key==PS_KEY_DOWN&&s->menu_selected+1<count)++s->menu_selected;
            if(key==PS_KEY_HOME)s->menu_selected=0;
            if(key==PS_KEY_END&&count)s->menu_selected=count-1;
            if(key==PS_KEY_PAGEUP)s->menu_selected=s->menu_selected>s->menu_slots?s->menu_selected-s->menu_slots:0;
            if(key==PS_KEY_PAGEDOWN&&count)s->menu_selected=min(s->menu_selected+s->menu_slots,count-1);
            s->menu_length=s->menu_bad=0;menu_draw(s);return;
        }
        if(key==13&&!s->menu_length&&!s->menu_bad){
            if(!count||ps_menu_choose(s,s->menu_selected+1)==2){if(s->menu_depth){s->menu_bad=1;menu_draw(s);}}
            if(!s->menu_depth)prompt(s);
            return;
        }
    }
    if(key==3){ps_newline(s);ps_menu_close(s);prompt(s);return;}
    if(key==27){ps_newline(s);ps_menu_choose(s,0);if(!s->menu_depth)prompt(s);
            return;}
    if(key==8||key==127){
        if(s->menu_bad)s->menu_bad=0;
        else if(s->menu_length){--s->menu_length;if(s->io.flags&PS_INTERACTIVE)out(s,"\b \b");}
        if(menu_framed(s))menu_draw(s);
        return;
    }
    if(key==21){while(s->menu_length)menu_key(s,8);s->menu_bad=0;return;}
    if(key==13){
        unsigned i,n=0,bad=s->menu_bad;
        ps_newline(s);
        if(!s->menu_length&&!bad){ps_menu_refresh(s);return;}
        for(i=0;i<s->menu_length;++i)n=n*10+(unsigned)(s->menu_input[i]-'0');
        s->menu_length=s->menu_bad=0;
        if(bad||ps_menu_choose(s,n)==2){
            if(s->menu_depth){ink(s,PS_INK_WARNING);out(s,"Unavailable selection");reset(s);ps_menu_refresh(s);}
        }
        if(!s->menu_depth)prompt(s);
        return;
    }
    if(key>='0'&&key<='9'){
        if(s->menu_length==3){s->menu_bad=1;return;}
        s->menu_input[s->menu_length++]=(char)key;
        if(menu_framed(s))menu_draw(s);else if(s->io.flags&PS_INTERACTIVE){char c=(char)key;raw(s,&c,1);}
    }else if(key>=32&&key<256)s->menu_bad=1;
}
#if PS_WORKBENCH
#include "../host/workbench/menu.inc"
#endif
