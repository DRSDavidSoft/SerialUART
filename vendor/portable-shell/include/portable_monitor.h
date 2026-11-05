#ifndef PORTABLE_MONITOR_H
#define PORTABLE_MONITOR_H
/* Byte-only, allocation-free live logs and task progress. Caller owns all storage,
 * timestamps, event scheduling and synchronization. One serialized owner per view. */
#include "portable_meter.h"
#include "portable_style.h"
#ifndef PS_MONITOR_TEXT
#define PS_MONITOR_TEXT 160
#endif
#ifndef PS_MONITOR_LABEL
#define PS_MONITOR_LABEL 48
#endif
#ifndef PS_MONITOR_TASKS
#define PS_MONITOR_TASKS 1
#endif
#if PS_MONITOR_TEXT < 8 || PS_MONITOR_LABEL < 8
#error "Monitor text buffers must hold at least 8 bytes"
#endif
enum { PS_LOG_DEBUG, PS_LOG_INFO, PS_LOG_SUCCESS, PS_LOG_WARNING, PS_LOG_ERROR };
enum { PS_TASK_WAITING, PS_TASK_RUNNING, PS_TASK_DONE, PS_TASK_FAILED };
enum { PS_MONITOR_PAUSE, PS_MONITOR_RESUME, PS_MONITOR_OLDER, PS_MONITOR_NEWER, PS_MONITOR_LIVE, PS_MONITOR_FILTER };
typedef struct {uint32_t time;unsigned level;char text[PS_MONITOR_TEXT];} ps_log_record;
typedef struct {unsigned id,state;uint32_t done,total;char label[PS_MONITOR_LABEL];} ps_monitor_task;
typedef struct {
    ps_log_record *records;unsigned capacity,count,head,dropped,offset,paused,minimum;
#if PS_MONITOR_TASKS
    ps_monitor_task *tasks;unsigned task_capacity,task_count;
#endif
} ps_monitor;
typedef struct {unsigned columns,rows,unicode,color;const char *title;} ps_monitor_style;
static inline void ps_monitor_copy(char *dst,unsigned capacity,const char *src){
    unsigned n=0;if(!src)src="";
    while(n+1<capacity&&src[n]){dst[n]=src[n];++n;}dst[n]=0;
    ps_terminal_text(dst,capacity,dst,n);
}
static inline ps_monitor ps_monitor_init(ps_log_record *records,unsigned count,ps_monitor_task *tasks,unsigned task_count){
    ps_monitor m={0};m.records=records;m.capacity=records?count:0;
#if PS_MONITOR_TASKS
    m.tasks=tasks;m.task_capacity=tasks?task_count:0;
#else
    (void)tasks;(void)task_count;
#endif
    return m;
}
static inline const char *ps_log_name(unsigned level){
    switch(level){case PS_LOG_DEBUG:return "DEBUG";case PS_LOG_SUCCESS:return "OK";
        case PS_LOG_WARNING:return "WARN";case PS_LOG_ERROR:return "ERROR";default:return "INFO";}
}
static inline unsigned ps_log_ink(unsigned level){
    return level==PS_LOG_ERROR?PS_INK_ERROR:level==PS_LOG_WARNING?PS_INK_WARNING:
        level==PS_LOG_SUCCESS?PS_INK_SUCCESS:level==PS_LOG_DEBUG?PS_INK_MUTED:PS_INK_TEXT;
}
/* Ingestion continues while log follow is paused. Oldest entries are overwritten;
 * dropped counts discarded history, not dropped input bytes. */
