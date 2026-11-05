/* Optional list presentation of the existing caller-owned menu tree. */
#if PS_MENU_LIST_VIEW
static unsigned menu_list(ps_session *s){
    return (s->menu_style&PS_MENU_LIST)&&(s->io.flags&PS_INTERACTIVE)&&s->io.rows>=12&&s->io.columns>=24;
}
static unsigned list_fold(unsigned c){return c>='A'&&c<='Z'?c+32:c;}
static int list_match(const char *text,const char *query){
    if(!text)return !*query;
    while(*text&&*query){if(list_fold((ps_byte)*text)==list_fold((ps_byte)*query))++query;++text;}
    return !*query;
}
/* Stable-order subsequence filtering; returned indices always address the provider. */
static unsigned list_index(ps_session *s,unsigned ordinal,unsigned *total){
    const ps_menu *m=s->menu_stack[s->menu_depth-1];unsigned i,n=0,result=m->count;
    for(i=0;i<m->count;++i){
        const char *description=s->menu_description?s->menu_description(s->menu_description_context,m,i):0;
        if(list_match(m->items[i].label,s->menu_filter)||list_match(description,s->menu_filter)){
            if(n==ordinal)result=i;
            ++n;
        }
    }
    *total=n;return result;
}
static void list_reset(ps_session *s){s->menu_filter[0]=0;s->menu_filtering=s->menu_filter_pos=s->menu_help=0;}
static void list_draw(ps_session *s){
    const ps_menu *m=s->menu_stack[s->menu_depth-1];unsigned total,i,index,slots=(s->io.rows-8)/3,page,pages,start=0,w=s->io.columns-6;
    list_index(s,0,&total);if(s->menu_selected>=total)s->menu_selected=total?total-1:0;
    page=s->menu_selected/slots;pages=(total+slots-1)/slots;s->menu_top=page*slots;s->menu_slots=slots;
    canvas_begin(s,11,0);ansi(s,"\033[?25l");
    for(i=1;i<=s->io.rows;++i){at(s,i,1);ansi(s,"\033[2K");}
    at(s,1,3);menu_heading(s,w);
    at(s,2,3);if(!menu_notice(s,w)){ink(s,PS_INK_MUTED);ps_number(s,total);out(s," / ");ps_number(s,m->count);out(s," items");reset(s);}
    while(s->menu_filtering&&cells(s->menu_filter+start,s->menu_filter_pos-start,s->io.flags)>=w-8)start=next(s->menu_filter,start);
    at(s,3,3);ink(s,PS_INK_ACTION);out(s,s->menu_filtering?"Filter: ":"Filter  ");field(s,s->menu_filter+start,w-8);reset(s);
    for(i=0;i<slots&&s->menu_top+i<total;++i){
        const ps_menu_item *item;const char *description;unsigned active=s->menu_top+i==s->menu_selected;
        index=list_index(s,s->menu_top+i,&total);item=m->items+index;
        description=s->menu_description?s->menu_description(s->menu_description_context,m,index):0;
        if(!description)description=item->submenu?"Open submenu":(item->flags&PS_MENU_DISABLED)||!item->run?"Unavailable":"";
        at(s,5+i*3,3);ink(s,PS_INK_ACTION);out(s,active?((s->io.flags&PS_UNICODE)?"┃ ":"| "):"  ");
        if(active){surface(s,2);}ink(s,(item->flags&PS_MENU_DISABLED)||(!item->run&&!item->submenu)?PS_INK_INACTIVE:PS_INK_TEXT);
        field(s,item->label,w-2);reset(s);
        at(s,6+i*3,3);ink(s,PS_INK_ACTION);out(s,active?((s->io.flags&PS_UNICODE)?"┃ ":"| "):"  ");
        ink(s,PS_INK_MUTED);field(s,description,w-2);reset(s);
    }
    if(!total){at(s,5,5);ink(s,PS_INK_MUTED);out(s,"No matching items");reset(s);}
    at(s,s->io.rows-3,3);
    if(s->menu_bad){ink(s,PS_INK_WARNING);out(s,"Unavailable selection");}
    else {ink(s,PS_INK_MUTED);out(s,"Page ");ps_number(s,pages?page+1:0);out(s," / ");ps_number(s,pages);
        if(pages<=12){out(s,"  ");for(i=0;i<pages;++i){ink(s,i==page?PS_INK_ACTION:PS_INK_MUTED);out(s,(s->io.flags&PS_UNICODE)?(i==page?"● ":"○ "):(i==page?"* ":". "));}}}
    reset(s);at(s,s->io.rows-1,3);
    footer_field(s,s->menu_filtering?"Enter Accept  Esc Clear  ^U Clear":(s->io.flags&PS_UNICODE)?"↑/↓ Select  Enter Open  / Filter  ? Help":"Up/Down Select  Enter Open  / Filter  ? Help",s->io.columns-4,0);
    at(s,s->io.rows,3);footer_field(s,s->menu_help&&!s->menu_filtering?"PgUp/PgDn Page  Home/End First/last  Esc Back  Ctrl-C Close":"",s->io.columns-4,0);
    if(s->menu_filtering){at(s,3,11+cells(s->menu_filter+start,s->menu_filter_pos-start,s->io.flags));ansi(s,"\033[?25h");}
    frame_end(s);
}
static void list_key(ps_session *s,unsigned key){
    unsigned total,index=list_index(s,s->menu_selected,&total),slots=(s->io.rows-8)/3;
    if(key==3){ps_menu_close(s);prompt(s);return;}
    if(s->menu_filtering){
        unsigned n=length(s->menu_filter);
        if(key==13)s->menu_filtering=0;
        else if(key==27){list_reset(s);}
        else if(key==21){s->menu_filter[0]=0;s->menu_filter_pos=0;}
        else if(key==PS_KEY_LEFT)s->menu_filter_pos=prev(s->menu_filter,s->menu_filter_pos);
        else if(key==PS_KEY_RIGHT){if(s->menu_filter_pos<n)s->menu_filter_pos=next(s->menu_filter,s->menu_filter_pos);}
        else if(key==PS_KEY_HOME)s->menu_filter_pos=0;
        else if(key==PS_KEY_END)s->menu_filter_pos=n;
        else if(key==PS_KEY_DELETE)edit_delete(s->menu_filter,&n,&s->menu_filter_pos,0);
        else if(key==8||key==127)edit_delete(s->menu_filter,&n,&s->menu_filter_pos,1);
        else if(key>=32&&key<256)edit_insert(s->menu_filter,&n,&s->menu_filter_pos,sizeof s->menu_filter,key);
        s->menu_selected=s->menu_top=0;
    }else if(key=='/'){s->menu_filtering=1;s->menu_filter_pos=length(s->menu_filter);}
    else if(key=='?')s->menu_help=!s->menu_help;
    else if(key==27){if(s->menu_filter[0]){list_reset(s);s->menu_selected=0;}else {list_reset(s);ps_menu_choose(s,0);if(!s->menu_depth)prompt(s);return;}}
    else if(key==PS_KEY_UP||key=='k'){if(s->menu_selected)--s->menu_selected;}
    else if(key==PS_KEY_DOWN||key=='j'){if(s->menu_selected+1<total)++s->menu_selected;}
    else if(key==PS_KEY_HOME)s->menu_selected=0;
    else if(key==PS_KEY_END)s->menu_selected=total?total-1:0;
    else if(key==PS_KEY_PAGEDOWN||key==PS_KEY_RIGHT)s->menu_selected=total?min(s->menu_selected+slots,total-1):0;
    else if(key==PS_KEY_PAGEUP||key==PS_KEY_LEFT)s->menu_selected=s->menu_selected>slots?s->menu_selected-slots:0;
    else if(key==13){if(index>=s->menu_stack[s->menu_depth-1]->count||ps_menu_choose(s,index+1)==2)s->menu_bad=1;
        if(!s->menu_depth){prompt(s);return;}}
    if(key!=13)s->menu_bad=0;
    list_draw(s);
}
#else

static void list_reset(ps_session *s){(void)s;}
#endif
