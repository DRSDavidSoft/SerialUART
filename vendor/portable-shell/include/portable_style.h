#ifndef PORTABLE_STYLE_H
#define PORTABLE_STYLE_H
#include "portable_util.h"
#ifndef PS_SYNC_OUTPUT
#define PS_SYNC_OUTPUT 1
#endif
#ifndef PS_FOOTER_KEYCAPS
#define PS_FOOTER_KEYCAPS 1
#endif
#ifndef PS_TAB_BRACKETS
#define PS_TAB_BRACKETS 0
#endif
#ifndef PS_KEYCAP_STYLE
#define PS_KEYCAP_STYLE "\033[0;1;38;5;255;48;5;238m"
#endif
#ifndef PS_FOOTER_GRID
#define PS_FOOTER_GRID 1
#endif
#ifndef PS_ACTION_BUTTONS
#define PS_ACTION_BUTTONS 1
#endif
#ifndef PS_KEYCAP_PROMINENT_STYLE
#define PS_KEYCAP_PROMINENT_STYLE "\033[0;1;30;43m"
#endif
/* Shared fixed-cell grid. Whole rows fall back to compact spacing when they do
   not fit; no hint is dropped merely to align the columns. UTF-8 continuation
   bytes do not consume cells (footer vocabulary uses single-cell glyphs). */
static inline void ps_style_footer_format(char *dst,unsigned capacity,const char *src,unsigned width,int grid) {
    unsigned at=0;
    if(!capacity)return;
    while(*src&&at+1<capacity){
        const char *end=src,*p;unsigned count=1,max=0,cells=0,column=0;
        while(*end&&*end!='\r'&&*end!='\n')++end;
        for(p=src;p<end;){
            if(*p==' '&&p+1<end&&p[1]==' '){if(cells>max)max=cells;cells=0;++count;while(p<end&&*p==' ')++p;}
            else {if(((unsigned char)*p&192)!=128)++cells;++p;}
        }
        if(cells>max)max=cells;
        {int aligned=grid&&max<20&&(count-1)*20+cells<=width;
        while(src<end&&at+1<capacity){
            if(*src==' '&&src+1<end&&src[1]==' '){unsigned spaces=2;
                while(src<end&&*src==' ')++src;
                if(aligned)spaces=20-column%20;
                while(spaces--&&at+1<capacity){dst[at++]=' ';++column;}
            }else {if(((unsigned char)*src&192)!=128)++column;dst[at++]=*src++;}
        }}
        while((*src=='\r'||*src=='\n')&&at+1<capacity)dst[at++]=*src++;
    }
    dst[at]=0;
}
static inline void ps_style_footer_append(char *dst,unsigned cap,const char *src){unsigned n=0;while(n<cap&&dst[n])++n;while(*src&&n+1<cap)dst[n++]=*src++;if(n<cap)dst[n]=0;}
/* Only shortcut tokens receive keycaps; descriptions retain normal styling. */
static inline int ps_style_key_token(const char *text,unsigned length) {
    static const char *const keys[]={"↑/↓","←/→","↑↓","↑↓←→","←","→","Space","Enter","Esc",
        "Tab","Shift+Tab","Home/End","^U","F2","Del","Up/Down","Up/Dn","Left/Right",
        "Left","Right","Enter/Space","W/R/O","+/-","1-9","n","d","c","/","W","R","O","F","G",
        "^S","^Q/Esc","^Z","^A/^E","S","D","Backspace","0","Ctrl-C"};
    unsigned i,j;
    if(length>=2&&length<=3&&text[0]=='F'){
        for(i=1;i<length&&text[i]>='0'&&text[i]<='9';++i){}
        if(i==length)return 1;
    }
    if(length>=2&&text[0]=='^'){
        for(i=0;i<length;){
            if(i+1>=length||text[i]!='^'||text[i+1]<'A'||text[i+1]>'Z')break;
            i+=2;if(i==length)return 1;if(text[i++]!='/')break;
        }
    }
    for(i=0;i<PS_COUNT_OF(keys);++i){
        for(j=0;j<length&&keys[i][j]&&keys[i][j]==text[j];++j){}
        if(j==length&&!keys[i][j])return 1;
    }
    return 0;
}
/* Shared desktop/device palette. No target detection, allocation or runtime. */
#ifndef PS_STYLE_THEMES
#define PS_STYLE_THEMES 1
#endif
/* Stable role IDs: color follows meaning, never the target architecture. */


