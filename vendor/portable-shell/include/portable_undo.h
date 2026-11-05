#ifndef PORTABLE_UNDO_H
#define PORTABLE_UNDO_H
#include "portable_terminal.h"
/* Bounded snapshot journal. Caller owns (depth+1)*capacity bytes and cursor slots.
 * No allocation. Capacity includes NUL. Bind before use; not thread-safe. */
typedef struct {
    char *frames;unsigned *positions;
    unsigned capacity,slots,count,index;
} ps_undo;
static inline void ps_undo_bind(ps_undo *u,char *frames,unsigned *positions,unsigned capacity,unsigned slots){
    u->frames=frames;u->positions=positions;u->capacity=capacity;u->slots=slots;u->count=u->index=0;
}
static inline void ps_undo_clear(ps_undo *u){
    if(u->frames)ps_secure_clear(u->frames,(size_t)u->capacity*u->slots);
    if(u->positions)ps_secure_clear(u->positions,(size_t)u->slots*sizeof(unsigned));
    u->count=u->index=0;
}
static inline int ps_undo_record(ps_undo *u,const char *line,unsigned cursor){
    size_t n=ps_text_length(line),i;char *frame;
    if(!u->frames||!u->positions||!u->slots||n>=u->capacity||cursor>n)return 0;
    /* Don't checkpoint a UTF-8 sequence while its remaining bytes are arriving. */
    if(n){size_t lead=n-1;unsigned char c;unsigned want;
        while(lead&&(line[lead]&0xc0)==0x80)--lead;
        c=(unsigned char)line[lead];want=c>=0xf0?4:c>=0xe0?3:c>=0xc0?2:1;
        if(n-lead<want)return 0;
    }
    if(u->count){frame=u->frames+(size_t)u->index*u->capacity;
        for(i=0;i<=n&&frame[i]==line[i];++i){}
        if(i>n){u->positions[u->index]=cursor;return 1;}
        u->count=u->index+1;
    }
    if(u->count==u->slots){
        ps_bytes_move(u->frames,u->frames+u->capacity,(size_t)(u->slots-1)*u->capacity);
        ps_bytes_move(u->positions,u->positions+1,(size_t)(u->slots-1)*sizeof(unsigned));--u->count;
    }
    u->index=u->count++;frame=u->frames+(size_t)u->index*u->capacity;
    ps_secure_clear(frame,u->capacity);ps_bytes_copy(frame,line,n+1);u->positions[u->index]=cursor;return 1;
}
static inline int ps_undo_step(ps_undo *u,int redo,char *line,unsigned *cursor){
    if(!u->count||(redo?u->index+1>=u->count:!u->index))return 0;
    if(redo)++u->index;else --u->index;
    ps_bytes_copy(line,u->frames+(size_t)u->index*u->capacity,u->capacity);
    *cursor=u->positions[u->index];return 1;
}
#endif
