unsigned titleBorderShift(const Snapshot &s) const {
    if(sections()&&sectionBorderTitles_&&!PS_DEVICE_VIEW_PLAIN_ROWS)
        for(unsigned i=0;i<rowCount(s);++i)if(sectionStart(s,i))return 1;
    return 0;
}
unsigned tableCapacity(const Snapshot &s,unsigned w) const {
    const unsigned shift=tabsInsideTable_?0:1;
    const unsigned diagnostics=page_==Led&&!PS_DEVICE_VIEW_MATRIX?(s.lines<24?5:9):page_==Game?3:page_==Pixel?2:0;
    unsigned visible=s.lines>9+shift+diagnostics-(PS_DEVICE_VIEW_PLAIN_ROWS?2:0)?s.lines-9-shift-diagnostics+(PS_DEVICE_VIEW_PLAIN_ROWS?2:0):1;
    visible+=titleBorderShift(s);
    if(visible>PS_DEVICE_VIEW_MAX_ROWS)visible=PS_DEVICE_VIEW_MAX_ROWS;
    const unsigned reserve=8192U+unsigned(s.lines)*110U;
    const unsigned budget=(PS_DEVICE_VIEW_FRAME_BYTES>reserve?PS_DEVICE_VIEW_FRAME_BYTES-reserve:1)/(w+450U);
    if(visible>budget)visible=budget?budget:1;
    return visible;
}
unsigned tableGutter(const Snapshot &s,unsigned width,unsigned offset) const {
    if(!ansi_||compactScrollbar_||width-offset<64)return 0;
    if(reserveScrollbar_)return 2;
    uint16_t items[PS_DEVICE_VIEW_MAX_ROWS*4];unsigned focus;
    return sectionItems(s,items,focus,true)>tableCapacity(s,width-1-offset)?2:0;
}
/* DeviceView members. Shared device presentation, independent of CPU/transport.
 * Typed controls and palettes follow portable_controls.inc; applications retain
 * validators, authorization, pending state and all physical side effects. */
static bool decimal(const char *s, uint32_t &n) {
    n=0;if(!s||!*s)return false;
    for(;*s;++s){if(*s<'0'||*s>'9'||n>(UINT32_MAX-unsigned(*s-'0'))/10U)return false;
        n=n*10U+unsigned(*s-'0');}return true;
}
static int toggle(const char *s) {
    if(ps_text_equal(s,"true")||ps_text_equal(s,"on")||ps_text_equal(s,"1"))return 1;
    if(ps_text_equal(s,"false")||ps_text_equal(s,"off")||ps_text_equal(s,"0"))return 0;
    return -1;
}
static void numberText(char *out,uint32_t n) {
    char reverse[10];unsigned count=0;
    do {reverse[count++]=char('0'+n%10U);n/=10U;}while(n);
    unsigned at=0;while(count)out[at++]=reverse[--count];out[at]=0;
}
static unsigned levelPercent(uint32_t offset,uint32_t span){
    if(!span)return 0;
    unsigned result=0;const uint32_t whole=span/100,remainder=span%100;
    while(result<100&&offset>=(result+1)*whole+((result+1)*remainder+99)/100)++result;
    return result;
}
static uint32_t rowHash(const Row &r) {
    uint32_t h=2166136261U;
    auto add=[&](const char *text){
        for(const char *p=text;p&&*p;++p){h^=uint8_t(*p);h*=16777619U;}
        h*=16777619U; // Hash a zero delimiter, separating fields including empty values.
    };
    // Secret values are deliberately absent; providers must revalidate writes.
    add(r.secret?"":r.value);
    h^=unsigned(r.kind);h*=16777619U;
    if(r.kind==Choice){
        h^=r.choiceCount;h*=16777619U;
        for(unsigned i=0;r.choices&&i<r.choiceCount;++i){
            add(r.choices[i]);
            if(r.choiceLabels)add(r.choiceLabels[i]);
        }
    }
    return h;
}
bool validateChoice(const Snapshot &s) {
    if(!choiceMenu_||!editing_)return true;
    const Row *r=currentRow(s);
    if(r&&r->kind==Choice&&ps_text_equal(r->key,editKey_)&&r->choices&&
       choiceIndex_<r->choiceCount&&!r->secret&&!r->pending&&r->editable&&observed_==rowHash(*r))return true;
    clearDraft();setNotice("State changed; reopen editor");dirty_=full_=true;return false;
}
static bool validValue(const Row &r,const char *value) {
    if(r.kind==Toggle)return toggle(value)>=0;
    if(r.kind==Level){uint32_t n;return decimal(value,n)&&n>=r.minimum&&n<=r.maximum;}
    if(r.kind==Choice){for(unsigned i=0;r.choices&&i<r.choiceCount;++i)if(ps_text_equal(value,r.choices[i]))return true;return false;}
    return true;
}
bool adjust(uint8_t key,const Snapshot &s,Action &a) {
    const Row *r=currentRow(s);
    if(!r||!r->editable||r->secret||r->pending||!keySafe(r->key))return false;
    if(r->kind==Toggle&&key==' '){
        const int v=toggle(r->value);if(v<0){setNotice("State unknown");return false;}
        copy(a.value,sizeof(a.value),ps_text_equal(r->value,"on")||ps_text_equal(r->value,"off")?(v?"off":"on"):(v?"false":"true"));
    }else if(r->kind==Level&&(key=='+'||key=='-')){
        uint32_t n;if(!decimal(r->value,n)||n<r->minimum||n>r->maximum||!r->step)return false;
        const uint32_t step=r->step;
        if(key=='+')n=r->maximum-n<step?r->maximum:n+step;
        else n=n-r->minimum<step?r->minimum:n-step;
        numberText(a.value,n);
    }else if(r->kind==Choice&&(key==' '||key=='+'||key=='-')){
        unsigned at=0;while(r->choices&&at<r->choiceCount&&!ps_text_equal(r->value,r->choices[at]))++at;
        if(!r->choices||at==r->choiceCount)return false;
        at=key=='-'?(at?at-1:r->choiceCount-1):(at+1)%r->choiceCount;
        copy(a.value,sizeof(a.value),r->choices[at]);
    }else return false;
    if(ps_text_equal(a.value,r->value))return false;
    copy(a.key,sizeof(a.key),r->key);a.type=Action::Set;dirty_=true;return true;
}
bool startDialog(const Snapshot &s,Action &a) {
    if(!PS_DEVICE_VIEW_DIALOGS||editing_||grid_)return false;
    feed('\r',s,a);crPending_=false;
    if(editing_){dialog_=true;full_=dirty_=true;}return false;
}
bool choiceKey(uint8_t key,const Snapshot &s,Action &) {
    if(!validateChoice(s))return false;
    const Row *r=currentRow(s);
    if(key=='A')choiceIndex_=uint8_t((choiceIndex_+r->choiceCount-1)%r->choiceCount);
    else if(key=='B'||key=='\t'||key==' ')choiceIndex_=uint8_t((choiceIndex_+1)%r->choiceCount);
    else if(key=='H')choiceIndex_=0;
    else if(key=='F')choiceIndex_=uint8_t(r->choiceCount-1);
    else if(key>='1'&&key<='9'&&unsigned(key-'1')<r->choiceCount)choiceIndex_=uint8_t(key-'1');
    else return false;
    copy(draft_,sizeof draft_,r->choices[choiceIndex_]);cursor_=ps_text_length(draft_);
    draftChanged_=!ps_text_equal(draft_,r->value);dirty_=full_=true;return false;
}
struct ChoiceBounds { unsigned left,top,width,visible,start; };
ChoiceBounds choiceBounds(const Snapshot &s,const Row &r) const {
    const unsigned columns=s.columns>240?240:s.columns;
    unsigned width=20;
    char clean[241];
    for(unsigned i=0;i<r.choiceCount;++i){
        unsigned n=unsigned(terminalText(clean,sizeof clean,r.choiceLabels?r.choiceLabels[i]:r.choices[i]))+10;
        if(n>width)width=n;
    }
    if(width>columns-4)width=columns-4;
    const unsigned visible=r.choiceCount<s.lines-10?r.choiceCount:s.lines-10;
    const unsigned requested=choiceIndex_>=visible?choiceIndex_-visible+1:0;
    const unsigned lastStart=r.choiceCount-visible;
    return {(columns-width)/2+1,4,width,visible,requested<lastStart?requested:lastStart};
}
static const char *pageName(unsigned p) {
    static const char *const names[]={"Device","Network","Matrix","Pixels","Game","Terminal","Cloud","Notifications","Clock"};
    return p<PageCount?names[p]:"";
}
static bool pageAvailable(unsigned p,const Snapshot &s) {
    return (p!=Led||(PS_DEVICE_VIEW_DIAGNOSTICS&&s.canLeds))&&(p!=Pixel||pixels(s))&&
        (p!=Game||(PS_DEVICE_VIEW_DIAGNOSTICS&&s.canGame))&&(p!=TerminalPage||s.terminalSettings)&&
        (p!=Cloud||s.cloudSettings)&&(p!=Notifications||s.notificationSettings)&&(p!=Clock||s.clockSettings);
}

