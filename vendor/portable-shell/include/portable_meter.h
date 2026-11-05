#ifndef PORTABLE_METER_H
#define PORTABLE_METER_H
/* MIT. Byte-only C99/C++11 meter: caller owns state, transport and readback.
 * No allocation or platform dependencies. Text pointers are borrowed.
 * Rendering starts at column one; each emitted row ends with CRLF.
 * Hosts route keys/clicks to the focused control. Requests never change readback. */
#include "portable_terminal.h"
#include <stdint.h>
enum { PS_METER_LINE, PS_METER_BLOCKS, PS_METER_TICKS, PS_METER_CHEVRONS };
/* OR into style: opt-in paired markers for adjustable, centered block labels. */
enum { PS_METER_ADJUST_MARKERS = 256, PS_METER_STYLE_MASK = 255 };
enum { PS_METER_TEXT_ABOVE, PS_METER_TEXT_INSIDE, PS_METER_TEXT_NONE };
enum { PS_METER_DECREASE, PS_METER_INCREASE, PS_METER_MINIMUM, PS_METER_MAXIMUM };
typedef struct {
    uint32_t minimum, maximum, observed, requested, step;
    unsigned adjustable, show_requested;
} ps_meter;
typedef struct {
    unsigned width, style, text, panel, unicode, color;
    const char *title, *subtitle;
} ps_meter_style;
/* Return nonzero to accept a request. NULL permits a local draft. Callback must
 * not reenter this control. Application subsequently supplies observed readback. */
typedef int (*ps_meter_request_fn)(void *, uint32_t);
static inline uint32_t ps_meter_clamp(const ps_meter *m, uint32_t v) {
    return v<m->minimum?m->minimum:v>m->maximum?m->maximum:v;
}
/* Exact floor(value*cells/span), without wide multiply/divide or overflow. */
static inline unsigned ps_meter_fraction(uint32_t value, uint32_t span, unsigned cells) {
    uint32_t rem=0; unsigned result=0,i;
    if (!span) return 0;
    if (value>=span) return cells;
    for(i=0;i<cells;++i) {
        if(rem>=span-value){rem-=span-value;++result;}else rem+=value;
    }
    return result;
}
static inline int ps_meter_request(ps_meter *m, uint32_t value, ps_meter_request_fn fn, void *ctx) {
    if(!m || !m->adjustable || m->maximum<=m->minimum) return 0;
    value=ps_meter_clamp(m,value);
    if(value==m->requested || (fn&&!fn(ctx,value))) return 0;
    m->requested=value; return 1;
}
static inline int ps_meter_adjust(ps_meter *m, unsigned action, ps_meter_request_fn fn, void *ctx) {
    uint32_t v,step;
    if(!m || m->maximum<=m->minimum) return 0;
    v=ps_meter_clamp(m,m->requested);step=m->step?m->step:1;
    switch(action){
    case PS_METER_DECREASE:v=step>v-m->minimum?m->minimum:v-step;break;
    case PS_METER_INCREASE:v=step>m->maximum-v?m->maximum:v+step;break;
    case PS_METER_MINIMUM:v=m->minimum;break;
    case PS_METER_MAXIMUM:v=m->maximum;break;
    default:return 0;
    }
    return ps_meter_request(m,v,fn,ctx);
}
/* Mouse/drag adapter: zero-based position in a track with 2..240 cells. */
static inline int ps_meter_position(ps_meter *m,unsigned position,unsigned cells,ps_meter_request_fn fn,void *ctx){
    uint32_t span,v;unsigned divisions;
    if(!m||m->maximum<=m->minimum||cells<2||cells>240)return 0;
    divisions=cells-1;if(position>divisions)position=divisions;
    span=m->maximum-m->minimum;
    v=(span/divisions)*position+(span%divisions)*position/divisions;
    return ps_meter_request(m,m->minimum+v,fn,ctx);
}
static inline const char *ps_meter_cell(unsigned style,unsigned filled,unsigned unicode){
    style &= PS_METER_STYLE_MASK;
    if(style==PS_METER_TICKS)return unicode?(filled?"┃":"┊"):(filled?"|":":");
    return unicode?(style==PS_METER_BLOCKS?(filled?"█":"░"):(filled?"━":"─")):(filled?(style==PS_METER_BLOCKS?"#":"="):".");
}
/* Spaced segments share their geometry between standalone and table renderers. */
static inline const char *ps_meter_track_cell(unsigned style,unsigned filled,unsigned unicode,unsigned index){
    return (style&PS_METER_STYLE_MASK)==PS_METER_TICKS&&(index&1)?" ":ps_meter_cell(style,filled,unicode);
}
/* A fixed-cell-width value field, including chevrons. Input is sanitized ASCII;
 * generated delimiters use Unicode. Capacity includes UTF-8 expansion and NUL. */
