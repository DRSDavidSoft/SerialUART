static void help_heading(ps_session *s,const char *label){
    ink(s,PS_INK_HEADING);ansi(s,"\033[1m");ps_write(s,label);out(s,":");reset(s);ps_newline(s);
}
static void help_row(ps_session *s,const char *label,const char *description,unsigned width,unsigned role){
    unsigned n=flow_cells(s,label),indent=width+4;
    out(s,"  ");ink(s,role);
    if(n>width){flow_text(s,label,2,2);reset(s);spaces(s,indent);}
    else {ps_write(s,label);reset(s);spaces(s,width-n+2);}
    flow_text(s,description,indent,indent);
}
#if PS_HELP_DETAILS
#if PS_WORKBENCH
#include "../host/workbench/help.inc"
#endif
static const ps_help_document *help_document(ps_session *s,const char *name){
    unsigned i;for(i=0;i<s->command_count;++i)if(s->command_help[i]&&equal(s->commands[i].name,name))return s->command_help[i];
    for(i=0;i<s->help_document_count;++i)
        if(equal(name,s->help_documents[i].name))return s->help_documents+i;
#if PS_WORKBENCH
    return equal(name,"segments")?&segment_help:0;
#else
    return 0;
#endif
}
static const char *help_description(ps_session *s,const ps_command *c){
    const ps_help_document *d=help_document(s,c->name);
    if(d&&d->description)return d->description;
#if PS_WORKBENCH
    unsigned i;
    if(c->run==builtin)for(i=0;i<PS_COUNT_OF(builtins);++i)
        if(equal(c->name,builtins[i].name))return help_summaries[i];
#endif
    return c->usage;
}
static void help_items(ps_session *s,const char *heading,const ps_help_item *items,unsigned count,unsigned role){
    unsigned i,width=0,limit=flow_width(s)/3;
    if(!items||!count)return;
    for(i=0;i<count;++i)if(flow_cells(s,items[i].label)>width)width=flow_cells(s,items[i].label);
    if(width>limit)width=limit;
    ps_newline(s);help_heading(s,heading);
    for(i=0;i<count;++i)help_row(s,items[i].label,items[i].description,width,role);
}
#endif
PS_API void ps_set_help_documents(ps_session *s,const ps_help_document *docs,unsigned count){
#if PS_HELP_DETAILS
    if(s){s->help_documents=docs;s->help_document_count=docs?count:0;}
#else
    (void)s;(void)docs;(void)count;
#endif
}
PS_API int ps_print_help(ps_session *s,const char *name,unsigned layout){
    unsigned i,j,k,printed=0,width=0,group_width=0;
    if(!s)return 0;
    for(i=0;i<s->command_count;++i){
        const ps_command *c=s->commands+i;
        if(flow_cells(s,c->name)>width)width=flow_cells(s,c->name);
        if(flow_cells(s,command_category(c))>group_width)group_width=flow_cells(s,command_category(c));
    }
    if(width>flow_width(s)/3)width=flow_width(s)/3;
    for(i=0;i<s->command_count;++i){
        const ps_command *c=s->commands+i;
        if(name){
            if(!name_match(s,c->name,name,0))continue;
#if PS_HELP_DETAILS
            {const ps_help_document *d=help_document(s,c->name);
            help_heading(s,(s->help_labels&&s->help_labels->description?s->help_labels->description:"Description"));out(s,"  ");flow_text(s,help_description(s,c),2,2);ps_newline(s);
#endif
            ink(s,PS_INK_HEADING);ps_write(s,s->help_labels&&s->help_labels->usage?s->help_labels->usage:"Usage");out(s,": ");reset(s);ps_newline(s);
            out(s,"  ");ink(s,PS_INK_COMMAND);ps_write(s,c->name);out(s," ");if(s->io.flags&PS_COLOR)reset(s);
            flow_text(s,c->usage,3+flow_cells(s,c->name),4);
#if PS_HELP_DETAILS
            if(d){help_items(s,(s->help_labels&&s->help_labels->arguments?s->help_labels->arguments:"Arguments"),d->arguments,d->argument_count,PS_INK_NUMBER);
                help_items(s,(s->help_labels&&s->help_labels->options?s->help_labels->options:"Options"),d->options,d->option_count,PS_INK_SUCCESS);
                help_items(s,(s->help_labels&&s->help_labels->examples?s->help_labels->examples:"Examples"),d->examples,d->example_count,PS_INK_COMMAND);}
            }
#endif
            return 1;
        }
        if(layout==PS_HELP_COMPACT){
            ink(s,0);field(s,command_category(c),group_width+2);
            ink(s,PS_INK_COMMAND);
            if(flow_cells(s,c->name)>width){flow_text(s,c->name,group_width+2,2);reset(s);spaces(s,2);flow_text(s,c->usage,2,2);++printed;continue;}
            field(s,c->name,width+2);reset(s);
            flow_text(s,c->usage,group_width+width+4,2);++printed;continue;
        }
        for(j=0;j<i;++j)if(equal(command_category(s->commands+j),command_category(c)))break;
        if(j!=i)continue;
        if(printed)ps_newline(s);
        help_heading(s,command_category(c));
        for(k=i;k<s->command_count;++k){
            const ps_command *entry=s->commands+k;
            if(!equal(command_category(entry),command_category(c)))continue;
#if PS_HELP_DETAILS
            help_row(s,entry->name,help_description(s,entry),width,PS_INK_COMMAND);
#else
            help_row(s,entry->name,entry->usage,width,PS_INK_COMMAND);
#endif
            ++printed;
        }
    }
    return (int)printed;
}