bool mouse(const ps_mouse_event &e,const Snapshot &s,Action &a){
    const unsigned b=e.button,x=e.column,y=e.row,button=b&3,drag=b&32;
    if(e.release){mouseDrag_=false;return false;}
    if(!ansi_||s.columns<40||s.lines<16||x>s.columns||y>s.lines||(b&~127U)||(b&28U))return false;
    if(b&64U){if((!editing_||choiceMenu_)&&!grid_&&button<2)for(unsigned i=0;i<3;++i)arrow(button?'B':'A',s,a);return false;}
    if(button!=0&&button!=2)return false;
    if(drag&&!mouseDrag_)return false;
    if(!drag)mouseDrag_=true;
    const unsigned width=s.columns>240?240:s.columns;
    const unsigned offset=PS_DEVICE_VIEW_VERTICAL_TABS&&tabs_==VerticalTabs&&width>=64?22:0;
    const unsigned w=width-1-offset-tableGutter(s,width,offset),shift=tabsInsideTable_?0:1,body=6+shift-(PS_DEVICE_VIEW_PLAIN_ROWS?2:0)-titleBorderShift(s);
    if(editing_){
        if(button==2&&!drag){clearDraft();dirty_=full_=true;return false;}
        if(choiceMenu_){
            const Row *r=currentRow(s);if(!r||!r->choices||!r->choiceCount)return false;
            const ChoiceBounds box=choiceBounds(s,*r);
            if(!drag&&x>box.left&&x<box.left+box.width-1&&y>box.top&&y<=box.top+box.visible){
                const unsigned index=box.start+y-box.top-1;
                if(index>=r->choiceCount)return false;
                // Validate the snapshot through the same keyboard path before selecting.
                choiceKey('H',s,a);if(!editing_)return false;
                choiceIndex_=uint8_t(index);copy(draft_,sizeof draft_,r->choices[index]);
                draftChanged_=!ps_text_equal(draft_,r->value);return edit('\r',s,a);
            }
            return false;
        }
        if(dialog_&&!drag){
            const unsigned dw=width<76?width-6:70,left=(width-dw)/2+1,top=s.lines/2-3;
            if(y==top+5&&x>=left+2&&x<left+19){
                dialogFocus_=x<left+9?ApplyFocus:CancelFocus;
                return edit('\r',s,a);
            }
        }
        if(y==editRow_&&x>=editColumn_&&x<editColumn_+editWidth_){
            dialogFocus_=DraftFocus;
            const size_t start=cursor_>=editWidth_?cursor_-editWidth_+1:0;
            cursor_=start+x-editColumn_;if(cursor_>ps_text_length(draft_))cursor_=ps_text_length(draft_);
            dirty_=true;
        }
        return false;
    }
    if(!drag){
        unsigned hit=PageCount;
        if(offset&&x<=20&&y>=3&&y<s.lines-3U){unsigned row=3;for(unsigned p=0;p<PageCount;++p)if(pageAvailable(p,s)){if(row++==y)hit=p;}}
        else if(!offset&&y==2+shift){
            unsigned total=0,left=3;
            for(unsigned p=0;p<PageCount;++p)if(pageAvailable(p,s))total+=unsigned(ps_text_length(pageName(p)))+3;
            for(unsigned p=0;p<PageCount;++p){
                if(!pageAvailable(p,s)||(total>w-4&&p!=page_))continue;
                unsigned n=unsigned(ps_text_length(pageName(p)))+3;
                if(x>=left&&x<left+n)hit=p;
                left+=n;
            }
        }
        if(hit<PageCount){page_=Page(hit);selected_=0;sectionFocus_=grid_=false;dirty_=full_=true;return false;}
    }
    if(grid_)return false;
    if(x<=offset+1||y<body||y>=body+mouseVisible_)return false;
    uint16_t items[PS_DEVICE_VIEW_MAX_ROWS*4];unsigned focus;
    const unsigned total=sectionItems(s,items,focus,true);
    if(x>=offset+w&&x<width-1)return false;
    unsigned line=mouseStart_+y-body;
    if(x==width-1&&total>mouseVisible_){
        line=(y-body)*(total-1)/(mouseVisible_>1?mouseVisible_-1:1);
        while(line<total&&(items[line]&0x6000))++line;
    }
    if(line>=total||(items[line]&0x6000))return false;
    selected_=uint8_t(items[line]&0x1fff);sectionFocus_=!!(items[line]&0x8000);dirty_=true;
    if(x>=offset+w)return false;
    if(sectionFocus_){if(!drag)toggleSection(s,0);return false;}
    const Row *r=currentRow(s);if(!r)return false;
    if(button==2&&!drag)return startDialog(s,a);
    const unsigned labelWidth=w<59?12:w<95?24:28;
    const unsigned valueX=offset+labelWidth+6+(groups_==CategoryColumn&&w>=95?19:0);
    if(x<valueX)return false;
    if(!r->editable||r->pending||r->secret||!keySafe(r->key))return false;
    if(PS_DEVICE_VIEW_WIDGETS&&r->kind==Level&&r->maximum>r->minimum&&r->step){
        uint32_t observed;if(!decimal(r->value,observed)||observed<r->minimum||observed>r->maximum)return false;
        const unsigned span=offset+w>valueX+2?offset+w-valueX-2:1;
        const unsigned position=x-valueX>span?span:x-valueX;
        const uint32_t range=r->maximum-r->minimum;
        uint32_t n=r->minimum+(range/span)*position+(range%span)*position/span;
        n=r->minimum+((n-r->minimum)/r->step)*r->step;
        if(position==span)n=r->maximum;
        if(n==observed)return false;
        numberText(a.value,n);copy(a.key,sizeof a.key,r->key);a.type=Action::Set;return true;
    }
    if(!drag){if(PS_DEVICE_VIEW_WIDGETS&&(r->kind==Toggle||r->kind==Choice))return adjust(' ',s,a);return feed('\r',s,a);}
    return false;
}
template<class W> void base(Writer<W> &out) const {if(ansi_)out.raw(ps_style_base(theme_,trueColor_));}
template<class W> void ink(Writer<W> &out,unsigned role) const {if(ansi_)out.raw(ps_style_ink(theme_,trueColor_,theme_!=Default,role));}
template<class W> void edge(Writer<W> &out,const Snapshot &s,const char *unicode,const char *ascii) const {
    base(out);ink(out,7);out.raw(s.unicode?unicode:ascii);base(out);
}
template<class W> void rule(Writer<W> &out,const Snapshot &s,unsigned width,const char *left,const char *right,bool newline=true) const {
    base(out);ink(out,7);out.raw(s.unicode?left:"+");
    for(unsigned i=0;i<width-2;++i)out.raw(s.unicode?"\xe2\x94\x80":"-");
    out.raw(s.unicode?right:"+");base(out);if(newline)out.raw("\r\n");
}
template<class W> void gridRule(Writer<W> &out,const Snapshot &s,unsigned w,unsigned group,unsigned label,const char *junction,const char *left,const char *right) const {
    base(out);ink(out,7);out.raw(s.unicode?left:"+");
    for(unsigned i=1;i<w-1;++i){const bool join=!PS_DEVICE_VIEW_PLAIN_ROWS&&(i==label+3+(group?group+3:0)||(group&&i==group+3));
        out.raw(s.unicode?(join?junction:"\xe2\x94\x80"):(join?"+":"-"));}
    out.raw(s.unicode?right:"+");base(out);out.raw("\r\n");
}
template<class W> void value(Writer<W> &out,const Snapshot &s,const Row &r,unsigned width) const {
    if(r.secret){ink(out,0);out.field("[hidden]",width);return;}
    if(r.pending){ink(out,2);out.field("Pending readback",width);return;}
    if(PS_DEVICE_VIEW_WIDGETS&&r.kind==Toggle){
        const int on=toggle(r.value);ink(out,on<0?0:on?5:theme_==Default?PS_INK_INACTIVE:3);
        if(on<0){out.field("Unknown",width);return;}
        out.raw(s.unicode?(on?"\xe2\x97\x8f ":"\xe2\x97\x8b "):(on?"+ ":"- "));
        out.field(on?r.onLabel:r.offLabel,width-2);return;
    }
    if(PS_DEVICE_VIEW_WIDGETS&&r.kind==Level&&r.meter&&r.meterStyle==PS_METER_CHEVRONS&&width>=6){
        char text[244];ps_meter_value_field(text,sizeof text,width,r.value,s.unicode);
        ink(out,PS_INK_NUMBER);out.raw(text);return;
    }
    uint32_t n;
    if(PS_DEVICE_VIEW_WIDGETS&&r.kind==Level&&r.meter&&width>=22&&decimal(r.value,n)&&r.maximum>r.minimum&&n>=r.minimum&&n<=r.maximum){
        // At most ten cells, no multiplication overflow for uint32 ranges.
        const unsigned bar=10;const uint32_t span=r.maximum-r.minimum,offset=n-r.minimum;
        const unsigned filled=ps_meter_fraction(offset,span,bar);
        for(unsigned i=0;i<bar;++i){ink(out,i<filled?PS_INK_NUMBER:0);out.raw(ps_meter_track_cell(r.meterStyle,i<filled,s.unicode,i));}
        ink(out,PS_INK_NUMBER);out.raw(" ");char text[40];
        const unsigned percent=levelPercent(n-r.minimum,r.maximum-r.minimum);
        ps_terminal_format(text,sizeof(text),"%u%% (%u/%u)",percent,n,r.maximum);
        out.field(text,width-bar-1);return;
    }
    const char *shown=r.value;
    if(r.kind==Choice&&r.choices&&r.choiceLabels)for(unsigned i=0;i<r.choiceCount;++i)if(ps_text_equal(r.value,r.choices[i])){shown=r.choiceLabels[i];break;}
    ink(out,theme_==Default?(r.kind==Level?PS_INK_NUMBER:r.kind==Choice?PS_INK_COMMAND:r.editable?PS_INK_STRING:PS_INK_TEXT):(r.editable?1:4));
    if(r.unit&&*r.unit){char text[241];ps_terminal_format(text,sizeof(text),"%s %s",shown,r.unit);out.field(text,width);}
    else out.field(shown,width);
}