enum { PS_THEME_VIOLET, PS_THEME_RED, PS_THEME_CYAN, PS_THEME_ONE_DARK, PS_THEME_DEFAULT };
enum { PS_INK_MUTED=0, PS_INK_COMMAND=1,
       PS_INK_WARNING=2, PS_INK_ERROR=3, PS_INK_TEXT=4, PS_INK_SUCCESS=5,
       PS_INK_HEADING=6, PS_INK_BORDER=7, PS_INK_STRING=8,
       PS_INK_NUMBER=9, PS_INK_INACTIVE=10, PS_INK_ACTION=11 };
static inline const char *ps_style_base(unsigned theme, int rgb) {
    if (!PS_STYLE_THEMES) return "\033[0m";
    if(theme==PS_THEME_DEFAULT)return rgb?"\033[0;38;2;230;233;239;48;2;24;26;31m":"\033[0;38;5;255;48;5;234m";
    if (!rgb) return "\033[0;38;5;255;48;5;234m";
    return theme==3 ? "\033[0;38;2;171;178;191;48;2;40;44;52m" :
           theme==2 ? "\033[0;38;2;220;228;238;48;2;12;18;29m" :
           theme==1 ? "\033[0;38;2;255;226;232;48;2;30;20;25m" :
                      "\033[0;38;2;241;231;255;48;2;23;19;31m";
}
static inline const char *ps_style_ink(unsigned theme, int rgb, int red_border, unsigned kind) {
    if (!PS_STYLE_THEMES) return "";
    static const char *const semantic_rgb[]={
        "\033[38;2;151;160;175m", "\033[38;2;216;160;255m",
        "\033[38;2;255;204;102m", "\033[38;2;255;107;122m",
        "\033[38;2;230;233;239m", "\033[38;2;126;224;163m",
        "\033[38;2;112;211;235m", "\033[38;2;108;119;137m",
        "\033[38;2;166;218;149m", "\033[38;2;255;184;108m",
        "\033[38;2;151;160;175m", "\033[38;2;112;211;235m"};
    static const char *const semantic_ansi[]={
        "\033[38;5;247m", "\033[38;5;183m", "\033[38;5;221m", "\033[38;5;203m",
        "\033[38;5;255m", "\033[38;5;114m", "\033[38;5;80m", "\033[38;5;243m",
        "\033[38;5;150m", "\033[38;5;215m", "\033[38;5;247m", "\033[38;5;80m"};
    if(theme==PS_THEME_DEFAULT){
        if(kind==PS_INK_BORDER&&red_border)kind=PS_INK_ERROR;
        return (rgb?semantic_rgb:semantic_ansi)[kind%12];
    }
    if(kind>=8)kind=1; /* Preserve explicitly selected named palettes. */
    static const char *const original[] = {"\033[38;5;245m", "\033[38;5;80m",    "\033[38;5;223m",
                                           "\033[38;5;203m", "\033[1;38;5;231m", "\033[38;5;151m",
                                           "\033[38;5;80m",  "\033[38;5;245m"};
    static const char *const violet[] = {"\033[38;2;162;150;181m", "\033[38;2;232;121;249m",
                                         "\033[38;2;255;201;157m", "\033[38;2;255;107;138m",
                                         "\033[38;2;241;231;255m", "\033[38;2;134;239;172m",
                                         "\033[38;2;190;153;255m", "\033[38;2;111;78;132m"};
    static const char *const red[] = {"\033[38;2;181;151;161m", "\033[38;2;255;107;138m",
                                      "\033[38;2;255;201;157m", "\033[38;2;255;87;113m",
                                      "\033[38;2;255;226;232m", "\033[38;2;134;239;172m",
                                      "\033[38;2;253;164;175m", "\033[38;2;125;68;87m"};
    static const char *const fallback[] = {"\033[38;5;246m", "\033[38;5;213m", "\033[38;5;223m",
                                           "\033[38;5;203m", "\033[38;5;255m", "\033[38;5;157m",
                                           "\033[38;5;183m", "\033[38;5;97m"};
    static const char *const one_dark[] = {"\033[38;2;120;129;149m", "\033[38;2;198;120;221m",
                                           "\033[38;2;229;192;123m", "\033[38;2;224;108;117m",
                                           "\033[38;2;220;223;228m", "\033[38;2;152;195;121m",
                                           "\033[38;2;198;120;221m", "\033[38;2;92;99;112m"};
    if (kind == PS_INK_BORDER && theme != PS_THEME_CYAN && red_border) {
        return rgb ? "\033[38;2;255;95;115m" : "\033[38;5;203m";
    }
    return theme == PS_THEME_CYAN                  ? original[kind % 8]
             : rgb ? (theme == PS_THEME_ONE_DARK   ? one_dark[kind % 8]
                                               : theme == PS_THEME_RED ? red[kind % 8]
                                                               : violet[kind % 8])
                                            : fallback[kind % 8];
}
static inline const char *ps_style_surface(unsigned theme, int rgb, unsigned active) {
    if (!PS_STYLE_THEMES) return active ? "\033[7m" : "\033[1m";
    if(theme==PS_THEME_DEFAULT){
        if(!rgb)return active==1?"\033[48;5;252;38;5;234m":active==2?"\033[48;5;237m":"\033[48;5;236;38;5;255m";
        return active==1?"\033[48;2;211;218;230m\033[38;2;24;26;31m":active==2?"\033[48;2;48;55;67m":"\033[48;2;36;41;50m\033[38;2;230;233;239m";
    }
    if (theme == PS_THEME_CYAN)
        return active == 1   ? "\033[48;5;23;38;5;159;1m"
                 : active == 2 ? "\033[48;5;236m"
                               : "\033[48;5;23;38;5;159m";
    else if (!rgb)
        return active == 1   ? "\033[48;5;171;38;5;16m"
                 : active == 2 ? "\033[48;5;53m"
                               : "\033[48;5;53;38;5;225m";
    else if (theme == PS_THEME_ONE_DARK)
        return active == 1   ? "\033[48;2;198;120;221m\033[38;2;33;37;43m"
                 : active == 2 ? "\033[48;2;62;68;81m"
                               : "\033[48;2;33;37;43m\033[38;2;171;178;191m";
    else if (theme == PS_THEME_RED)
        return active == 1   ? "\033[48;2;251;113;133m\033[38;2;42;16;24m"
                 : active == 2 ? "\033[48;2;72;35;47m"
                               : "\033[48;2;55;29;39m\033[38;2;253;194;205m";
    else
        return active == 1   ? "\033[48;2;209;111;232m\033[38;2;30;16;40m"
                 : active == 2 ? "\033[48;2;62;37;78m"
                               : "\033[48;2;45;31;59m\033[38;2;225;194;247m";
}

