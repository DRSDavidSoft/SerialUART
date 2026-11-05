/* Provider-owned, lazily populated tree; no filesystem calls in the core. */
PS_API unsigned ps_file_picker_active(const ps_session *s){return s?s->file_count:0;}
PS_API void ps_file_picker_close(ps_session *s){
    if(s&&s->file_count){s->file_count=0;if(PS_MOUSE)ansi(s,"\033[?1002l\033[?1006l");ansi(s,"\033[0m\033[?25h\033[?1049l");}
}
static unsigned file_split(ps_session *s){return s->file_layout==PS_FILE_SPLIT&&s->io.columns>=56;}
static unsigned file_directory_only(ps_session *s){return s->file_layout==PS_FILE_SPLIT;}
static int file_valid(const ps_file_node *nodes,unsigned count){
    unsigned i;if(!nodes||!count||count>128)return 0;
    for(i=0;i<count;++i){unsigned p=nodes[i].parent,depth=0;
        if(!nodes[i].name||!nodes[i].name[0])return 0;
        if(p!=PS_FILE_ROOT&&(p>=i||!(nodes[p].flags&PS_FILE_DIRECTORY)))return 0;
        while(p!=PS_FILE_ROOT){if(++depth>16)return 0;p=nodes[p].parent;}
    }return 1;
}
static void file_load(ps_session *s,unsigned id){
    unsigned n;
    if(id>=s->file_count||!(s->file_nodes[id].flags&PS_FILE_DIRECTORY)||s->file_nodes[id].flags&PS_FILE_DISABLED)return;
    if(s->file_loaded[id]==2){s->file_notice="Directory unavailable";return;}
    if(s->file_load&&!s->file_loaded[id]){
        n=s->file_load(s->file_context,id,s->file_count);
        if(n<s->file_count||!file_valid(s->file_nodes,n)){s->file_notice="Directory unavailable";s->file_loaded[id]=2;return;}
        s->file_count=n;s->file_loaded[id]=1;
        if(n==128)s->file_notice="128-node snapshot limit; reopen a narrower path";
    }
}
static void file_visit(ps_session *s,unsigned parent,unsigned depth,unsigned *ids,unsigned *depths,unsigned *count){
    unsigned i;for(i=0;i<s->file_count;++i)if(s->file_nodes[i].parent==parent){
        if(file_directory_only(s)&&!(s->file_nodes[i].flags&PS_FILE_DIRECTORY))continue;
        ids[*count]=i;depths[(*count)++]=depth;
        if(s->file_expanded[i])file_visit(s,i,depth+1,ids,depths,count);
    }
}
static unsigned file_visible(ps_session *s,unsigned *ids,unsigned *depths){unsigned count=0;file_visit(s,PS_FILE_ROOT,0,ids,depths,&count);return count;}
static unsigned file_listing(ps_session *s,unsigned *ids){unsigned i,n=0;for(i=0;i<s->file_count;++i)if(s->file_nodes[i].parent==s->file_directory)ids[n++]=i;return n;}
static unsigned file_last(ps_session *s,unsigned id){unsigned j;for(j=id+1;j<s->file_count;++j)if(s->file_nodes[j].parent==s->file_nodes[id].parent&&(!file_directory_only(s)||(s->file_nodes[j].flags&PS_FILE_DIRECTORY)))return 0;return 1;}
static unsigned file_icon_width(ps_session *s){return (s->io.flags&PS_UNICODE)&&s->file_icons==PS_FILE_ICONS_EMOJI?3:2;}
static const char *file_icon(ps_session *s,unsigned id){
    unsigned dir=s->file_nodes[id].flags&PS_FILE_DIRECTORY,link=s->file_nodes[id].flags&PS_FILE_LINK;
    if(!(s->io.flags&PS_UNICODE)||s->file_icons==PS_FILE_ICONS_ASCII)return link?"@ ":dir?(s->file_expanded[id]?"- ":"+ "):". ";
    if(s->file_icons==PS_FILE_ICONS_NERD)return link?"\xef\x83\x81 ":dir?(s->file_expanded[id]?"\xef\x81\xbc ":"\xef\x81\xbb "):"\xef\x85\x9b ";
    if(s->file_icons==PS_FILE_ICONS_EMOJI)return link?"🔗 ":dir?(s->file_expanded[id]?"📂 ":"📁 "):"📄 ";
    return link?"↗ ":dir?(s->file_expanded[id]?"▾ ":"▸ "):"▪ ";
}
static void file_focus_directory(ps_session *s,unsigned id){
    if(id<s->file_count&&(s->file_nodes[id].flags&PS_FILE_DIRECTORY)){
        if(s->file_directory!=id){s->file_directory=id;s->file_list_selected=s->file_list_top=0;if(s->file_count<128)s->file_notice=0;}
        file_load(s,id);
    }
}
static void file_row(ps_session *s,unsigned id,unsigned depth,unsigned interior,int selected,int tree){
    unsigned icon=file_icon_width(s),indent=min(depth*2,interior>icon?interior-icon:0),j;
    if(selected==2)view_color(s,"\033[37;40m");else if(selected)view_selection(s);else view_color(s,s->file_nodes[id].flags&PS_FILE_DISABLED?"\033[90m":s->file_nodes[id].flags&PS_FILE_DIRECTORY?"\033[33m":"\033[37m");
    for(j=0;j<indent/2;++j){unsigned ancestor=id,k=depth-j-1;while(k--)ancestor=s->file_nodes[ancestor].parent;
        if(tree&&j+1==depth)out(s,(s->io.flags&PS_UNICODE)?(file_last(s,ancestor)?"└─":"├─"):(file_last(s,ancestor)?"`-":"+-"));
        else out(s,!tree||file_last(s,ancestor)?"  ":(s->io.flags&PS_UNICODE)?"│ ":"| ");
    }
    if(indent&1)out(s," ");
    out(s,file_icon(s,id));field(s,s->file_nodes[id].name,interior-indent-icon);reset(s);
}
static void file_render(ps_session *s){
    unsigned ids[128],depths[128],list[128],count=file_visible(s,ids,depths),ln,i,slots=s->io.rows>=7?s->io.rows-6:1,w=s->io.columns-1;
    unsigned split=file_split(s),listing=s->file_layout==PS_FILE_LIST||(s->file_layout==PS_FILE_SPLIT&&!split),left=split?w*2/5:w;
    if(s->file_selected>=count)s->file_selected=count?count-1:0;
    if(split&&count)file_focus_directory(s,ids[s->file_selected]);
    ln=file_listing(s,list);if(s->file_list_selected>=ln)s->file_list_selected=ln?ln-1:0;
    if(s->file_selected<s->file_top)s->file_top=s->file_selected;
    if(s->file_selected>=s->file_top+slots)s->file_top=s->file_selected-slots+1;
    if(s->file_list_selected<s->file_list_top)s->file_list_top=s->file_list_selected;
    if(s->file_list_selected>=s->file_list_top+slots)s->file_list_top=s->file_list_selected-slots+1;
    canvas_begin(s,2,split);view_color(s,"\033[1;33m");field(s,s->file_title,w);reset(s);
    if(s->io.rows<7){at(s,2,1);field(s,"Resize to browse",w);frame_end(s);return;}
    view_rule(s,2,1,left,0);if(split)view_rule(s,2,left+2,w-left-1,0);
    for(i=0;i<slots;++i){
        unsigned p=(listing?s->file_list_top:s->file_top)+i;
        at(s,i+3,1);view_side(s);
        if(listing){if(p<ln)file_row(s,list[p],0,left-2,p==s->file_list_selected,0);else field(s,"",left-2);}
        else if(p<count)file_row(s,ids[p],depths[p],left-2,p==s->file_selected?(split&&s->file_pane?2:1):0,1);else field(s,"",left-2);
        view_side(s);
        if(split){p=s->file_list_top+i;at(s,i+3,left+2);view_side(s);if(p<ln)file_row(s,list[p],0,w-left-3,p==s->file_list_selected&&s->file_pane,0);else field(s,"",w-left-3);view_side(s);}
    }
    view_rule(s,slots+3,1,left,1);if(split)view_rule(s,slots+3,left+2,w-left-1,1);
    at(s,s->io.rows-1,1);
    if(s->file_notice)field(s,s->file_notice,w);
    else {out(s,split?(s->file_pane?"Listing  ":"Tree  "):listing?"Listing  ":"Tree  ");ps_number(s,listing||split?ln:count);out(s," entries  ");if(s->file_directory<s->file_count)field(s,s->file_nodes[s->file_directory].name,w>24?w-24:0);}
    ansi(s,"\033[K");
    at(s,s->io.rows,1);footer_field(s,split?"Tab Panel  ↑/↓ Select  ←/→ Fold  Enter Open  Esc Cancel":listing?"↑/↓ Select  Backspace Parent  Enter Open  Esc Cancel":"↑/↓ Select  ←/→ Fold  Enter Open  Esc Cancel",w,0);
    frame_end(s);
}
PS_API void ps_file_picker_style(ps_session *s,unsigned layout,unsigned icons){if(s){s->file_layout=layout<=PS_FILE_SPLIT?layout:PS_FILE_TREE;s->file_icons=icons<=PS_FILE_ICONS_ASCII?icons:PS_FILE_ICONS_UNICODE;if(s->file_count)file_render(s);}}
PS_API int ps_file_browser_open(ps_session *s,const char *title,const ps_file_node *nodes,unsigned count,ps_file_pick_fn fn,ps_file_load_fn load,void *ctx){
    if(!s||ps_form_active(s)||!title||!file_valid(nodes,count)||s->document_open||s->exit||!(s->io.flags&PS_INTERACTIVE))return 0;
    if(s->board)table_close(s);
    ps_menu_close(s);ps_file_picker_close(s);
    s->file_nodes=nodes;s->file_count=count;s->file_title=title;s->file_pick=fn;s->file_load=load;s->file_context=ctx;
    s->file_selected=s->file_top=s->file_pane=s->file_list_selected=s->file_list_top=0;s->file_directory=0;s->file_notice=0;
    zero(s->file_expanded,sizeof s->file_expanded);zero(s->file_loaded,sizeof s->file_loaded);
    if(load){file_load(s,0);s->file_expanded[0]=1;}
    s->canvas_kind=0;ansi(s,"\033[?1049h");if(PS_MOUSE)ansi(s,"\033[?1002h\033[?1006h");file_render(s);return 1;
}
PS_API int ps_file_picker_open(ps_session *s,const char *title,const ps_file_node *nodes,unsigned count,ps_file_pick_fn fn,void *ctx){return ps_file_browser_open(s,title,nodes,count,fn,0,ctx);}
static void file_key(ps_session *s,unsigned key){
    unsigned ids[128],depths[128],count,id,i,listing=s->file_layout==PS_FILE_LIST||(s->file_layout==PS_FILE_SPLIT&&(!file_split(s)||s->file_pane));
    unsigned *selected=listing?&s->file_list_selected:&s->file_selected;
    if(key==27||key==3){ps_file_pick_fn fn=s->file_pick;void *ctx=s->file_context;ps_file_picker_close(s);if(fn)fn(s,ctx,PS_FILE_ROOT);prompt(s);return;}
    if(key==9&&file_split(s)){s->file_pane=!s->file_pane;file_render(s);return;}
    count=listing?file_listing(s,ids):file_visible(s,ids,depths);
    if(listing&&(key==8||key==127||key==PS_KEY_LEFT)){
        unsigned parent=s->file_nodes[s->file_directory].parent;
        if(parent!=PS_FILE_ROOT){file_focus_directory(s,parent);s->file_expanded[parent]=1;count=file_visible(s,ids,depths);for(i=0;i<count;++i)if(ids[i]==parent)s->file_selected=i;}
        file_render(s);return;
    }
    if(!count)return;
    if(*selected>=count)*selected=count-1;
    id=ids[*selected];
    if(key==PS_KEY_UP&&*selected)--*selected;
    else if(key==PS_KEY_DOWN&&*selected+1<count)++*selected;
    else if(key==PS_KEY_HOME)*selected=0;
    else if(key==PS_KEY_END)*selected=count-1;
    else if(key==PS_KEY_PAGEUP)*selected=*selected>5?*selected-5:0;
    else if(key==PS_KEY_PAGEDOWN)*selected=min(count-1,*selected+5);
    else if(key==PS_KEY_LEFT){if(s->file_expanded[id])s->file_expanded[id]=0;else for(i=0;i<count;++i)if(ids[i]==s->file_nodes[id].parent){*selected=i;break;}}
    else if(key==PS_KEY_RIGHT||key==13||key==' '){
        if(s->file_nodes[id].flags&PS_FILE_DISABLED){ps_ring_bell(s);return;}
        if(s->file_nodes[id].flags&PS_FILE_DIRECTORY){
            file_load(s,id);s->file_expanded[id]=listing||key==PS_KEY_RIGHT?1:!s->file_expanded[id];
            if(listing){file_focus_directory(s,id);count=file_visible(s,ids,depths);for(i=0;i<count;++i)if(ids[i]==id)s->file_selected=i;}
        }else if(key==13){ps_file_pick_fn fn=s->file_pick;void *ctx=s->file_context;ps_file_picker_close(s);if(fn)fn(s,ctx,id);prompt(s);return;}
    }
    file_render(s);
}
static void file_mouse(ps_session *s,unsigned x,unsigned y){
    unsigned ids[128],depths[128],split=file_split(s),left=(s->io.columns-1)*2/5;
    unsigned listing=s->file_layout==PS_FILE_LIST||(s->file_layout==PS_FILE_SPLIT&&(!split||x>left+1)),p,count;
    if(y<3||y>=s->io.rows-3)return;
    if(split&&(x==left||x==left+1||x==left+2))return;
    count=listing?file_listing(s,ids):file_visible(s,ids,depths);p=(listing?s->file_list_top:s->file_top)+y-3;
    if(p<count){s->file_pane=listing;if(listing)s->file_list_selected=p;else {
        unsigned id=ids[p];s->file_selected=p;
        if((s->file_nodes[id].flags&PS_FILE_DIRECTORY)&&!(s->file_nodes[id].flags&PS_FILE_DISABLED)&&x>=2+depths[p]*2&&x<2+depths[p]*2+file_icon_width(s)){file_load(s,id);s->file_expanded[id]=!s->file_expanded[id];}
    }file_render(s);}
}
#if PS_WORKBENCH
#include "../host/workbench/files.inc"
#endif