// Exactly one context owns hints. Editing suppresses page/grid navigation.
template<class W> void hint(Writer<W> &out,const char *text) const {
    bool first=true;
    while(*text){
        char token[96];unsigned n=0;
        const bool space=(unsigned char)*text<=32;
        while(text[n]&&((unsigned char)text[n]<=32)==space&&n+1<sizeof token){token[n]=text[n];++n;}
        token[n]=0;
        const bool styled=ansi_&&keycaps_&&!space&&ps_style_key_token(token,n);
        if(styled)out.raw(!PS_STYLE_THEMES?"\x1b[1;7m":first&&footerEmphasis_?PS_KEYCAP_PROMINENT_STYLE:PS_KEYCAP_STYLE);
        out.raw(token);
        if(styled){out.raw("\x1b[0m");base(out);ink(out,0);first=false;}
        if(space)for(unsigned i=0;i<n;++i)if(token[i]=='\n')first=true;
        text+=n;
    }
}
template<class W> void footer(Writer<W> &out,const Snapshot &s,unsigned w) const {
    if(ansi_){out.indent(0);out.position(s.lines-1);out.raw("\x1b[J");}
    base(out);ink(out,0);
    char line[384]={0},aligned[512];
    if(editing_&&dialog_&&ansi_)return; // The modal owns its only Apply/Cancel hint.
    if(editing_){ps_style_footer_append(line,sizeof line,s.unicode?"\xe2\x86\x90/\xe2\x86\x92 cursor  Enter apply  Esc cancel":w<60?"Left/Right move Enter apply Esc cancel":"Left/Right cursor Enter apply Esc cancel");ps_style_footer_append(line,sizeof line,"\r\n");ps_style_footer_format(aligned,sizeof aligned,line,w,footerGrid_);hint(out,aligned);return;}
    const Row *r=currentRow(s);
    if(grid_&&page_==Led){
        ps_style_footer_append(line,sizeof line,s.unicode?"\xe2\x86\x91\xe2\x86\x93\xe2\x86\x90\xe2\x86\x92 move":"Up/Down Left/Right");
        ps_style_footer_append(line,sizeof line,w>=60?"  W idle  R lost  O off":"  W/R/O state");
    }else if(grid_&&page_==Pixel){ps_style_footer_append(line,sizeof line,s.unicode?"\xe2\x86\x90/\xe2\x86\x92 test  G group  W/R/O color":"Left/Right test G group W/R/O color");}
    else if(page_==Game&&grid_){ps_style_footer_append(line,sizeof line,s.gameReady?"1-9 press  R reset":"AVR unavailable");}
    else if(sectionFocus_&&!sectionsCollapsible_){ps_style_footer_append(line,sizeof line,s.unicode?"↑/↓ select":"Up/Down select");}
    else if(sectionFocus_){ps_style_footer_append(line,sizeof line,w<60?"Enter/Space toggle":s.unicode?"Enter/Space toggle  \xe2\x86\x90 collapse  \xe2\x86\x92 expand":"Enter/Space toggle  Left close Right open");}
    else {ps_style_footer_append(line,sizeof line,s.unicode?"\xe2\x86\x91/\xe2\x86\x93 select":"Up/Down select");
        if(r&&r->editable&&!r->pending){
            ps_style_footer_append(line,sizeof line,PS_DEVICE_VIEW_WIDGETS&&r->kind==Toggle?"  Space toggle":PS_DEVICE_VIEW_WIDGETS&&r->kind==Choice?(s.unicode?"  \xe2\x86\x90/\xe2\x86\x92 choose":"  +/- choose"):PS_DEVICE_VIEW_WIDGETS&&r->kind==Level?(s.unicode?"  \xe2\x86\x90/\xe2\x86\x92 adjust":"  +/- adjust"):"  Enter edit");
            if(w>=70&&r->kind==Level)ps_style_footer_append(line,sizeof line,"  Enter edit");
            if(w>=90&&PS_DEVICE_VIEW_DIALOGS)ps_style_footer_append(line,sizeof line,"  F2 dialog");
        }
    }
    ps_style_footer_append(line,sizeof line,"\r\n");
    if(page_==Led||page_==Pixel||page_==Game)ps_style_footer_append(line,sizeof line,grid_?"F settings  ":page_==Led?"F matrix  ":page_==Game?"F game  ":"F pixels  ");
    ps_style_footer_append(line,sizeof line,w>=70?"Tab pages  Shift+Tab previous  Esc close":"Tab pages  Esc close");
    ps_style_footer_format(aligned,sizeof aligned,line,w,footerGrid_);hint(out,aligned);
    if(ansi_)out.raw("\x1b[K");else out.raw("\r\n");
}

