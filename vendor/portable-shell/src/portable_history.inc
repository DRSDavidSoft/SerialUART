/* History UI reuses the workspace border, field and selection primitives. */
static void history_dialog_draw(ps_session *s) {
    unsigned slots=s->io.rows-4,i,w=s->io.columns-2;
    if(s->history_selected<s->history_top)s->history_top=s->history_selected;
    if(s->history_selected>=s->history_top+slots)s->history_top=s->history_selected-slots+1;
    ansi(s,"\033[H\033[J\033[?25l");
    at(s,1,2);field(s,"Command history",w);
    view_rule(s,2,1,w,0);
    for(i=0;i<slots;++i){
        unsigned index=s->history_top+i;char number[16];unsigned n;
        at(s,3+i,1);view_side(s);
        if(index<s->history_count){
            if(index==s->history_selected)view_selection(s);
            n=dec(number,index+1);number[n]=0;
            if(w>10){field(s,number,4);out(s," ");field(s,ps_history_get(s,index),w-7);}
            else field(s,ps_history_get(s,index),w-2);
            reset(s);
        }else field(s,"",w-2);
        view_side(s);
    }
    view_rule(s,s->io.rows-1,1,w,1);
    at(s,s->io.rows,1);footer_field(s,"Enter Run  Tab Edit  Esc Cancel",s->io.columns-1,0);
}
static void history_dialog_close(ps_session *s){
    if(!s->history_dialog)return;
    s->history_dialog=0;ansi(s,"\033[0m\033[?25h");
    if(!s->board)ansi(s,"\033[?1049l");
    else {s->dirty=1;render(s);}
}
/* Expansion is deliberately limited to a complete interactive line. Quoted,
   embedded and scripted exclamation marks remain literal. */
static int history_expand_line(ps_session *s){
#if PS_HISTORY_EXPANSION
    unsigned index=0,i;
    const char *entry;
    if(s->line[0]!='!')return 1;
    if(equal(s->line,"!!"))index=s->history_count;
    else {
        if(!s->line[1])return 1;
        for(i=1;s->line[i];++i){
            if(s->line[i]<'0'||s->line[i]>'9')return 1;
            if(index>PS_HISTORY)return 0;
            index=index*10+(unsigned)(s->line[i]-'0');
        }
    }
    if(!index||index>s->history_count)return 0;
    entry=ps_history_get(s,index-1);line_set(s,entry);
#endif
    return 1;
}
