#ifndef PORTABLE_SCREEN_H
#define PORTABLE_SCREEN_H
/* Optional retained cell canvas. Caller owns current and presented cell arrays.
 * Views are clipped rectangles, not independent allocations. Serialize all calls. */
#include "portable_rich.h"
typedef struct {char glyph[5];uint32_t foreground,background;unsigned attributes,span;} ps_cell;
typedef struct {ps_cell *cells,*presented;unsigned columns,rows,valid,synchronized;} ps_screen;
typedef struct {ps_screen *screen;unsigned x,y,width,height;} ps_window;
static inline ps_cell ps_cell_make(uint32_t foreground,uint32_t background,unsigned attributes){
    ps_cell c={{' ',0,0,0,0},0,0,0,1};c.foreground=foreground;c.background=background;c.attributes=attributes;return c;
}
static inline int ps_cell_equal(const ps_cell *a,const ps_cell *b){
    unsigned i;if(a->foreground!=b->foreground||a->background!=b->background||a->attributes!=b->attributes||a->span!=b->span)return 0;
    for(i=0;i<5;++i)if(a->glyph[i]!=b->glyph[i])return 0;
    return 1;
}
static inline int ps_screen_init(ps_screen *s,ps_cell *cells,ps_cell *presented,unsigned columns,unsigned rows){
    unsigned i;ps_cell blank=ps_cell_make(0xabb2bf,0x282c34,0);
    if(!s||!cells||!presented||cells==presented||!columns||!rows||columns>512||rows>256)return 0;
    if((size_t)columns>((size_t)-1)/sizeof(ps_cell)/rows)return 0;
    s->cells=cells;s->presented=presented;s->columns=columns;s->rows=rows;s->valid=0;s->synchronized=1;
    for(i=0;i<columns*rows;++i)cells[i]=blank;
    return 1;
}
static inline ps_window ps_screen_window(ps_screen *s,unsigned x,unsigned y,unsigned width,unsigned height){
    ps_window w={s,x,y,0,0};
    if(s&&x<s->columns&&y<s->rows){w.width=width<s->columns-x?width:s->columns-x;w.height=height<s->rows-y?height:s->rows-y;}return w;
}
static inline void ps_screen_invalidate(ps_screen *s){s->valid=0;}
/* Clear any wide glyph touched by a write, including its continuation cell. */
static inline void ps_screen_unlink(ps_screen *s,unsigned x,unsigned y){
    ps_cell *c=s->cells+(size_t)y*s->columns+x;ps_cell blank=ps_cell_make(c->foreground,c->background,c->attributes);
    if(c->span==0&&x)s->cells[(size_t)y*s->columns+x-1]=blank;
    if(c->span==2&&x+1<s->columns)s->cells[(size_t)y*s->columns+x+1]=blank;
    *c=blank;
}
static inline int ps_window_put(ps_window *w,unsigned x,unsigned y,const char *glyph,ps_cell style){
    uint32_t cp;unsigned n,width,i,ax;
    if(!w||!w->screen||x>=w->width||y>=w->height||!glyph)return 0;
    n=ps_rich_decode(glyph,ps_text_length(glyph),&cp);width=ps_rich_cell_width(cp);
    if(!n||!width||width>w->width-x||cp==0xfffd||cp<32||(cp>=127&&cp<160))return 0;
    ax=w->x+x;ps_screen_unlink(w->screen,ax,w->y+y);
    if(width==2)ps_screen_unlink(w->screen,ax+1,w->y+y);
    for(i=0;i<5;++i)style.glyph[i]=i<n?glyph[i]:0;
    style.span=width;w->screen->cells[(size_t)(w->y+y)*w->screen->columns+ax]=style;
    if(width==2){style.glyph[0]=0;style.span=0;w->screen->cells[(size_t)(w->y+y)*w->screen->columns+ax+1]=style;}return 1;
}
static inline unsigned ps_window_text(ps_window *w,unsigned x,unsigned y,const char *text,ps_cell style){
    unsigned count=0,n,width;uint32_t cp;
    while(*text&&x<w->width){n=ps_rich_decode(text,ps_text_length(text),&cp);width=ps_rich_cell_width(cp);
        if(!ps_window_put(w,x,y,text,style))break;
        x+=width;count+=width;text+=n;
    }return count;
}
static inline void ps_window_fill(ps_window *w,ps_cell style){
    unsigned x,y;for(y=0;y<w->height;++y)for(x=0;x<w->width;++x)ps_window_put(w,x,y," ",style);
}
static inline void ps_window_border(ps_window *w,ps_cell style,unsigned unicode){
    unsigned x,y;if(w->width<2||w->height<2)return;
    for(x=1;x+1<w->width;++x){ps_window_put(w,x,0,unicode?"─":"-",style);ps_window_put(w,x,w->height-1,unicode?"─":"-",style);}
    for(y=1;y+1<w->height;++y){ps_window_put(w,0,y,unicode?"│":"|",style);ps_window_put(w,w->width-1,y,unicode?"│":"|",style);}
    ps_window_put(w,0,0,unicode?"╭":"+",style);ps_window_put(w,w->width-1,0,unicode?"╮":"+",style);
    ps_window_put(w,0,w->height-1,unicode?"╰":"+",style);ps_window_put(w,w->width-1,w->height-1,unicode?"╯":"+",style);
}
/* Scroll complete rows within a view. Positive moves content upward. */
static inline void ps_window_scroll(ps_window *w,int lines,ps_cell blank){
    unsigned x,y,n;int dy;if(!lines||!w->height)return;
    n=lines>0?(unsigned)lines:0u-(unsigned)lines;
    if(n>=w->height){ps_window_fill(w,blank);return;}
    if(!w->width)return;
    for(y=0;y<w->height;++y){
        ps_cell *first=w->screen->cells+(size_t)(w->y+y)*w->screen->columns+w->x;
        if(first->span==0)ps_screen_unlink(w->screen,w->x,w->y+y);
        if(first[w->width-1].span==2)ps_screen_unlink(w->screen,w->x+w->width-1,w->y+y);
    }
    for(y=0;y<w->height;++y){
        unsigned row=lines>0?y:w->height-1-y;
        dy=lines>0?(int)row+(int)n:(int)row-(int)n;
        for(x=0;x<w->width;++x){
            ps_cell c=blank;
            if(dy>=0&&(unsigned)dy<w->height)c=w->screen->cells[(size_t)(w->y+(unsigned)dy)*w->screen->columns+w->x+x];
            /* A clipped half of a wide glyph is blank, never an orphan. */
            if((x==0&&c.span==0)||(x+1==w->width&&c.span==2))c=blank;
            w->screen->cells[(size_t)(w->y+row)*w->screen->columns+w->x+x]=c;
        }
    }
}
/* Copy a pad viewport from a different backing canvas. No overlapping self-copy. */
static inline int ps_window_blit(ps_window *dst,const ps_window *src){
    unsigned x,y,width=dst->width<src->width?dst->width:src->width,height=dst->height<src->height?dst->height:src->height;
    if(dst->screen==src->screen)return 0;
    for(y=0;y<height;++y){
        for(x=0;x<width;++x){
            const ps_cell *c=src->screen->cells+(size_t)(src->y+y)*src->screen->columns+src->x+x;
            if(c->span&&c->span<=width-x)ps_window_put(dst,x,y,c->glyph,*c);
            else if(x==0||c->span==2)ps_window_put(dst,x,y," ",*c);
        }
    }
    return 1;
}
/* Present only changed cells; unchanged frames emit zero bytes. Caller owns
 * alternate screen/cursor lifecycle. Output callback must accept all bytes.
 * Lower-right writes may wrap on a terminal: use a viewport smaller than the
 * physical screen or have the host disable autowrap while presenting. */
static inline unsigned ps_screen_present(ps_screen *s,ps_rich *r){
    unsigned x,y,changed=0,active=0;char position[32];
    for(y=0;y<s->rows;++y)for(x=0;x<s->columns;++x){
        size_t index=(size_t)y*s->columns+x;ps_cell *c=s->cells+index;
        if(s->valid&&ps_cell_equal(c,s->presented+index))continue;
        ++changed;
        if(c->span){
            if(!active){if(s->synchronized)ps_rich_literal(r,"\033[?2026h");active=1;}
            ps_terminal_format(position,sizeof position,"\033[%u;%uH",y+1,x+1);ps_rich_literal(r,position);
            ps_rich_reset(r);ps_rich_color(r,c->foreground,0);ps_rich_color(r,c->background,1);ps_rich_attributes(r,c->attributes);
            ps_rich_literal(r,c->glyph);
        }
        s->presented[index]=*c;
    }
    if(active){ps_rich_reset(r);if(s->synchronized)ps_rich_literal(r,"\033[?2026l");}
    s->valid=1;return changed;
}
#endif