static inline void ps_monitor_append(ps_monitor *m,uint32_t time,unsigned level,const char *text){
    ps_log_record *r;if(!m||!m->capacity)return;
    r=m->records+m->head;r->time=time;r->level=level>PS_LOG_ERROR?PS_LOG_INFO:level;
    ps_monitor_copy(r->text,sizeof r->text,text);m->head=(m->head+1)%m->capacity;
    if(m->count<m->capacity)++m->count;else if(m->dropped!=(unsigned)-1)++m->dropped;
    if((m->offset||m->paused)&&m->offset+1<m->count)++m->offset;
}
static inline int ps_monitor_update(ps_monitor *m,unsigned id,unsigned state,uint32_t done,uint32_t total,const char *label){
#if PS_MONITOR_TASKS
    unsigned i;ps_monitor_task *task;if(!m||state>PS_TASK_FAILED||done>total)return 0;
    for(i=0;i<m->task_count&&m->tasks[i].id!=id;++i){}
    if(i==m->task_count){if(i==m->task_capacity)return 0;++m->task_count;}
    task=m->tasks+i;task->id=id;task->state=state;task->done=done;task->total=total;
    ps_monitor_copy(task->label,sizeof task->label,label);return 1;
#else
    (void)m;(void)id;(void)state;(void)done;(void)total;(void)label;return 0;
#endif
}
static inline void ps_monitor_action(ps_monitor *m,unsigned action){
    if(!m)return;
    switch(action){
    case PS_MONITOR_PAUSE:m->paused=1;break;
    case PS_MONITOR_RESUME:m->paused=0;m->offset=0;break;
    case PS_MONITOR_OLDER:if(m->offset+1<m->count)++m->offset;break;
    case PS_MONITOR_NEWER:if(m->offset)--m->offset;break;
    case PS_MONITOR_LIVE:m->offset=0;m->paused=0;break;
    case PS_MONITOR_FILTER:m->minimum=(m->minimum+1)%5;m->offset=0;break;
    }
}
static inline void ps_monitor_emit(ps_terminal_write write,void *ctx,const char *text){ps_meter_emit(write,ctx,text);}
static inline void ps_monitor_ink(ps_terminal_write write,void *ctx,const ps_monitor_style *s,unsigned role){
    if(s->color)ps_monitor_emit(write,ctx,ps_style_ink(PS_THEME_DEFAULT,1,0,role));
}
static inline void ps_monitor_at(ps_terminal_write write,void *ctx,unsigned row,unsigned col){
    char b[32];ps_terminal_format(b,sizeof b,"\033[%u;%uH",row,col);ps_monitor_emit(write,ctx,b);
}
static inline void ps_monitor_field(ps_terminal_write write,void *ctx,const char *text,unsigned width){
    unsigned i=0;while(i<width&&text[i]){write(ctx,(const unsigned char *)text+i,1);++i;}
    while(i++<width)ps_monitor_emit(write,ctx," ");
}
/* Plain append-only output: no cursor, color, alternate-screen or animation bytes. */
static inline void ps_log_write(ps_terminal_write write,void *ctx,uint32_t time,unsigned level,const char *text){
    char line[PS_MONITOR_TEXT],prefix[48];if(!write)return;
    ps_monitor_copy(line,sizeof line,text);
    ps_terminal_format(prefix,sizeof prefix,"[%u ms] %s  ",(unsigned)time,ps_log_name(level));
    ps_monitor_emit(write,ctx,prefix);ps_monitor_emit(write,ctx,line);ps_monitor_emit(write,ctx,"\r\n");
}
static inline void ps_monitor_open(ps_terminal_write write,void *ctx){if(write)ps_monitor_emit(write,ctx,"\033[?1049h\033[?25l");}
static inline void ps_monitor_close(ps_terminal_write write,void *ctx){if(write)ps_monitor_emit(write,ctx,"\033[0m\033[?25h\033[?1049l");}
/* Render a fixed viewport, synchronized to avoid exposing intermediate clears.
 * Pausing holds the log anchor; task updates and ingestion continue. No terminal capabilities or OS are assumed. */