// Blank viewport cells are painted with cursor movement, not a frame buffer.
template<class W> void blankRows(Writer<W> &out,const Snapshot &s,unsigned first,unsigned last,
                                 unsigned w,unsigned group=0,unsigned label=0) const {
    if(!ansi_)return;
    base(out);ink(out,7);
    for(unsigned row=first;row<=last;++row){
        out.position(row);out.raw("\x1b[K");
        out.raw(s.unicode?"\xe2\x94\x82":"|");
        if(group&&!PS_DEVICE_VIEW_PLAIN_ROWS){out.position(row,group+4);out.raw(s.unicode?"\xe2\x94\x82":"|");}
        if(label&&!PS_DEVICE_VIEW_PLAIN_ROWS){out.position(row,label+4+(group?group+3:0));out.raw(s.unicode?"\xe2\x94\x82":"|");}
        out.position(row,w);out.raw(s.unicode?"\xe2\x94\x82":"|");
    }
    base(out);
}

template<class W> void sidebar(Writer<W> &out,const Snapshot &s,unsigned height) const {
    out.position(1);rule(out,s,20,"\xe2\x95\xad","\xe2\x95\xae",false);
    unsigned page=0;
    for(unsigned row=2;row<height;++row){
        if(row>2&&page==PageCount){
            ink(out,7);out.position(row);out.raw(s.unicode?"\xe2\x94\x82":"|");
            out.field("",18);
            out.position(row,20);out.raw(s.unicode?"\xe2\x94\x82":"|");continue;
        }
        out.position(row);edge(out,s,"\xe2\x94\x82 ","| ");
        if(row==2){ink(out,0);out.field("Pages",16);}
        else {
            while(page<PageCount&&!pageAvailable(page,s))++page;
            if(page<PageCount){
                const bool active=page==page_;
                if(active&&ansi_)out.raw(ps_style_surface(theme_,trueColor_,1));else ink(out,0);
                out.raw(active&&(tabBrackets_||!ansi_)?"[":" ");out.raw(pageName(page));out.raw(active&&(tabBrackets_||!ansi_)?"]":" ");base(out);
                out.field("",14-unsigned(ps_text_length(pageName(page))));++page;
            }else out.field("",16);
        }
        edge(out,s," \xe2\x94\x82"," |");
    }
    out.position(height);rule(out,s,20,"\xe2\x95\xb0","\xe2\x95\xaf",false);
}

