#ifndef PORTABLE_SETTINGS_EDITOR_H
#define PORTABLE_SETTINGS_EDITOR_H
/* Optional registry-to-panel adapter. No commands, settings, allocation or I/O.
   Keep the registry, descriptors, editor and supplied arrays alive while open.
   Close/detach before changing registry membership or rebuilding this adapter. */
#include "portable_settings.h"
#include "portable_style.h"
#ifndef PS_SETTINGS_EDITOR
#define PS_SETTINGS_EDITOR PS_SETTINGS
#endif
#ifndef PS_SETTINGS_GROUP_BYTES
#define PS_SETTINGS_GROUP_BYTES 64
#endif
#if PS_SETTINGS_GROUP_BYTES < 1
#error PS_SETTINGS_GROUP_BYTES must include a terminator
#endif
enum { PS_SETTINGS_EDITOR_MAX_ROWS=128, PS_SETTINGS_LEVEL_MAX=65535 };
typedef struct {
    const ps_setting *setting;
    char group[PS_SETTINGS_GROUP_BYTES];
    ps_control_value observed;
} ps_settings_editor_row;
typedef struct {
    ps_settings *registry;
    ps_settings_editor_row *rows;
    ps_control *controls;
    ps_panel panel;
    ps_control_page page;
    ps_panel_chrome chrome;
    unsigned count;
} ps_settings_editor;
static inline unsigned ps_settings_editor_group_size(const ps_setting *setting){
    unsigned n=0;if(setting->group)return ps_text_length(setting->group);
    while(setting->name[n]&&setting->name[n]!='.')++n;
    return setting->name[n]?n:0;
}
#if PS_PANEL_TEXT
static inline unsigned ps_settings_editor_text(void *context,unsigned id,char *text,ps_byte *roles,unsigned capacity){
    ps_settings_editor *e=(ps_settings_editor *)context;unsigned i,n;
    if(id>=e->count||e->controls[id].kind!=PS_CONTROL_TEXT||
       (e->rows[id].setting->type!=PS_SETTING_INT&&e->rows[id].setting->type!=PS_SETTING_UINT))return 0;
    n=ps_text_length(e->rows[id].observed.text);if(n>=capacity)return 0;
    for(i=0;i<n;++i){text[i]=e->rows[id].observed.text[i];roles[i]=PS_INK_NUMBER;}
    text[n]=0;return n;
}
#endif

static inline int ps_settings_editor_read(void *context,unsigned id,ps_control_value *out){
    ps_settings_editor *e=(ps_settings_editor *)context;
    ps_settings_editor_row *row;const ps_setting *setting;ps_setting_value value;
    ps_control_value current;unsigned i,n=0;ps_u32 revision;
    if(!PS_SETTINGS_EDITOR||!e||id>=e->count)return 0;
    row=e->rows+id;setting=row->setting;
    current.number=0;current.flags=0;current.revision=0;current.text[0]=0;
    if(ps_settings_find(e->registry,setting->name)==setting&&
       ps_settings_get(e->registry,setting->name,&value)==PS_SETTING_OK){
        current.flags=PS_CONTROL_VALID;
        if(!setting->write)current.flags|=PS_CONTROL_LOCKED;
        if(setting->type==PS_SETTING_TEXT){
            if(!value.text)current.flags=0;
            else {
                n=ps_text_length(value.text);
                if(n>=sizeof current.text){
                    current.flags|=PS_CONTROL_LOCKED;n=sizeof current.text-1;
                    while(n&&((unsigned char)value.text[n]&192)==128)--n;
                }
                for(i=0;i<n;++i)current.text[i]=value.text[i];
                current.text[n]=0;
            }
        }else if(setting->type==PS_SETTING_INT)
            ps_terminal_format(current.text,sizeof current.text,"%d",value.integer);
        else {
            current.number=value.natural;
            ps_terminal_format(current.text,sizeof current.text,"%u",value.natural);
            if(setting->type==PS_SETTING_UINT)current.flags|=PS_CONTROL_NUMBER_ONLY;
            if((setting->type==PS_SETTING_BOOL&&value.natural>1)||
               (setting->type==PS_SETTING_CHOICE&&value.natural>=setting->choice_count))current.flags=0;
        }
    }
    revision=row->observed.revision;
    if(current.number!=row->observed.number||current.flags!=row->observed.flags||
       !ps_text_equal(current.text,row->observed.text))++revision;
    current.revision=revision;row->observed=current;*out=current;
    return !!(current.flags&PS_CONTROL_VALID);
}
static inline int ps_settings_editor_write(void *context,unsigned id,const ps_control_value *value,ps_u32 revision){
    ps_settings_editor *e=(ps_settings_editor *)context;const ps_setting *setting;
    ps_control_value current;const char *text;char number[PS_TERMINAL_INTEGER_BYTES];
    if(!PS_SETTINGS_EDITOR||!e||!value||id>=e->count||
       !ps_settings_editor_read(e,id,&current)||current.revision!=revision||
       (current.flags&PS_CONTROL_LOCKED))return 0;
    setting=e->rows[id].setting;text=value->text;
    if(setting->type==PS_SETTING_BOOL)text=value->number==1?"true":value->number==0?"false":0;
    else if(setting->type==PS_SETTING_CHOICE)text=value->number<setting->choice_count?setting->choices[value->number]:0;
    else if(e->controls[id].kind==PS_CONTROL_LEVEL){
        ps_terminal_format(number,sizeof number,"%u",value->number);text=number;
    }
    return text&&ps_settings_set(e->registry,setting->name,text)==PS_SETTING_OK;
}
/* Build a single reusable page. Explicit groups win; otherwise the first dotted
   key component becomes the heading. Register adjacent members together.
   INT and large UINT use validated text editing, preserving their full range.
   Values too long for the shared cell editor stay visible but locked. */
