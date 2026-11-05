#ifndef PORTABLE_SETTINGS_H
#define PORTABLE_SETTINGS_H
/* Optional typed settings registry. No storage ownership, OS access or persistence.
 * Descriptors and callback-returned strings remain owned by the consumer. */
#include "portable_shell.h"
#include "portable_terminal.h"
#ifndef PS_SETTINGS
#define PS_SETTINGS 1
#endif
enum { PS_SETTING_BOOL, PS_SETTING_INT, PS_SETTING_UINT, PS_SETTING_CHOICE, PS_SETTING_TEXT };
enum { PS_SETTING_OK, PS_SETTING_NOT_FOUND, PS_SETTING_READ_ONLY, PS_SETTING_INVALID,
       PS_SETTING_REJECTED, PS_SETTING_UNAVAILABLE, PS_SETTING_FULL };
/* INT uses integer; BOOL/UINT/CHOICE use natural (choice index); TEXT uses text. */
typedef struct { int integer; ps_u32 natural; const char *text; } ps_setting_value;
typedef int (*ps_setting_read_fn)(void *,ps_setting_value *);
typedef int (*ps_setting_write_fn)(void *,const ps_setting_value *);
typedef int (*ps_setting_validate_fn)(void *,const ps_setting_value *);
typedef struct {
    const char *name,*description,*group;
    unsigned type;
    int minimum,maximum;
    ps_u32 unsigned_minimum,unsigned_maximum;
    const char *const *choices; unsigned choice_count,max_length;
    ps_setting_read_fn read;
    ps_setting_write_fn write;
    ps_setting_validate_fn validate;
    void *context;
} ps_setting;
typedef struct { const ps_setting **entries; unsigned count,capacity; } ps_settings;
static inline void ps_settings_init(ps_settings *r,const ps_setting **storage,unsigned capacity){
    if(!r)return;
    r->entries=storage;r->count=0;r->capacity=storage?capacity:0;
}
static inline const ps_setting *ps_settings_find(const ps_settings *r,const char *name){
    unsigned i;if(!PS_SETTINGS||!r||!name)return 0;
    for(i=0;i<r->count;++i)if(ps_text_equal(r->entries[i]->name,name))return r->entries[i];
    return 0;
}
static inline int ps_settings_register(ps_settings *r,const ps_setting *entry){
    unsigned i;
    if(!PS_SETTINGS||!r||!entry||!entry->name||!*entry->name||!entry->read||entry->type>PS_SETTING_TEXT)return PS_SETTING_INVALID;
    if(entry->type==PS_SETTING_INT&&entry->minimum>entry->maximum)return PS_SETTING_INVALID;
    if(entry->type==PS_SETTING_UINT&&entry->unsigned_minimum>entry->unsigned_maximum)return PS_SETTING_INVALID;
    if(entry->type==PS_SETTING_CHOICE){
        if(!entry->choices||!entry->choice_count)return PS_SETTING_INVALID;
        for(i=0;i<entry->choice_count;++i)if(!entry->choices[i])return PS_SETTING_INVALID;
    }
    for(i=0;i<r->count;++i)if(ps_text_equal(r->entries[i]->name,entry->name)){r->entries[i]=entry;return 0;}
    if(r->count==r->capacity)return PS_SETTING_FULL;
    r->entries[r->count++]=entry;return 0;
}
static inline int ps_settings_unregister(ps_settings *r,const char *name){
    unsigned i;if(!PS_SETTINGS||!r||!name)return PS_SETTING_NOT_FOUND;
    for(i=0;i<r->count;++i)if(ps_text_equal(r->entries[i]->name,name)){
        for(;i+1<r->count;++i)r->entries[i]=r->entries[i+1];
        --r->count;return 0;
    }
    return PS_SETTING_NOT_FOUND;
}
static inline int ps_setting_parse(const ps_setting *entry,const char *text,ps_setting_value *value){
    ps_u32 n=0,limit=0xffffffffu;unsigned i=0,negative=0;
    if(!entry||!text||!value)return PS_SETTING_INVALID;
    value->integer=0;value->natural=0;value->text=text;
    if(entry->type==PS_SETTING_TEXT)
        return ps_text_length(text)<=entry->max_length?0:PS_SETTING_INVALID;
    if(entry->type==PS_SETTING_BOOL){
        if(ps_text_equal(text,"true")||ps_text_equal(text,"1"))value->natural=1;
        else if(!ps_text_equal(text,"false")&&!ps_text_equal(text,"0"))return PS_SETTING_INVALID;
        return 0;
    }
    if(entry->type==PS_SETTING_CHOICE){
        for(i=0;i<entry->choice_count;++i)if(ps_text_equal(text,entry->choices[i])){value->natural=i;return 0;}
        return PS_SETTING_INVALID;
    }
    if(entry->type==PS_SETTING_INT){
        if(text[i]=='-'){negative=1;++i;}
        else if(text[i]=='+')++i;
        limit=negative?2147483648u:2147483647u;
    }
    if(!text[i])return PS_SETTING_INVALID;
    for(;text[i];++i){
        unsigned d=(unsigned)(text[i]-'0');
        if(d>9||n>(limit-d)/10u)return PS_SETTING_INVALID;
        n=n*10u+d;
    }
    value->natural=n;
    if(entry->type==PS_SETTING_UINT)return n>=entry->unsigned_minimum&&n<=entry->unsigned_maximum?0:PS_SETTING_INVALID;
    if(entry->type!=PS_SETTING_INT)return PS_SETTING_INVALID;
    value->integer=negative?(n==2147483648u?(-2147483647-1):-(int)n):(int)n;
    return value->integer>=entry->minimum&&value->integer<=entry->maximum?0:PS_SETTING_INVALID;
}
static inline int ps_settings_get(const ps_settings *r,const char *name,ps_setting_value *value){
    const ps_setting *entry=ps_settings_find(r,name);
    if(!entry)return PS_SETTING_NOT_FOUND;
    if(!value)return PS_SETTING_INVALID;
    value->integer=0;value->natural=0;value->text=0;
    return entry->read(entry->context,value)?0:PS_SETTING_UNAVAILABLE;
}
static inline int ps_settings_set(ps_settings *r,const char *name,const char *text){
    ps_setting_value value;int result;const ps_setting *entry=ps_settings_find(r,name);
    if(!entry)return PS_SETTING_NOT_FOUND;
    if(!entry->write)return PS_SETTING_READ_ONLY;
    result=ps_setting_parse(entry,text,&value);if(result)return result;
    if(entry->validate&&!entry->validate(entry->context,&value))return PS_SETTING_REJECTED;
    return entry->write(entry->context,&value)?0:PS_SETTING_REJECTED;
}
/* Optional command callback: consumer selects its name, category, usage and help.
 * No arguments lists names; KEY reads; KEY VALUE validates and writes once. */
