PS_API void ps_editor_set_style(ps_session *s,unsigned flags){if(s)s->document_style=flags;}
#if PS_EDITOR
static unsigned document_border(ps_session *s){return (s->document_style&PS_EDITOR_BORDER)&&s->io.rows>=6;}
static unsigned document_header(ps_session *s){return !!(s->document_style&PS_EDITOR_HEADER);}
static unsigned document_height(ps_session *s){return s->io.rows-2-document_header(s)-2*document_border(s);}
/* Bounded UTF-8 document editor; all persistence belongs to the caller. */
PS_API unsigned ps_editor_active(const ps_session *s){return s?s->document_open:0;}
PS_API const char *ps_editor_text(const ps_session *s){return s?s->document:"";}
/* A visual row ends at an explicit newline or the last fitting word boundary. */
static unsigned document_row(ps_session *s,unsigned start,unsigned *end){
    unsigned i=start,col=0,word=0,w=s->io.columns-1-2*document_border(s);
    while(i<s->document_length){
        unsigned n,c,r;
        if(s->document[i]=='\n'){*end=i;return i+1;}
        c=rune(s->document+i,&n);r=c=='\t'?4-col%4:((s->io.flags&PS_UNICODE)?width(c):1);
        if(col+r>w){if(word>start)i=word;*end=i;return i;}
        col+=r;i+=n;if(c==' '||c=='\t')word=i;
    }
    *end=i;return i;
}
static unsigned document_column(ps_session *s,unsigned start,unsigned pos){
    unsigned col=0;
    while(start<pos){unsigned n,c=rune(s->document+start,&n);col+=c=='\t'?4-col%4:((s->io.flags&PS_UNICODE)?width(c):1);start+=n;}
    return col;
}
static unsigned document_location(ps_session *s,unsigned *start){
    unsigned a=0,row=0,end,b;
    while(a<s->document_length){b=document_row(s,a,&end);
        if(s->document_pos< b || (s->document_pos==end && (end==s->document_length||s->document[end]=='\n')))break;
        if(b==a)break;
        a=b;++row;
    }
    *start=a;return row;
}
static void document_render(ps_session *s){
    unsigned start,row=document_location(s,&start),height=document_height(s),a=0,r=0,end,b;
    unsigned border=document_border(s),top=1+document_header(s)+border;
    unsigned col=document_column(s,start,s->document_pos),painted=0,content_width=s->io.columns-1-2*border;
    char status[128];unsigned sn;
    if(row<s->document_top)s->document_top=row;
    if(row>=s->document_top+height)s->document_top=row-height+1;
    canvas_begin(s,3,s->document_style);ink(s,PS_INK_HEADING);
        if(document_header(s))field(s,s->document_title,s->io.columns-2);
    reset(s);
    if(border){
        unsigned y;view_rule(s,top-1,1,s->io.columns-1,0);
        for(y=top;y<top+height;++y){at(s,y,1);view_side(s);at(s,y,s->io.columns-1);view_side(s);}
        view_rule(s,top+height,1,s->io.columns-1,1);
    }
    for(;;){
        b=document_row(s,a,&end);
        if(r>=s->document_top && r<s->document_top+height){
            unsigned i=a,cx=0;at(s,r-s->document_top+top,1+border);
            while(i<end){unsigned n,c=rune(s->document+i,&n);
                if(c=='\t'){unsigned spaces=4-cx%4;cx+=spaces;while(spaces--)out(s," ");}
                else {if((s->io.flags&PS_UNICODE)&&c!=0xfffd&&!(c>=128&&c<160))raw(s,s->document+i,n);else field(s,s->document+i,1);cx+=(s->io.flags&PS_UNICODE)?width(c):1;}
                i+=n;
            }
            if(cx<content_width)spaces(s,content_width-cx);
            ++painted;
        }
        if(b==a||b==s->document_length||r>=s->document_top+height)break;
        a=b;++r;
    }
    while(painted<height){at(s,top+painted,1+border);spaces(s,content_width);++painted;}
    at(s,s->io.rows-1,1);ink(s,PS_INK_INACTIVE);
    if(s->document_style&PS_EDITOR_READ_ONLY){
        unsigned pos=0,total=1,last;
        while(pos<s->document_length){unsigned following=document_row(s,pos,&last);if(following<=pos||following>=s->document_length)break;pos=following;++total;}
        str(status,"Read only  Rows ",sizeof status);sn=length(status);
        sn+=dec(status+sn,s->document_top+1);status[sn++]='-';sn+=dec(status+sn,min(s->document_top+height,total));
        status[sn++]='/';sn+=dec(status+sn,total);status[sn++]=' ';status[sn]=0;
    }else{
        str(status,s->document_dirty?"Modified  ":"Unmodified  ",sizeof status);sn=length(status);
        sn+=dec(status+sn,s->document_length);status[sn++]='/';sn+=dec(status+sn,PS_EDITOR_CAPACITY-1);
        str(status+sn," bytes  ",sizeof status-sn);sn=length(status);
    }
    if(s->document_notice)str(status+sn,s->document_notice,sizeof status-sn);
    field(s,status,s->io.columns-1);reset(s);
    at(s,s->io.rows,1);ink(s,PS_INK_HEADING);
    if(s->document_dialog)footer_field(s,"S Save & close  D Discard  Esc Cancel",s->io.columns-1,0);
    else if(s->document_style&PS_EDITOR_READ_ONLY)footer_field(s,"↑/↓ Scroll  PgUp/PgDn Page  Home/End Start/end  Esc Back",s->io.columns-1,0);
    else footer_field(s,"^S Save  ^Q/Esc Close  ^Z Undo/redo  ^A/^E Line start/end",s->io.columns-1,0);
    reset(s);at(s,row-s->document_top+top,col+1+border);ansi(s,(s->document_style&PS_EDITOR_READ_ONLY)?"\033[?25l":"\033[?25h");
    frame_end(s);
}
static void document_close(ps_session *s){
    s->document_open=0;s->document_dialog=0;
    ansi(s,"\033[0m\033[?25h\033[?1049l");prompt(s);
}
PS_API int ps_editor_open(ps_session *s,const char *title,const char *text,ps_editor_save_fn save,void *ctx){
    unsigned i,n;if(!s||ps_form_active(s)||!title||!text||s->document_open||s->exit||!(s->io.flags&PS_INTERACTIVE))return 0;
    n=length(text);if(n>=PS_EDITOR_CAPACITY)return 0;
    for(i=0;i<n;++i)if(((unsigned char)text[i]<32||text[i]==127)&&text[i]!='\n'&&text[i]!='\t')return 0;
    if(s->board)table_close(s);
    ps_file_picker_close(s);
    ps_menu_close(s);
    copy(s->document,text,n+1);s->document_title=title;s->document_save=save;s->document_context=ctx;
    s->document_length=n;s->document_pos=s->document_top=s->document_dirty=s->document_dialog=s->document_undo_valid=0;
    s->document_notice=0;s->document_open=1;s->search=0;
    s->canvas_kind=0;ansi(s,"\033[?1049h\033[?2004h");document_render(s);return 1;
}
static void document_snapshot(ps_session *s){
    copy(s->document_undo,s->document,s->document_length+1);
    s->document_undo_pos=s->document_pos;s->document_undo_dirty=s->document_dirty;s->document_undo_valid=1;
}
static void document_insert(ps_session *s,unsigned c){
    if(s->document_style&PS_EDITOR_READ_ONLY)return;
    if(s->document_length+1>=PS_EDITOR_CAPACITY){s->document_notice="Buffer full";return;}
    if(!s->paste && (c&192)!=128)document_snapshot(s);
    move(s->document+s->document_pos+1,s->document+s->document_pos,s->document_length-s->document_pos+1);
    s->document[s->document_pos++]=(char)c;++s->document_length;s->document_dirty=1;
}
static int document_save(ps_session *s){
    if(s->document_style&PS_EDITOR_READ_ONLY)return 0;
    if(!s->document_save||!s->document_save(s->document_context,s->document,s->document_length)){
        s->document_notice="Save failed; draft retained";return 0;
    }
    s->document_dirty=0;s->document_undo_valid=0;s->document_notice="Saved";return 1;
}
#if PS_WORKBENCH
static int document_save_scratch(void *ctx,const char *text,unsigned n){
    ps_session *s=ctx;copy(s->scratch,text,n);s->scratch[n]=0;return 1;
}
#endif
static void document_vertical(ps_session *s,int delta){
    unsigned start,row=document_location(s,&start),col=document_column(s,start,s->document_pos),a=0,r=0,end,b,cx=0;
    unsigned target=delta<0?(row<(unsigned)-delta?0:row-(unsigned)-delta):row+(unsigned)delta;
    while(r<target){b=document_row(s,a,&end);if(b==a||b==s->document_length){a=b;break;}a=b;++r;}
    document_row(s,a,&end);
    while(a<end){unsigned n,c=rune(s->document+a,&n),w=c=='\t'?4-cx%4:((s->io.flags&PS_UNICODE)?width(c):1);if(cx+w>col)break;cx+=w;a+=n;}
    s->document_pos=a;
}
static void document_key(ps_session *s,unsigned key){
    unsigned start,end,b;s->document_notice=0;
    if(s->document_style&PS_EDITOR_READ_ONLY){
        if(key==PS_KEY_HOME){s->document_pos=0;document_render(s);return;}
        if(key==PS_KEY_END){s->document_pos=s->document_length;document_render(s);return;}
        if(key!=27&&key!=17&&key!=3&&key!=PS_KEY_LEFT&&key!=PS_KEY_RIGHT&&
           key!=PS_KEY_UP&&key!=PS_KEY_DOWN&&key!=PS_KEY_PAGEUP&&key!=PS_KEY_PAGEDOWN)return;
    }
    if(s->document_dialog){
        if(key=='c'||key=='C'||key==27)s->document_dialog=0;
        else if(key=='d'||key=='D'){document_close(s);return;}
        else if(key=='s'||key=='S'){if(document_save(s)){document_close(s);return;}}
        document_render(s);return;
    }
    if(key==27||key==17||key==3){if(s->document_dirty)s->document_dialog=1;else {document_close(s);return;}}
    else if(key==19)document_save(s);
    else if(key==26&&s->document_undo_valid){
        unsigned i,p=s->document_pos,d=s->document_dirty;
        for(i=0;i<PS_EDITOR_CAPACITY;++i){char c=s->document[i];s->document[i]=s->document_undo[i];s->document_undo[i]=c;}
        s->document_pos=s->document_undo_pos;s->document_undo_pos=p;s->document_dirty=s->document_undo_dirty;s->document_undo_dirty=d;s->document_length=length(s->document);
    }
    else if(key==PS_KEY_LEFT)s->document_pos=prev(s->document,s->document_pos);
    else if(key==PS_KEY_RIGHT)s->document_pos=next(s->document,s->document_pos);
    else if(key==PS_KEY_UP||key==PS_KEY_DOWN)document_vertical(s,key==PS_KEY_UP?-1:1);
    else if(key==PS_KEY_PAGEUP||key==PS_KEY_PAGEDOWN)document_vertical(s,key==PS_KEY_PAGEUP?-(int)(document_height(s)):(int)(document_height(s)));
    else if(key==PS_KEY_HOME||key==1){document_location(s,&start);s->document_pos=start;}
    else if(key==PS_KEY_END||key==5){document_location(s,&start);document_row(s,start,&end);s->document_pos=end;}
    else if(key==8||key==127||key==PS_KEY_DELETE){
        start=s->document_pos;b=key==PS_KEY_DELETE?next(s->document,start):start;
        if(key!=PS_KEY_DELETE)start=prev(s->document,start);
        if(b>start){document_snapshot(s);move(s->document+start,s->document+b,s->document_length-b+1);s->document_length-=b-start;s->document_pos=start;s->document_dirty=1;}
    }
    else if(key==13||key==9||(key>=32&&key<256))document_insert(s,key==13?'\n':key);
    document_render(s);
}
static void document_paste(ps_session *s,unsigned c){
    if(s->document_dialog||(s->document_style&PS_EDITOR_READ_ONLY))return;
    if(c=='\n'&&s->suppress_lf){s->suppress_lf=0;return;}
    s->suppress_lf=c=='\r';if(c=='\r')c='\n';
    if(c=='\n'||c=='\t'||(c>=32&&c!=127))document_insert(s,c);
    document_render(s);
}
#else
PS_API unsigned ps_editor_active(const ps_session *s){(void)s;return 0;}
PS_API const char *ps_editor_text(const ps_session *s){(void)s;return "";}
PS_API int ps_editor_open(ps_session *s,const char *t,const char *p,ps_editor_save_fn f,void *ctx){(void)s;(void)t;(void)p;(void)f;(void)ctx;return 0;}
static void document_render(ps_session *s){(void)s;}
static void document_close(ps_session *s){(void)s;}
static void document_snapshot(ps_session *s){(void)s;}
static void document_key(ps_session *s,unsigned k){(void)s;(void)k;}
static void document_paste(ps_session *s,unsigned k){(void)s;(void)k;}
#if PS_WORKBENCH
static int document_save_scratch(void *ctx,const char *p,unsigned n){(void)ctx;(void)p;(void)n;return 0;}
#endif
#endif
