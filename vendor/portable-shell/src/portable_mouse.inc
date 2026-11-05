/* Byte-protocol mouse routing. Coordinates share the renderer's cell geometry. */
static void table_mouse(ps_session *s,const ps_mouse_event *e) {
    unsigned x=e->column,y=e->row,b=e->button,button=b&3,drag=b&32;
    unsigned inside=!!(s->table_style&PS_TABLE_TABS_INSIDE)&&table_height(s)>=15;
    unsigned body=table_body(s),slots=table_slots(s),i,index,total,focus;
    unsigned short items[512];
    Grid g;
    if(table_prompt(s)&&!s->editing&&!e->release&&!drag&&button==0&&!(b&~3U)&&x<=s->io.columns&&y<=s->io.rows){
        s->table_prompt_focus=y>table_height(s);
        s->dirty=1;
        if(s->table_prompt_focus){render(s);return;}
    }
    if(!s->board||!(s->io.flags&PS_INTERACTIVE)||s->io.columns<42||s->io.rows<14)return;
#if PS_DASHBOARD
    if(s->control_view&&s->dashboard&&!table_prompt(s)&&!s->editing){dashboard_mouse(s,e);return;}
#endif
    if(e->release){s->mouse_drag=0;return;}
    if(x>s->io.columns||y>s->io.rows||b&~127U)return;
    if(b&28U)return; /* Let terminal selection/modifier gestures remain non-mutating. */
    if(b&64U){
        if(table_prompt(s)){if(y>table_height(s))return;s->table_prompt_focus=0;}
        if(!s->editing && button<2)for(i=0;i<3;++i)ps_key(s,button?PS_KEY_DOWN:PS_KEY_UP);
        return;
    }
    if(button!=0&&button!=2)return;
    if(drag&&!s->mouse_drag)return;
    if(!drag)s->mouse_drag=1;
    if(s->editing){
        if(button==2&&!drag){ps_key(s,27);return;}
        if(s->control_view&&s->dialog&&!drag){
            unsigned w=min(60,s->io.columns-6),left=(s->io.columns-w)/2+1,top=(s->io.rows-9)/2+1;
            if(y==top+7&&x>=left+3&&x<left+26){
                s->dialog_focus=x<left+13?1:2;ps_key(s,13);return;
            }
        }
        if(y==s->mouse_edit_y&&x>=s->mouse_edit_x&&x<s->mouse_edit_x+s->mouse_edit_width){
            unsigned pos=s->mouse_edit_start,target=x-s->mouse_edit_x;
            while(pos<s->editlen&&cells(s->edit+s->mouse_edit_start,pos-s->mouse_edit_start,s->io.flags)<target)pos=next(s->edit,pos);
            s->editpos=pos;s->dialog_focus=0;s->dirty=1;
        }
        return;
    }
    if(!drag&&y==3+inside){
        for(i=0;i<(s->control_view?s->panel->count:4);++i)
            if(s->mouse_tab_right[i]&&x>=s->mouse_tab_left[i]&&x<=s->mouse_tab_right[i]){
                s->tab=i;s->selected=s->scroll=s->control_header=0;s->dirty=1;return;
            }
        return;
    }
    if(y<body||y>=body+slots||x<2)return;
    total=s->control_view?control_items(s,items,&focus,1):tab_count(s);
    index=s->scroll+y-body;
    if(x>=s->io.columns-1){
        if(total<=slots)return;
        index=(y-body)*(total-1)/(slots>1?slots-1:1);
        while(s->control_view&&index<total&&(items[index]&0x6000))++index;
        if(index>=total)return;
    }
    if(x>=s->io.columns-table_gutter(s)-1&&x<s->io.columns-1)return;
    if(index>=total)return;
    if(s->control_view){
        if(items[index]&0x6000)return;
        s->selected=items[index]&0x1fff;s->control_header=!!(items[index]&0x8000);
    }else s->selected=index;
    s->dirty=1;
    if(x>=s->io.columns-1)return;
    if(s->control_header){if(!drag)ps_key(s,13);return;}
    g=grid_for(s);focus=4;
    for(i=0;i+1<g.count;++i)focus+=g.widths[i]+3;
    if(button==2&&!drag){ps_key(s,s->control_view?PS_KEY_F2:13);return;}
    if(x<focus)return;
    if(s->control_view){
        const ps_control *c=control_selected(s);
        ps_control_value v;
        if(!c)return;
        if(c->kind==PS_CONTROL_LEVEL){
            unsigned observed;
            unsigned span=g.widths[g.count-1]>2?g.widths[g.count-1]-2:1;
            unsigned offset=min(x-focus,span);
            if(!control_writable(s,c,&v))return;
            observed=v.number;
            v.number=c->minimum+(c->maximum-c->minimum)*offset/span;
            if(c->step)v.number=c->minimum+((v.number-c->minimum)/c->step)*c->step;
            if(offset==span)v.number=c->maximum;
            if(v.number!=observed)control_commit(s,c,&v,v.revision);
        }else if(!drag)ps_key(s,13);
    }else if(!drag)ps_key(s,13);
}

/* Click selects; Enter activates. Wheel shares keyboard focus/scroll behavior. */
static void workspace_mouse(ps_session *s,const ps_mouse_event *e){
    unsigned b=e->button,y=e->row,x=e->column;
    if(e->release||b&60U||x>s->io.columns||y>s->io.rows)return;
    if(b&64U){
        if(table_prompt(s)){if(y>table_height(s))return;s->table_prompt_focus=0;}if((b&3)<2)ps_key(s,(b&1)?PS_KEY_DOWN:PS_KEY_UP);return;}
    if((b&3)!=0)return;
#if PS_FORM && PS_MOUSE
    if(s->form){form_mouse(s,x,y);return;}
#endif
#if PS_MENU_LIST_VIEW
    if(s->menu_depth&&menu_list(s)){
        unsigned total;list_index(s,0,&total);
        if(y>=5&&y<5+s->menu_slots*3&&x>=3&&x<s->io.columns-2){
            unsigned selected=s->menu_top+(y-5)/3;
            if(selected<total){s->menu_selected=selected;s->menu_bad=0;list_draw(s);}
        }
        return;
    }
#endif
    if(s->file_count&&s->io.rows>=7&&y>=3&&y<s->io.rows-3&&x>1&&x<s->io.columns-1){
        file_mouse(s,x,y);
    }else if(s->menu_depth&&menu_framed(s)&&y>=4&&y<4+s->menu_slots&&x>s->menu_x&&x<s->menu_x+s->menu_width-1){
        s->menu_selected=s->menu_top+y-4;s->menu_length=s->menu_bad=0;menu_draw(s);
    }
}
