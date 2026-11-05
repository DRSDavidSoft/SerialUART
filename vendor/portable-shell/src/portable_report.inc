/* Borderless reports share text flow and terminal primitives with command help. */
PS_API unsigned ps_print_report(ps_session *s,const ps_report_row *rows,unsigned count,unsigned style){
#if PS_REPORT
    unsigned i,available;
    if(!s||(!rows&&count))return 0;
    available=flow_width(s);
    for(i=0;i<count;++i){
        const ps_report_row *r=rows+i;
        const char *label=r->label?r->label:"",*note=r->annotation?r->annotation:"",*value=r->value?r->value:"";
        unsigned l=flow_cells(s,label),n=flow_cells(s,note),v=flow_cells(s,value),left=l+(n?n+1:0),pad,j;
        if(r->flags&PS_REPORT_GAP)ps_newline(s);
        if(left+v+6>available){
            ink(s,(r->flags&PS_REPORT_SECTION)?PS_INK_HEADING:PS_INK_TEXT);
            out(s,"  ");flow_text(s,label,2,2);reset(s);
            if(n){ink(s,PS_INK_MUTED);out(s,"    ");flow_text(s,note,4,4);reset(s);}
            if(v){ink(s,(r->flags&PS_REPORT_NUMBER)?PS_INK_NUMBER:PS_INK_TEXT);
                if(v+2<=available){spaces(s,available-v);ps_write(s,value);ps_newline(s);}
                else {out(s,"    ");flow_text(s,value,4,4);}reset(s);}
            continue;
        }
        out(s,"  ");ink(s,(r->flags&PS_REPORT_SECTION)?PS_INK_HEADING:PS_INK_TEXT);
        if(r->flags&PS_REPORT_SECTION)ansi(s,"\033[1m");
        ps_write(s,label);reset(s);
        if(n){out(s," ");ink(s,PS_INK_MUTED);ps_write(s,note);}
        out(s," ");ink(s,PS_INK_MUTED);
        pad=available-2-left-1-(v?v+1:0);
        for(j=0;j<pad;++j)out(s,(style&PS_REPORT_SPACES)?" ":
            ((style&PS_REPORT_DENSE)||!(j&1))?".":" ");
        if(v){out(s," ");ink(s,(r->flags&PS_REPORT_NUMBER)?PS_INK_NUMBER:PS_INK_TEXT);ps_write(s,value);}
        reset(s);ps_newline(s);
    }
    return count;
#else
    (void)s;(void)rows;(void)count;(void)style;return 0;
#endif
}