static const char *cellName(const Snapshot &s,unsigned state){
    static const char *const names[]={"Idle","Lost","Off","Won"};
    return state<4?(s.cellStateNames?s.cellStateNames[state]:names[state]):"Unknown";
}
static uint32_t cellRgb(const Snapshot &s,unsigned id){
    static const uint32_t colors[]={0xffffff,0xff0000,0,0x00ff00};
    const unsigned state=s.cellStates?s.cellStates[id-1]:255;
    return s.cellColors?s.cellColors[id-1]:state<4?colors[state]:0;
}
template<class W> void matrix(Writer<W> &out,const Snapshot &s,unsigned w,unsigned shift){
    const unsigned slots=s.cellStaggered?20:10,tile=w>=slots*4+6?4:3;
    const unsigned visibleX=(w-6)/tile<slots?(w-6)/tile:slots;
    const unsigned n=cell_-1,selectedY=n/10,selectedX=s.cellStaggered?2*(n%10)+(n/10)%2:n%10;
    const unsigned startX=selectedX>=visibleX?selectedX-visibleX+1:0;
    const unsigned visibleY=s.lines>=18+shift?8:s.lines-10-shift,startY=selectedY>=visibleY?selectedY-visibleY+1:0;
    rule(out,s,w,shift?"╭":"├",shift?"╮":"┤");
    edge(out,s,"\xe2\x94\x82 ","| ");ink(out,4);
    out.raw(s.cellStaggered?(s.unicode?"20 \xc3\x97 4 staggered":"20 x 4 staggered"):(s.unicode?"10 \xc3\x97 8 cells":"10 x 8 cells"));
    const unsigned heading=s.cellStaggered?16:12;
    out.field("",w-4-heading);edge(out,s," \xe2\x94\x82"," |");out.raw("\r\n");
    for(unsigned y=startY;y<startY+visibleY;++y){
        edge(out,s,"\xe2\x94\x82 ","| ");ink(out,0);char label[3]={char('A'+y),' ',0};out.raw(label);
        for(unsigned x=startX;x<startX+visibleX;++x){
            if(s.cellStaggered&&x%2!=y%2){out.field("",tile);continue;}
            const unsigned id=y*10+(s.cellStaggered?x/2:x)+1;
            const unsigned state=s.cellStates?s.cellStates[id-1]:255;
            const uint32_t rgb=cellRgb(s,id);const bool picked=id==cell_;
            const unsigned bright=((rgb>>16)&255)*299+((rgb>>8)&255)*587+(rgb&255)*114;
            if(ansi_){
                if(state>=4){ink(out,0);}
                else if(PS_STYLE_THEMES&&trueColor_){char style[80];
                    ps_terminal_format(style,sizeof(style),"\x1b[48;2;%u;%u;%um\x1b[38;2;%u;%u;%um",(rgb>>16)&255,(rgb>>8)&255,rgb&255,bright>128000?20:255,bright>128000?20:255,bright>128000?20:255);out.raw(style);}
                else if(PS_STYLE_THEMES){const unsigned index=16+36*(((rgb>>16)&255)*5/255)+6*(((rgb>>8)&255)*5/255)+(rgb&255)*5/255;
                    char style[40];ps_terminal_format(style,sizeof(style),"\x1b[48;5;%um\x1b[%um",index,bright>128000?30:97);out.raw(style);}
                if(picked)out.raw("\x1b[1;4m");
            }
            char number[8];ps_terminal_format(number,sizeof(number),"%02u",id);
            static const char *const plainState[]={"I","L","O","W"};
            out.raw(picked?(s.unicode?"\xe2\x96\xb8":">"):state>=4?"?":!ansi_?plainState[state]:" ");out.raw(number);
            if(tile==4)out.raw(" ");
            base(out);
        }
        out.field("",w-6-visibleX*tile);edge(out,s," \xe2\x94\x82"," |");out.raw("\r\n");
    }
    if(ansi_){blankRows(out,s,5+shift+visibleY,s.lines-4,w);out.position(s.lines-3);}
    rule(out,s,w,"\xe2\x95\xb0","\xe2\x95\xaf");
    if(ansi_){out.indent(0);out.position(s.lines-2);}
    const unsigned state=s.cellStates?s.cellStates[cell_-1]:255;char status[128];
    ps_terminal_format(status,sizeof(status),"Cell %02u  %s",cell_,cellName(s,state));ink(out,4);out.safe(status,w>=60?24:w);
    if(w>=60){const uint32_t rgb=cellRgb(s,cell_);char hex[8]={'#',0};const char *digits="0123456789ABCDEF";
        for(unsigned i=0;i<6;++i)hex[i+1]=digits[(rgb>>(20-4*i))&15];
        if(state<4){out.raw("  ");out.raw(hex);}ink(out,0);out.raw("  ");
        out.safe(*notice_?notice_:"Configured state colors",w-35);}
    else if(*notice_){const unsigned statusLength=unsigned(ps_text_length(status));if(statusLength+2<w){out.raw("  ");out.safe(notice_,w-statusLength-2);}}
    out.raw("\r\n");footer(out,s,s.columns-1);
}
template<class Write> bool renderTable(const Snapshot &s,Write write) {
    Writer<Write> out(write,ansi_);
    const unsigned width=s.columns>240?240:s.columns;
    bool paintAll=full_||!ansi_||!PS_DEVICE_VIEW_RETAIN_ROWS;
    if(ansi_){base(out);out.raw(full_?"\x1b[H\x1b[2J":"\x1b[H");}
    full_=false;
    if(width<40||s.lines<16){out.raw(s.unicode?"Resize terminal to 40 \xc3\x97 16\r\n":"Resize terminal to 40 x 16\r\n");return true;}
    // Leave the physical terminal's last column free to avoid delayed autowrap.
    const unsigned offset=PS_DEVICE_VIEW_VERTICAL_TABS&&tabs_==VerticalTabs&&ansi_&&width>=64?22:0;
    const unsigned gutter=tableGutter(s,width,offset);
    if(gutter!=paintedGutter_){paintAll=true;paintedGutter_=gutter;if(ansi_)out.raw("\x1b[H\x1b[2J");}
    const unsigned w=width-1-offset-gutter;
    const unsigned shift=tabsInsideTable_?0:1,body=6+shift-(PS_DEVICE_VIEW_PLAIN_ROWS?2:0)-titleBorderShift(s);
    if(offset){sidebar(out,s,s.lines-3);out.indent(offset);out.position(1);}
    base(out);ink(out,7);if(!shift)out.raw(s.unicode?"\xe2\x95\xad\xe2\x94\x80 ":"+- ");
    ink(out,4);char title[241];const unsigned titleSize=unsigned(terminalText(title,w-8,s.title));out.raw(title);
    if(!shift){ink(out,7);out.raw(" ");for(unsigned i=titleSize+4;i<w-1;++i)out.raw(s.unicode?"\xe2\x94\x80":"-");out.raw(s.unicode?"\xe2\x95\xae":"+");}
    else out.field("",w-titleSize);
    base(out);out.raw("\r\n");if(shift){out.field("",w);out.raw("\r\n");out.raw("  ");}
    else edge(out,s,"\xe2\x94\x82 ","| ");
    unsigned used=0,tabTotal=0;for(unsigned i=0;i<PageCount;++i)if(pageAvailable(i,s))tabTotal+=unsigned(ps_text_length(pageName(i)))+3;
    if(offset){ink(out,4);out.raw(pageName(page_));used=unsigned(ps_text_length(pageName(page_)));}
    for(unsigned i=0;i<PageCount;++i){
        if(offset||!pageAvailable(i,s)||(tabTotal>w-4&&i!=page_))continue;
        if(i==page_&&ansi_)out.raw(ps_style_surface(theme_,trueColor_,1));else ink(out,0);
        out.raw(i==page_&&(tabBrackets_||!ansi_)?"[":" ");out.raw(pageName(i));out.raw(i==page_&&(tabBrackets_||!ansi_)?"]":" ");base(out);
        out.raw(" "); // Inter-tab spacing is outside the selected surface.
        used+=unsigned(ps_text_length(pageName(i)))+3;
    }
    if(!offset&&tabTotal>w-4){char hint[40];ps_terminal_format(hint,sizeof(hint),"  %u/%u",unsigned(page_)+1,unsigned(PageCount));
        const unsigned n=unsigned(ps_text_length(hint));if(used+n<w-4){ink(out,0);out.raw(hint);used+=n;}}
    out.field("",w-4-used);if(shift)out.raw("  ");else edge(out,s," \xe2\x94\x82"," |");out.raw("\r\n");
    if(PS_DEVICE_VIEW_MATRIX&&page_==Led&&grid_&&s.canLeds){matrix(out,s,w,shift);return true;}
    const bool category=!PS_DEVICE_VIEW_PLAIN_ROWS&&groups_==CategoryColumn&&w>=95;
    const unsigned groupWidth=category?16:0,labelWidth=w<59?12:w<95?24:28;
    const unsigned valueWidth=w-7-labelWidth-(category?groupWidth+3:0);
    gridRule(out,s,w,groupWidth,labelWidth,"┬",shift?"╭":"├",shift?"╮":"┤");
    auto cell=[&](const char *text,unsigned n){ink(out,theme_==Default?PS_INK_HEADING:0);out.field(text,n);edge(out,s," \xe2\x94\x82 "," | ");};
    if(!PS_DEVICE_VIEW_PLAIN_ROWS){
    edge(out,s,"\xe2\x94\x82 ","| ");if(category)cell("Category",groupWidth);cell("Setting",labelWidth);
    ink(out,theme_==Default?PS_INK_HEADING:0);out.field("Value",valueWidth);edge(out,s," \xe2\x94\x82"," |");out.raw("\r\n");
    }
    const unsigned count=rowCount(s);if(selected_>=count)selected_=count?uint8_t(count-1):0;
    const unsigned diagnostics=page_==Led&&!PS_DEVICE_VIEW_MATRIX?(s.lines<24?5:9):page_==Game?3:page_==Pixel?2:0;
    unsigned visible=tableCapacity(s,w);
    // Separate logical rows from section headings without heap storage.
    uint16_t logical[PS_DEVICE_VIEW_MAX_ROWS*4];unsigned selectedLine;
    const unsigned total=sectionItems(s,logical,selectedLine,true);
    if(visible>(total?total:1))visible=total?total:1;
    if(paintedRows_!=visible){paintAll=true;paintedRows_=visible;}
    const unsigned start=selectedLine>=visible?selectedLine-visible+1:0;
    mouseStart_=start;mouseVisible_=visible;
    if(!PS_DEVICE_VIEW_PLAIN_ROWS&&!titleBorderShift(s))gridRule(out,s,w,groupWidth,labelWidth,total&&(logical[start]&0xe000)?"\xe2\x94\xb4":"\xe2\x94\xbc","\xe2\x94\x9c","\xe2\x94\xa4");
    for(unsigned i=0;i<visible;++i){
        const unsigned line=start+i,at=line<total?(logical[line]&0x1fff):count;
        const bool group=line<total&&(logical[line]&0x8000);
        const unsigned border=line<total?(logical[line]&0x6000):0;
        const Row *r=at<count?s.rows+at:nullptr;const bool selected=r&&at==selected_&&!grid_&&(group==sectionFocus_);
        Snapshot single;single.rows=r;single.rowCount=r?1:0;
        const uint32_t stamp=hash(single)^uint32_t(line<total?logical[line]:0)*16777619U^(selected?0x40000000U:0)^(group&&sectionClosed(at)?0x20000000U:0);
        const bool selectionChanged=(selected_!=paintedSelection_)&&(at==selected_||at==paintedSelection_);
        if(PS_DEVICE_VIEW_RETAIN_ROWS&&!paintAll&&painted_[i]==stamp&&!selectionChanged&&paintedGrid_==grid_)continue;
        if(PS_DEVICE_VIEW_RETAIN_ROWS)painted_[i]=stamp;
        out.position(body+i);
        if(border){
            const bool joined=border==0x4000 ? i&&!(logical[line-1]&0xe000)
                : (line+1>=total || !(logical[line+1]&0xe000)) && body+i<s.lines-4-diagnostics;
            gridRule(out,s,w,groupWidth,labelWidth,!joined?"\xe2\x94\x80":border==0x4000?"\xe2\x94\xb4":"\xe2\x94\xac","\xe2\x94\x9c","\xe2\x94\xa4");continue;
        }
        if(group){
            char label[241];terminalText(label,sizeof label,r->category);
            for(unsigned j=0;label[j];++j)if(label[j]>='a'&&label[j]<='z')label[j]=char(label[j]-'a'+'A');
            const char *marker=ps_table_section_marker(s.unicode,sectionsCollapsible_,sectionIndicators_,sectionClosed(at),selected,sectionIndicatorsAlways_);
            if(sectionBorderTitles_){
                char clipped[241];const unsigned n=unsigned(terminalText(clipped,PS_DEVICE_VIEW_PLAIN_ROWS?w-8:labelWidth-2,label));
                const unsigned markerWidth=*marker?2:0;
                edge(out,s,"├─ ","+- ");
                if(selected&&ansi_)out.raw(ps_style_surface(theme_,trueColor_,1));else ink(out,PS_INK_HEADING);
                out.raw(marker);out.raw(clipped);out.raw(" ");base(out);ink(out,7);
                const bool above=!i||!(logical[line-1]&0xe000);
                const bool below=line+1>=total||!(logical[line+1]&0xe000);
                for(unsigned col=4+markerWidth+n;col<w-1;++col){
                    const bool join=!PS_DEVICE_VIEW_PLAIN_ROWS&&col==labelWidth+3;
                    out.raw(join?ps_table_junction(s.unicode,above,below):s.unicode?"─":"-");
                }
                edge(out,s,"┤","+");out.raw("\r\n");continue;
            }
            edge(out,s,"\xe2\x94\x82","|");
            if(ansi_)out.raw(PS_STYLE_THEMES?ps_style_surface(theme_,trueColor_,selected?1:0):selected?"\x1b[7m":"\x1b[1m");
            out.raw(" ");out.raw(marker);
            out.field(label,w-4-unsigned(ps_text_length(marker)?2:0));out.raw(" ");edge(out,s,"\xe2\x94\x82","|");out.raw("\r\n");continue;
        }
        edge(out,s,"\xe2\x94\x82 ","| ");
        if(category)cell(r&&(i==0||!ps_text_equal(r->category,s.rows[at-1].category))?r->category:"",groupWidth);
        if(selected&&ansi_)out.raw(ps_style_surface(theme_,trueColor_,2));
        ink(out,selected&&theme_!=Default?1:4);
        bool categories=false;
        if(sections())for(unsigned j=0;j<count;++j)if(s.rows[j].category&&*s.rows[j].category){categories=true;break;}
        const char *mark=ps_table_marker(selectionMarker_,s.unicode,categories,sectionsCollapsible_,s.unicode&&sectionIndicators_==0);
        char markerText[241];const unsigned marker=selectionMarker_?unsigned(terminalText(markerText,sizeof markerText,mark)):(*mark?2:0);
        const unsigned markerWidth=marker<labelWidth?marker:labelWidth;
        if(!selectionMarker_&&selected&&markerWidth==2)out.raw(mark);
        else out.field(selected&&selectionMarker_?markerText:"",markerWidth);
        out.field(r?r->label:"",labelWidth-markerWidth);if(PS_DEVICE_VIEW_PLAIN_ROWS)out.raw("   ");else edge(out,s," \xe2\x94\x82 "," | ");
        if(PS_DEVICE_VIEW_INLINE_EDIT&&selected&&editing_&&(!dialog_||!ansi_)){editValue(out,valueWidth);editRow_=body+i;editColumn_=offset+labelWidth+6+(category?groupWidth+3:0);editWidth_=valueWidth;}
        else if(r)value(out,s,*r,valueWidth);else out.field("",valueWidth);
        edge(out,s," \xe2\x94\x82"," |");out.raw("\r\n");
    }
    paintedSelection_=selected_;paintedGrid_=grid_;
    if(ansi_){
        if(paintAll)blankRows(out,s,body+visible,s.lines-4-diagnostics,w,groupWidth,labelWidth);
        if(gutter){base(out);for(unsigned i=0;i<s.lines-9-shift-diagnostics+(PS_DEVICE_VIEW_PLAIN_ROWS?2:0)+titleBorderShift(s);++i){out.position(body+i,w+1);out.field("",gutter);}}
        if(total>visible){
            const unsigned height=s.lines-9-shift-diagnostics+(PS_DEVICE_VIEW_PLAIN_ROWS?2:0)+titleBorderShift(s);
            const unsigned thumb=height*visible/total?height*visible/total:1;
            const unsigned top=start*(height-thumb)/(total-visible);
            base(out);int previous=-1;
            for(unsigned i=0;i<height;++i){
                const bool active=i>=top&&i<top+thumb;
                const bool border=i<visible&&((logical[start+i]&0x6000)||(sectionBorderTitles_&&(logical[start+i]&0x8000)));
                out.position(body+i,w+gutter);
                if(previous!=int(active)){ink(out,active?1:7);previous=int(active);}
                out.raw(s.unicode?(!gutter&&border?(active?"┫":"┤"):(active?"┃":"│")):(active?"#":!gutter&&border?"+":"|"));
            }
        }
        out.position(s.lines-3-diagnostics);
    }
    const bool bottomJoin=body+visible<s.lines-3-diagnostics || !total || !(logical[start+visible-1]&0xe000);
    gridRule(out,s,w,groupWidth,labelWidth,bottomJoin?"\xe2\x94\xb4":"\xe2\x94\x80","\xe2\x95\xb0","\xe2\x95\xaf");
    if(pageAvailable(page_,s)&&!(PS_DEVICE_VIEW_MATRIX&&page_==Led))renderDiagnostics(out,s,w);
    if(ansi_){out.indent(0);out.position(s.lines-2);}
    base(out);ink(out,0);
    const Row *r=currentRow(s);
    char status[64];
    if(sectionFocus_)ps_terminal_format(status,sizeof(status),"Section %s",sectionClosed(selected_)?"collapsed":"expanded");
    else ps_terminal_format(status,sizeof(status),"%u / %u",count?selected_+1:0,count);
    ink(out,theme_==Default?PS_INK_NUMBER:1);out.raw(status);out.raw("  ");ink(out,0);
    const char *context=*notice_?notice_:r&&r->pending?"Waiting for readback":r&&!r->editable?"Read only":"";
    out.safe(context,width-1-unsigned(ps_text_length(status))-2);out.raw("\r\n");footer(out,s,width-1);
    if(PS_DEVICE_VIEW_CHOICE_MENU&&choiceMenu_&&editing_&&ansi_){
        const Row *choice=currentRow(s);
        if(choice&&choice->choices&&choice->choiceCount){
            const ChoiceBounds box=choiceBounds(s,*choice);
            out.indent(0);out.raw("\x1b[H\x1b[2J");base(out);
            out.position(1,box.left);ink(out,PS_INK_HEADING);out.safe(editLabel_,box.width);
            auto line=[&](unsigned y,const char *left,const char *right){
                out.position(y,box.left);ink(out,PS_INK_ERROR);out.raw(s.unicode?left:"+");
                for(unsigned i=1;i<box.width-1;++i)out.raw(i==5?(s.unicode?(y==box.top?"┬":"┴"):"+"):(s.unicode?"─":"-"));
                out.raw(s.unicode?right:"+");base(out);
            };
            line(box.top,"╭","╮");
            for(unsigned i=0;i<box.visible;++i){
                const unsigned index=box.start+i;char number[12];numberText(number,index+1);
                out.position(box.top+i+1,box.left);ink(out,PS_INK_ERROR);out.raw(s.unicode?"│":"|");base(out);
                const bool selected=index==choiceIndex_;
                if(selected)out.raw(trueColor_?"\x1b[48;2;229;192;123m\x1b[30m":"\x1b[43;30m");
                else ink(out,PS_INK_TEXT);
                out.raw(" ");out.field(number,3);base(out);ink(out,PS_INK_ERROR);out.raw(s.unicode?"│":"|");base(out);
                if(selected)out.raw(trueColor_?"\x1b[48;2;229;192;123m\x1b[30m":"\x1b[43;30m");else ink(out,PS_INK_TEXT);
                out.raw(" ");out.field(choice->choiceLabels?choice->choiceLabels[index]:choice->choices[index],box.width-8);
                base(out);ink(out,PS_INK_ERROR);out.raw(s.unicode?"│":"|");base(out);
            }
            line(box.top+box.visible+1,"╰","╯");
            out.position(s.lines-2,2);ink(out,PS_INK_MUTED);out.safe(notice_,s.columns-3);
            out.position(s.lines-1,2);hint(out,s.columns<64?(s.unicode?"↑/↓ select  Enter apply  Esc cancel":"Up/Down select Enter apply Esc cancel"):(s.unicode?"↑/↓ select  1-9 choose  Enter apply  Esc cancel":"Up/Down select  1-9 choose  Enter apply  Esc cancel"));
            out.raw("\x1b[?25l");full_=true;
        }
    }else if(PS_DEVICE_VIEW_DIALOGS&&dialog_&&editing_&&ansi_){
        const unsigned dw=width<76?width-6:70,left=(width-dw)/2+1,top=s.lines/2-3;
        auto position=[&](unsigned row){char b[24];ps_terminal_format(b,sizeof(b),"\x1b[%u;%uH",row,left);out.raw(b);};
        position(top);rule(out,s,dw,"\xe2\x95\xad","\xe2\x95\xae",false);
        position(top+1);edge(out,s,"\xe2\x94\x82 ","| ");ink(out,4);out.field(editLabel_,dw-4);edge(out,s," \xe2\x94\x82"," |");
        position(top+2);edge(out,s,"│ ","| ");ink(out,PS_INK_MUTED);out.raw("Observed  ");base(out);out.field(secret_?"********":r?r->value:"Unavailable",dw-14);edge(out,s," │"," |");
        position(top+3);edge(out,s,"│ ","| ");out.raw("Draft     ");editValue(out,dw-14);edge(out,s," │"," |");
        char limits[96]={0};if(r&&r->kind==Level)ps_terminal_format(limits,sizeof limits,"%u..%u  step %u",r->minimum,r->maximum,r->step);
        position(top+4);edge(out,s,"│ ","| ");ink(out,PS_INK_MUTED);out.field(*notice_?notice_:limits,dw-4);edge(out,s," │"," |");
        position(top+5);edge(out,s,"│ ","| ");
        if(PS_ACTION_BUTTONS)out.raw(ps_style_button(theme_,trueColor_,dialogFocus_==ApplyFocus?PS_BUTTON_FOCUSED:PS_BUTTON_IDLE));
        else if(dialogFocus_==ApplyFocus)out.raw(ps_style_surface(theme_,trueColor_,1));else ink(out,PS_INK_SUCCESS);
        out.raw(" Apply ");base(out);out.raw("  ");
        if(PS_ACTION_BUTTONS)out.raw(ps_style_button(theme_,trueColor_,dialogFocus_==CancelFocus?PS_BUTTON_FOCUSED:PS_BUTTON_IDLE));
        else if(dialogFocus_==CancelFocus)out.raw(ps_style_surface(theme_,trueColor_,1));
        out.raw(" Cancel ");base(out);out.field("",dw-21);edge(out,s," │"," |");
        position(top+6);rule(out,s,dw,"╰","╯",false);
        out.indent(0);out.position(s.lines-1,2);hint(out,"Tab focus  Enter apply  Esc cancel");
        editRow_=top+3;editColumn_=left+12;editWidth_=dw-14;
    }
    if(editing_&&!PS_DEVICE_VIEW_INLINE_EDIT&&!dialog_){
        char label[19];const unsigned n=unsigned(terminalText(label,sizeof label,editLabel_));
        out.position(s.lines);base(out);out.raw("Edit ");out.raw(label);out.raw(": ");
        editRow_=s.lines;editColumn_=8+n;editWidth_=width-editColumn_-1;
        editValue(out,editWidth_);
    }
    if(editing_&&ansi_)editLine(write);
    return true;
}

