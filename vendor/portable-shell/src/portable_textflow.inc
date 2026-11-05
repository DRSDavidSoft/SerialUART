/* Shared, fixed-storage help presentation. No host API or allocator. */
static unsigned flow_width(ps_session *s) {
    return s->io.columns >= 20 ? min(s->io.columns,240)-1 : 79;
}
static unsigned flow_cells(ps_session *s,const char *text) { return text?cells(text,length(text),s->io.flags):0; }
/* Preserve UTF-8 sequences, explicit newlines and hanging indentation.
   Metadata is trusted printable text, not embedded terminal control codes. */
static void flow_text(ps_session *s,const char *text,unsigned column,unsigned indent) {
    unsigned available=flow_width(s);
    if(!text)text="";
    while(*text){
        const char *end;unsigned cells=0;
        if(*text=='\n'){ps_newline(s);spaces(s,indent);column=indent;++text;continue;}
        if(*text==' '||*text=='\t'){
            ++text;
            if(column>indent&&column<available){out(s," ");++column;}
            continue;
        }
        end=text;while(*end&&*end!=' '&&*end!='\t'&&*end!='\n'){unsigned n,c=rune(end,&n);cells+=(s->io.flags&PS_UNICODE)?width(c):1;end+=n;}
        if(column>indent&&column+cells>available){ps_newline(s);spaces(s,indent);column=indent;}
        while(text<end){
            unsigned n,c=rune(text,&n),w=(s->io.flags&PS_UNICODE)?width(c):1; const char *next=text+n;
            if(column+w>available){ps_newline(s);spaces(s,indent);column=indent;}
            raw(s,text,(unsigned)(next-text));column+=w;text=next;
        }
    }
    ps_newline(s);
}