static inline int ps_settings_editor_init(ps_settings_editor *e,ps_settings *registry,
    ps_control *controls,ps_settings_editor_row *rows,unsigned capacity,const char *title){
    unsigned i,j,n;const char *group;
    if(!PS_SETTINGS_EDITOR||!PS_SETTINGS||!e||!registry||!controls||!rows||!title||
       registry->count>capacity||registry->count>PS_SETTINGS_EDITOR_MAX_ROWS)return 0;
    /* Validate first so a capacity/group error cannot partially rebuild storage. */
    for(i=0;i<registry->count;++i){const ps_setting *s=registry->entries[i];
        if(ps_settings_editor_group_size(s)>=PS_SETTINGS_GROUP_BYTES)return 0;
        if(s->type==PS_SETTING_CHOICE&&s->choice_count>PS_SETTINGS_EDITOR_MAX_ROWS)return 0;
    }
    e->registry=registry;e->rows=rows;e->controls=controls;e->count=registry->count;
    for(i=0;i<e->count;++i){const ps_setting *s=registry->entries[i];ps_control *c=controls+i;
        rows[i].setting=s;rows[i].observed.number=rows[i].observed.flags=rows[i].observed.revision=0;
        rows[i].observed.text[0]=0;group=s->group?s->group:s->name;n=ps_settings_editor_group_size(s);
        for(j=0;j<n;++j)rows[i].group[j]=group[j];
        rows[i].group[j]=0;c->id=i;c->group=rows[i].group;c->label=s->name;
        c->minimum=0;c->maximum=0;c->step=1;c->choices=s->choices;c->choice_count=s->choice_count;
        c->kind=s->type==PS_SETTING_BOOL?PS_CONTROL_TOGGLE:s->type==PS_SETTING_CHOICE?PS_CONTROL_CHOICE:PS_CONTROL_TEXT;
        if(s->type==PS_SETTING_UINT&&s->unsigned_maximum<=PS_SETTINGS_LEVEL_MAX){
            c->kind=PS_CONTROL_LEVEL;c->minimum=s->unsigned_minimum;c->maximum=s->unsigned_maximum;
        }
        if(c->kind==PS_CONTROL_LEVEL&&c->minimum==c->maximum)c->kind=PS_CONTROL_TEXT;
    }
    e->page.title=title;e->page.controls=controls;e->page.count=e->count;
    e->panel.title=title;e->panel.status=0;e->panel.pages=&e->page;e->panel.count=1;
    e->panel.read=ps_settings_editor_read;e->panel.write=ps_settings_editor_write;e->panel.context=e;
    e->chrome.columns=0;e->chrome.count=0;e->chrome.summary=0;e->chrome.notice=0;
    e->chrome.actions=0;e->chrome.navigation=0;e->chrome.text=0;
#if PS_PANEL_TEXT
    e->chrome.text=ps_settings_editor_text;
#endif
    return 1;
}
static inline int ps_settings_editor_open(ps_settings_editor *e,ps_session *session){
    if(!PS_SETTINGS_EDITOR||!e||!session||!ps_panel_attach(session,&e->panel))return 0;
    ps_panel_set_chrome(session,&e->chrome);
    return ps_panel_open(session,0);
}
#endif