static inline int ps_meter_value_field(char *dst,unsigned capacity,unsigned width,const char *value,unsigned unicode){
    char clean[241];unsigned n=0,i,k=0,left,edge=unicode?1:2,available;
    const char *open=unicode?"«":"<<",*close=unicode?"»":">>";
    if(!dst||!value||width<6||width>240||capacity<width+(unicode?3:1))return 0;
    while(n<240&&value[n]){clean[n]=value[n];++n;}clean[n]=0;
    ps_terminal_text(clean,sizeof clean,clean,n);n=0;
    available=width-2*edge-2;while(clean[n]&&n<available)++n;
    left=(available-n)/2;
    for(i=0;i<left;++i)dst[k++]=' ';
    for(i=0;open[i];++i)dst[k++]=open[i];
    dst[k++]=' ';for(i=0;i<n;++i)dst[k++]=clean[i];dst[k++]=' ';
    for(i=0;close[i];++i)dst[k++]=close[i];
    for(i=left+n;i<available;++i)dst[k++]=' ';
    dst[k]=0;return 1;
}
static inline void ps_meter_emit(ps_terminal_write write,void *ctx,const char *s){
    size_t n=0;while(s[n])++n;write(ctx,(const unsigned char *)s,n);
}
static inline void ps_meter_edge(ps_terminal_write write,void *ctx,const ps_meter_style *s,unsigned width,int bottom){
    unsigned i;ps_meter_emit(write,ctx,s->unicode?(bottom?"╰":"╭"):"+");
    for(i=0;i<width;++i)ps_meter_emit(write,ctx,s->unicode?"─":"-");
    ps_meter_emit(write,ctx,s->unicode?(bottom?"╯\r\n":"╮\r\n"):"+\r\n");
}
/* Returns emitted row count; invalid range/width emits nothing. Labels are
 * control-stripped ASCII (portable_terminal_text policy), clipped and padded.
 * Generated borders/tracks use Unicode when enabled. Color never conveys state
 * alone: observed and target values remain explicitly labeled. */
static inline unsigned ps_meter_render(const ps_meter *m,const ps_meter_style *s,ps_terminal_write write,void *ctx){
    char value[96],labels[3][241];unsigned width,filled,rows=0,i,j,count=0,n,start,kind,markers=0;
    uint32_t observed;
    if(!m||!s||!write||m->maximum<=m->minimum||s->width<8||s->width>240)return 0;
    width=s->width-(s->panel?2:0);observed=ps_meter_clamp(m,m->observed);
    kind=s->style&PS_METER_STYLE_MASK;
    filled=ps_meter_fraction(observed-m->minimum,m->maximum-m->minimum,width);
    if(m->show_requested)ps_terminal_format(value,sizeof value,"%u%%  Value %u  Target %u",ps_meter_fraction(observed-m->minimum,m->maximum-m->minimum,100),(unsigned)observed,(unsigned)ps_meter_clamp(m,m->requested));
    else ps_terminal_format(value,sizeof value,"%u%%  %u / %u",ps_meter_fraction(observed-m->minimum,m->maximum-m->minimum,100),(unsigned)observed,(unsigned)m->maximum);
    if(s->title&&*s->title)ps_terminal_text(labels[count++],sizeof labels[0],s->title,1024);
    if(s->subtitle&&*s->subtitle)ps_terminal_text(labels[count++],sizeof labels[0],s->subtitle,1024);
    if(s->text==PS_METER_TEXT_ABOVE)ps_terminal_text(labels[count++],sizeof labels[0],value,sizeof value);
    if(s->color)ps_meter_emit(write,ctx,"\033[0;37m");
    if(s->panel){ps_meter_edge(write,ctx,s,width,0);++rows;}
    for(i=0;i<count;++i){
        if(s->panel)ps_meter_emit(write,ctx,s->unicode?"│":"|");
        n=0;while(labels[i][n]&&n<width)++n;
        write(ctx,(const unsigned char *)labels[i],n);
        for(j=n;j<width;++j)ps_meter_emit(write,ctx," ");
        if(s->panel)ps_meter_emit(write,ctx,s->unicode?"│":"|");
        ps_meter_emit(write,ctx,"\r\n");++rows;
    }
    n=0;if(s->text==PS_METER_TEXT_INSIDE)while(value[n]&&n<width)++n;
    if(kind==PS_METER_BLOCKS && (s->style&PS_METER_ADJUST_MARKERS) &&
       m->adjustable && n && n+4<=width){markers=1;n+=4;}
    start=(width-n)/2;
    if(s->panel)ps_meter_emit(write,ctx,s->unicode?"│":"|");
    if(kind==PS_METER_CHEVRONS){
        char field[244];ps_meter_value_field(field,sizeof field,width,value,s->unicode);
        if(s->color)ps_meter_emit(write,ctx,"\033[33m");
        ps_meter_emit(write,ctx,field);
    }else for(i=0;i<width;++i){
        char letter[2]={0,0};const char *cell=" ";
        unsigned in_label=n&&i>=start&&i<start+n;
        if(in_label){
            unsigned offset=i-start;
            if(markers&&(offset==0||offset==n-1))
                cell=s->unicode?(offset==0?"◀":"▶"):(offset==0?"<":">");
            else if(!markers || (offset>=2&&offset<n-2)){
                letter[0]=value[offset-(markers?2:0)];cell=letter;
            }
        }
        /* A colored solid track uses cell backgrounds throughout. Mixing font
         * block glyphs with label backgrounds produces font-dependent seams. */
        if(s->color&&kind==PS_METER_BLOCKS){
            ps_meter_emit(write,ctx,i<filled?"\033[30;42m":"\033[97;100m");
            ps_meter_emit(write,ctx,cell);
        }else if(in_label){
            if(s->color)ps_meter_emit(write,ctx,i<filled?"\033[30;42m":"\033[97;40m");
            ps_meter_emit(write,ctx,cell);
        }else{
            if(s->color)ps_meter_emit(write,ctx,i<filled?"\033[0;32m":"\033[0;90m");
            ps_meter_emit(write,ctx,ps_meter_track_cell(s->style,i<filled,s->unicode,i));
        }
    }
    if(s->color)ps_meter_emit(write,ctx,"\033[0;37m");
    if(s->panel)ps_meter_emit(write,ctx,s->unicode?"│":"|");
    if(s->color)ps_meter_emit(write,ctx,"\033[0m");
    ps_meter_emit(write,ctx,"\r\n");++rows;
    if(s->panel){ps_meter_edge(write,ctx,s,width,1);++rows;}
    return rows;
}
#endif