static inline unsigned ps_monitor_render(const ps_monitor *m,const ps_monitor_style *s,uint32_t now,ps_terminal_write write,void *ctx){
    unsigned i,row=4,log_start,slots,n=0,skip,idx,shown=0,width;
    char b[256],title[PS_MONITOR_TEXT];
    if(!m||!s||!write||s->columns<40||s->columns>240||s->rows<12||s->rows>80)return 0;
    width=s->columns-4;
    ps_monitor_emit(write,ctx,"\033[?2026h\033[?25l");
    if(s->color)ps_monitor_emit(write,ctx,ps_style_base(PS_THEME_DEFAULT,1));
    for(i=1;i<=s->rows;++i){ps_monitor_at(write,ctx,i,1);ps_monitor_emit(write,ctx,"\033[2K");}
    ps_monitor_at(write,ctx,1,3);ps_monitor_ink(write,ctx,s,PS_INK_HEADING);
    ps_monitor_copy(title,sizeof title,s->title?s->title:"Live monitor");ps_monitor_field(write,ctx,title,width);
    ps_monitor_at(write,ctx,2,3);ps_monitor_ink(write,ctx,s,PS_INK_MUTED);
    ps_terminal_format(b,sizeof b,"%s | %u ms | %u records | %u overwritten | %s+",
        m->paused?"PAUSED":m->offset?"HISTORY":"LIVE",(unsigned)now,m->count,m->dropped,ps_log_name(m->minimum));ps_monitor_field(write,ctx,b,width);
#if PS_MONITOR_TASKS
    for(i=0;i<m->task_count&&row+6<s->rows;i++){
        const ps_monitor_task *t=m->tasks+i;unsigned j,bar=width>60?28:12;
        unsigned filled=t->total?ps_meter_fraction(t->done,t->total,bar):0;
        const char *state=t->state==PS_TASK_DONE?"DONE":t->state==PS_TASK_FAILED?"FAILED":t->state==PS_TASK_WAITING?"WAIT":"RUN";
        unsigned role=t->state==PS_TASK_FAILED?PS_INK_ERROR:t->state==PS_TASK_DONE?PS_INK_SUCCESS:PS_INK_ACTION;
        ps_monitor_at(write,ctx,row++,3);ps_monitor_ink(write,ctx,s,role);
        ps_terminal_format(b,sizeof b,"%s  %s",state,t->label);ps_monitor_field(write,ctx,b,width);
        ps_monitor_at(write,ctx,row++,3);
        if(t->total){
            for(j=0;j<bar;++j){ps_monitor_ink(write,ctx,s,j<filled?role:PS_INK_MUTED);ps_monitor_emit(write,ctx,ps_meter_track_cell(PS_METER_TICKS,j<filled,s->unicode,j));}
            ps_monitor_ink(write,ctx,s,PS_INK_NUMBER);
            ps_terminal_format(b,sizeof b,"  %u%%  %u / %u",ps_meter_fraction(t->done,t->total,100),(unsigned)t->done,(unsigned)t->total);
            ps_monitor_field(write,ctx,b,width-bar);
        }else{
            static const char *const spinner[]={"|","/","-","\\"};
            ps_monitor_emit(write,ctx,t->state==PS_TASK_RUNNING?spinner[(now/125)%4]:" ");
            ps_monitor_emit(write,ctx,"  ");ps_monitor_field(write,ctx,t->state==PS_TASK_RUNNING?"Working (total unknown)":"No progress total",width-3);
        }
    }
    if(i<m->task_count){ps_monitor_at(write,ctx,row++,3);ps_monitor_ink(write,ctx,s,PS_INK_MUTED);ps_terminal_format(b,sizeof b,"+ %u tasks outside viewport",m->task_count-i);ps_monitor_field(write,ctx,b,width);}
#endif
    ps_monitor_at(write,ctx,row++,3);ps_monitor_ink(write,ctx,s,PS_INK_HEADING);ps_monitor_emit(write,ctx,"EVENTS");
    log_start=row;slots=s->rows-3-log_start;
    skip=m->offset;
    /* Count eligible records through the history anchor, then select its last page. */
    for(i=skip;i<m->count;++i){idx=(m->head+m->capacity-1-i)%m->capacity;if(m->records[idx].level>=m->minimum)++n;}
    if(n>slots)n=slots;
    for(i=skip;i<m->count&&shown<n;++i){
        const ps_log_record *r;idx=(m->head+m->capacity-1-i)%m->capacity;r=m->records+idx;
        if(r->level<m->minimum)continue;
        ps_monitor_at(write,ctx,log_start+n-1-shown++,3);ps_monitor_ink(write,ctx,s,PS_INK_MUTED);
        ps_terminal_format(b,sizeof b,"%u",(unsigned)r->time);ps_monitor_field(write,ctx,b,10);
        ps_monitor_ink(write,ctx,s,ps_log_ink(r->level));ps_monitor_field(write,ctx,ps_log_name(r->level),7);
        ps_monitor_field(write,ctx,r->text,width-17);
    }
    if(!n){ps_monitor_at(write,ctx,log_start,3);ps_monitor_ink(write,ctx,s,PS_INK_MUTED);ps_monitor_emit(write,ctx,"No events at this level");}
    ps_monitor_at(write,ctx,s->rows-1,3);ps_monitor_ink(write,ctx,s,PS_INK_MUTED);
    ps_monitor_field(write,ctx,"History: older / newer   Live: resume + follow   Level: filter",width);
    if(s->color)ps_monitor_emit(write,ctx,"\033[0m");
    ps_monitor_emit(write,ctx,"\033[?2026l");return s->rows;
}
#endif
