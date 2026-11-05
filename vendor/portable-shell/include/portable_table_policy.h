#ifndef PORTABLE_TABLE_POLICY_H
#define PORTABLE_TABLE_POLICY_H
#ifndef PS_SECTION_BORDER_TITLES
#define PS_SECTION_BORDER_TITLES 1
#endif
/* The same indicator policy is used by both table front ends. */
static inline const char *ps_table_section_marker(int unicode,int collapsible,int style,int closed,int selected,int always) {
    if(!collapsible||style==2)return "";
    if(!closed&&!selected&&!always)return "";
    return unicode&&style==0?(closed?"▸ ":"▾ "):(closed?"+ ":"- ");
}
static inline const char *ps_table_junction(int unicode,int above,int below) {
    return !unicode?"+":above?(below?"┼":"┴"):(below?"┬":"─");
}
/* Shared selection policy; NULL override means automatic, "" means hidden. */
static inline const char *ps_table_marker(const char *override_text,int unicode,int categories,int collapsible,int chevrons) {
    if(override_text)return override_text;
    return !categories||!collapsible||!chevrons?(unicode?"\xe2\x96\xb8 ":"> "):"";
}
#endif