static inline int ps_settings_command(ps_session *s,int argc,const char *const *argv,void *context){
    ps_settings *r=(ps_settings *)context;ps_setting_value v;const ps_setting *entry;unsigned i;int result;
    if(!PS_SETTINGS||!r)return PS_SETTING_UNAVAILABLE;
    if(argc==1){for(i=0;i<r->count;++i){ps_write(s,r->entries[i]->name);ps_newline(s);}return 0;}
    if(argc==3)return ps_settings_set(r,argv[1],argv[2]);
    if(argc!=2)return PS_SETTING_INVALID;
    entry=ps_settings_find(r,argv[1]);result=ps_settings_get(r,argv[1],&v);if(result)return result;
    if(entry->type==PS_SETTING_TEXT){if(!v.text)return PS_SETTING_UNAVAILABLE;ps_write(s,v.text);}
    else if(entry->type==PS_SETTING_CHOICE){if(v.natural>=entry->choice_count)return PS_SETTING_UNAVAILABLE;ps_write(s,entry->choices[v.natural]);}
    else if(entry->type==PS_SETTING_INT){if(v.integer<0){ps_write(s,"-");ps_number(s,0u-(unsigned)v.integer);}else ps_number(s,(unsigned)v.integer);}
    else {if(entry->type==PS_SETTING_BOOL&&v.natural>1)return PS_SETTING_UNAVAILABLE;ps_number(s,v.natural);}
    ps_newline(s);return 0;
}
#endif
