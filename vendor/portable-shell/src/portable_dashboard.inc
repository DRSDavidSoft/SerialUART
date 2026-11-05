#if PS_DASHBOARD
typedef struct {unsigned x,y,w,h,first,visible;} DashboardBox;
static DashboardBox dashboard_box(ps_session *s,unsigned page){
    DashboardBox b;unsigned columns=s->io.columns>=90?2:1;
    unsigned rows=s->panel->count>columns&&s->io.rows>=24?2:1,slots=columns*rows,index;
    b.first=(s->tab/slots)*slots;b.visible=min(slots,s->panel->count-b.first);
    index=page-b.first;b.w=(s->io.columns-3)/columns;b.h=(s->io.rows-6)/rows;
    b.x=2+(index%columns)*(b.w+1);b.y=3+(index/columns)*b.h;return b;
}
static void dashboard_render(ps_session *s){
    unsigned tab=s->tab,selected=s->selected,editing=s->editing,scroll=s->scroll,i,j,editx=0,edity=0,editwidth=0;
    DashboardBox first=dashboard_box(s,tab);Grid active;
    zero(&active,sizeof active);s->mouse_edit_width=0;s->control_header=0;s->last_paint=s->now;
    canvas_begin(s,10,first.first);ansi(s,"\033[?25l");
    for(i=1;i<=s->io.rows;++i){at(s,i,1);ansi(s,"\033[2K");}
    at(s,1,2);ink(s,PS_INK_HEADING);field(s,s->panel->title,s->io.columns-4);reset(s);
    for(i=first.first;i<first.first+first.visible;++i){
        DashboardBox b=dashboard_box(s,i);const ps_control_page *p=s->panel->pages+i;
        unsigned slots=b.h-2,top=0,role=i==tab?PS_INK_HEADING:PS_INK_BORDER;Grid g;
        g.columns=0;g.count=2;g.widths[0]=(b.w-7)*2/5;g.widths[1]=b.w-7-g.widths[0];
        if(i==tab){
            if(selected<scroll)scroll=selected;
            if(selected>=scroll+slots)scroll=selected-slots+1;
            top=scroll;active=g;
        }
        s->tab=i;s->selected=i==tab?selected:~0U;s->editing=i==tab?editing:0;
        view_rule_color(s,b.y,b.x,b.w,0,ps_style_ink(s->theme,(s->io.flags&PS_TRUECOLOR)!=0,0,role));
        at(s,b.y,b.x+2);ink(s,role);out(s," ");field(s,p->title,min(b.w-8,cells(p->title,length(p->title),s->io.flags)));out(s," ");reset(s);
        for(j=0;j<slots;++j){
            at(s,b.y+1+j,b.x);ink(s,role);out(s,(s->io.flags&PS_UNICODE)?"│":"|");out(s," ");
            if(j+top<p->count){
                if(i==tab&&j+top==selected)surface(s,2);
                control_row(s,j+top,&g);reset(s);
                if(i==tab&&j+top==selected){edity=b.y+1+j;editx=b.x+g.widths[0]+5;editwidth=g.widths[1];}
            }else spaces(s,b.w-4);
            out(s," ");ink(s,role);out(s,(s->io.flags&PS_UNICODE)?"│":"|");reset(s);
        }
        view_rule_color(s,b.y+b.h-1,b.x,b.w,1,ps_style_ink(s->theme,(s->io.flags&PS_TRUECOLOR)!=0,0,role));
        if(p->count>slots){at(s,b.y+b.h-1,b.x+b.w-13);ink(s,PS_INK_MUTED);
            ps_number(s,top+1);out(s,"-");ps_number(s,min(top+slots,p->count));out(s,"/");ps_number(s,p->count);reset(s);}
    }
    s->tab=tab;s->selected=selected;s->editing=editing;s->scroll=scroll;
    at(s,s->io.rows-2,2);ink(s,PS_INK_MUTED);field(s,s->notice[0]?s->notice:s->panel->status,s->io.columns-3);reset(s);
    at(s,s->io.rows-1,2);footer_field(s,editing?"Enter Apply  Esc Cancel  ^U Clear":(s->io.flags&PS_UNICODE)?"Tab Panel  ↑/↓ Row  ←/→ Adjust  Enter Edit":"Tab Panel  Up/Down Row  Left/Right Adjust  Enter Edit",s->io.columns-3,0);
    at(s,s->io.rows,2);footer_field(s,editing?"F2 Dialog":"F2 Dialog  Esc Close",s->io.columns-3,0);
    if(editing){
        if(s->dialog)control_overlay(s,&active);
        else{
            unsigned start=0;
            while(start<s->editpos&&cells(s->edit+start,s->editpos-start,s->io.flags)>=editwidth)start=next(s->edit,start);
            s->mouse_edit_x=editx;s->mouse_edit_y=edity;s->mouse_edit_start=start;s->mouse_edit_width=editwidth;
            at(s,edity,editx+cells(s->edit+start,s->editpos-start,s->io.flags));ansi(s,"\033[?25h");
        }
    }
    frame_end(s);
}
static void dashboard_mouse(ps_session *s,const ps_mouse_event *e){
    DashboardBox first=dashboard_box(s,s->tab);unsigned i;
    if(e->release||e->button&60U)return;
    if(e->button&64U){control_key(s,(e->button&1)?PS_KEY_DOWN:PS_KEY_UP);return;}
    if((e->button&3)!=0)return;
    for(i=first.first;i<first.first+first.visible;++i){
        DashboardBox b=dashboard_box(s,i);unsigned top=i==s->tab?s->scroll:0;
        if(e->column>b.x&&e->column<b.x+b.w-1&&e->row>b.y&&e->row<b.y+b.h-1){
            unsigned index=top+e->row-b.y-1;if(index>=s->panel->pages[i].count)return;
            if(i!=s->tab)s->scroll=0;
            s->tab=i;s->selected=index;s->control_header=0;s->dirty=1;return;
        }
    }
}
#endif
PS_API int ps_panel_set_layout(ps_session *s,unsigned layout){
    if(!s||s->editing||layout>PS_PANEL_GRID)return 0;
#if PS_DASHBOARD
    s->dashboard=layout==PS_PANEL_GRID;s->control_header=0;s->scroll=0;s->canvas_kind=0;s->dirty=1;return 1;
#else
    return layout==PS_PANEL_TABS;
#endif
}