template<class W> void renderDiagnostics(Writer<W> &out,const Snapshot &snapshot,unsigned width) {
            if (page_ == Led) {
                const uint8_t startLine = snapshot.lines < 24 ? ((cell_ - 1) / 10 < 4 ? 0 : 4) : 0;
                const uint8_t endLine = snapshot.lines < 24 ? startLine + 4 : 8;
                for (uint8_t line = startLine; line < endLine; ++line) {
                    for (uint8_t col = 0; col < 10; ++col) {
                        const uint8_t id = line * 10 + col + 1;
                        const bool picked = id == cell_;
                        if (ansi_) {
                            const uint8_t state =
                                snapshot.cellStates ? snapshot.cellStates[id - 1] : 255;
                            if(PS_STYLE_THEMES)out.raw(state == 0 ? "\x1b[37m" : state == 1 ? "\x1b[31m" : "\x1b[90m");
                            if (picked && grid_)
                                out.raw("\x1b[7m");
                        }
                        char number[6];
                        ps_terminal_format(number, sizeof(number), width<50?(picked?">%02u":" %02u"):(picked ? "[%02u]" : " %02u "),
                                           id);
                        out.raw(number);
                        if (ansi_)
                            base(out);
                    }
                    out.raw("\r\n");
                }
                if (ansi_) {
                    ink(out,0);
                }
                if (ansi_) {
                    base(out);
                }
            } else if (page_ == Game) {
                out.raw("\r\n");
                for (uint8_t id = 1; id <= 9; ++id) {
                    const bool enabled = snapshot.enabledKeys & (1U << (id - 1));
                    if (ansi_)
                        out.raw(!enabled                ? "\x1b[90m"
                                : snapshot.winner == id ? "\x1b[1;35m"
                                : snapshot.winner       ? "\x1b[31m"
                                                        : "\x1b[37m");
                    char box[8];
                    ps_terminal_format(box, sizeof(box), "[%u]%s", id, id == 9 ? "\r\n" : " ");
                    out.raw(box);
                    if (ansi_)
                        base(out);
                }
                char winner[64];
                ps_terminal_format(winner, sizeof(winner), "Winner %u  %s\r\n", snapshot.winner,
                                   snapshot.gameReady ? "Ready" : "AVR unavailable");
                out.raw(winner);
            } else if (page_ == Pixel) {
                syncPixel(snapshot);
                const uint8_t count = group_ ? snapshot.groupCounts[groupAt(snapshot, pixel_)] : 1;
                char line[112];
                ps_terminal_format(line, sizeof(line),
                                   width < 70 ? "Pixels %u..%u / %u (%u)\r\n"
                                              : "Physical pixel %u..%u / %u (%u capacity)\r\n",
                                   pixel_ + 1, pixel_ + count, snapshot.usedPixels,
                                   snapshot.pixelCount);
                out.raw(line);
                out.raw(group_ ? "Group  " : "Single pixel  ");
                out.raw(color());
                out.raw("\r\n");
                if (ansi_)
                    ink(out,0);
                if (ansi_)
                    base(out);
            }
}