/* Action affordances share semantic colors; named palettes remain opt-in. */
enum { PS_BUTTON_IDLE, PS_BUTTON_FOCUSED, PS_BUTTON_DISABLED, PS_BUTTON_PENDING };
static inline const char *ps_style_button(unsigned theme, int rgb, unsigned state) {
    if (!PS_STYLE_THEMES) return state==PS_BUTTON_FOCUSED?"\033[0;1;7m":"\033[0;1m";
    if (state==PS_BUTTON_DISABLED)
        return rgb?"\033[0;38;2;151;160;175;48;2;36;41;50m":"\033[0;38;5;247;48;5;236m";
    if (state==PS_BUTTON_PENDING)
        return rgb?"\033[0;1;38;2;255;204;102;48;2;68;53;29m":"\033[0;1;38;5;221;48;5;58m";
    if (theme!=PS_THEME_DEFAULT) return ps_style_surface(theme,rgb,state==PS_BUTTON_FOCUSED?1:0);
    if (state==PS_BUTTON_FOCUSED)
        return rgb?"\033[0;1;38;2;24;26;31;48;2;112;211;235m":"\033[0;1;38;5;234;48;5;80m";
    return rgb?"\033[0;1;38;2;112;211;235;48;2;26;62;73m":"\033[0;1;38;5;80;48;5;23m";
}

/* Prompt-only consumers must not retain whole table palettes in static RAM. */
static inline const char *ps_style_prompt(unsigned theme, int rgb) {
    if (!PS_STYLE_THEMES) return "";
    if (theme==PS_THEME_DEFAULT) return rgb?"\033[38;2;112;211;235m":"\033[38;5;80m";
    if (theme==2) return "\033[38;5;80m";
    if (!rgb) return "\033[38;5;213m";
    return theme==3?"\033[38;2;198;120;221m":theme==1?"\033[38;2;255;107;138m":"\033[38;2;232;121;249m";
}

#endif
