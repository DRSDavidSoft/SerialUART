#include "portable_table_policy.h"
#ifndef PORTABLE_DEVICE_VIEW_HPP
#define PORTABLE_DEVICE_VIEW_HPP
#include "portable_shell_editor.hpp"
#include "portable_style.h"
#include "portable_meter.h"
/* MIT Copyright (c) 2026 AtomicDeploy contributors. Canonical snapshot/action
 * device view. Source lineage is in the donor inventory. Hardware calls and routing
 * stay with the caller. Include only in products requiring this optional view. */
#ifndef PS_DEVICE_VIEW_DIAGNOSTICS
#define PS_DEVICE_VIEW_DIAGNOSTICS 1
#endif
#ifndef PS_DEVICE_VIEW_INLINE_EDIT
#define PS_DEVICE_VIEW_INLINE_EDIT 1
#endif
#ifndef PS_DEVICE_VIEW_PLAIN_ROWS
#define PS_DEVICE_VIEW_PLAIN_ROWS 0
#endif
#ifndef PS_DEVICE_VIEW_TABLE
#define PS_DEVICE_VIEW_TABLE 1
#endif
/* Legacy TABLE=0 now selects uncached painting, never a second renderer.
 * Set RETAIN_ROWS explicitly in new integrations. Other features are independent. */
#ifndef PS_DEVICE_VIEW_RETAIN_ROWS
#define PS_DEVICE_VIEW_RETAIN_ROWS PS_DEVICE_VIEW_TABLE
#endif
#ifndef PS_DEVICE_VIEW_WIDGETS
#define PS_DEVICE_VIEW_WIDGETS 1
#endif
#ifndef PS_DEVICE_VIEW_DIALOGS
#define PS_DEVICE_VIEW_DIALOGS 1
#endif
#ifndef PS_DEVICE_VIEW_CHOICE_MENU
#define PS_DEVICE_VIEW_CHOICE_MENU PS_DEVICE_VIEW_DIALOGS
#endif
#ifndef PS_DEVICE_VIEW_MATRIX
#define PS_DEVICE_VIEW_MATRIX 1
#endif
#ifndef PS_DEVICE_VIEW_CATEGORY_COLUMN
#define PS_DEVICE_VIEW_CATEGORY_COLUMN 0
#endif
#ifndef PS_DEVICE_VIEW_SECTIONS
#define PS_DEVICE_VIEW_SECTIONS 1
#endif
#ifndef PS_DEVICE_VIEW_VERTICAL_TABS
#define PS_DEVICE_VIEW_VERTICAL_TABS 1
#endif
#ifndef PS_DEVICE_VIEW_MAX_ROWS
#define PS_DEVICE_VIEW_MAX_ROWS 64
#endif
#ifndef PS_DEVICE_VIEW_FRAME_BYTES
#define PS_DEVICE_VIEW_FRAME_BYTES 65535UL
#endif
#if PS_DEVICE_VIEW_FRAME_BYTES < 16384
#error Rich tables require a frame budget of at least 16384 bytes
#endif
#if PS_DEVICE_VIEW_MAX_ROWS < 1 || PS_DEVICE_VIEW_MAX_ROWS > 255
#error PS_DEVICE_VIEW_MAX_ROWS must be in 1..255
#endif
// Allocation-free device presentation with filtering, deterministic state,
// bounded rendering and sanitized text. The caller owns application behavior.
// Snapshots are observations; only the caller's common router may authorize
// and execute an explicit Action. Entering/rendering never produces an action.
namespace PortableShell {
class DeviceView {
  public:
    enum Theme : uint8_t { Violet=PS_THEME_VIOLET, Red=PS_THEME_RED, Cyan=PS_THEME_CYAN, OneDark=PS_THEME_ONE_DARK, Default=PS_THEME_DEFAULT };
    enum TabLayout : uint8_t { HorizontalTabs, VerticalTabs };
    enum GroupLayout : uint8_t { SectionRows, CategoryColumn };
    enum Kind : uint8_t { Text, Toggle, Level, Choice };
    enum Page : uint8_t {
        Device,
        Network,
        Led,
        Pixel,
        Game,
        TerminalPage,
        Cloud,
        Notifications,
        Clock,
        PageCount
    };
    struct Row {
        const char *label = nullptr;
        const char *key = nullptr;
        const char *value = nullptr;
        bool editable = false;
        bool secret = false;
        Kind kind = Text;
        const char *category = "";
        uint32_t minimum = 0, maximum = 65535, step = 1;
        const char *const *choices = nullptr;
        uint8_t choiceCount = 0;
        bool pending = false;
        bool meter = false; // Meaningful bounded levels/PWM, not ports or timestamps.
        unsigned meterStyle = PS_METER_LINE;
        const char *unit = "";
        const char *const *choiceLabels = nullptr;
        const char *onLabel = "On", *offLabel = "Off";
        constexpr Row(const char *label_ = nullptr, const char *key_ = nullptr,
                      const char *value_ = nullptr, bool editable_ = false, bool secret_ = false)
            : label(label_), key(key_), value(value_), editable(editable_), secret(secret_) {}
    };
    struct Snapshot {
        const char *title = "Device";
        const Row *rows = nullptr;               // Current page only, retained through feed/render.
        uint8_t rowCount = 0;                    // Bounded by PS_DEVICE_VIEW_MAX_ROWS.
        const uint8_t *cellStates = nullptr;     // Exactly 80: 0 idle, 1 lost, 2 off, 3 won; other values unknown.
        const uint32_t *cellColors = nullptr;    // Optional 80 configured state RGBs, never physical proof.
        const char *const *cellStateNames = nullptr; // Optional four labels for states 0..3.
        bool cellStaggered = false;              // Provider's paired, offset 20-column table geometry.
        uint16_t pixelCount = 0, usedPixels = 0; // Wire capacity and configured prefix.
        const uint8_t *groupCounts = nullptr;
        const uint16_t *groupStarts = nullptr; // Exactly groupCount prefix spans.
        uint8_t groupCount = 0;
        uint32_t groupLayoutRevision = 0;
        bool canLeds = false, canPixels = false; // Independently ACKed, default denied.
        bool canGame = false, gameReady = false, terminalSettings = false, unicode = false;
        bool cloudSettings = false, notificationSettings = false, clockSettings = false;
        uint16_t enabledKeys = 0, columns = 80, lines = 24;
        uint8_t winner = 0;
    };
    struct Action {
        enum Type : uint8_t { None, Set, Cell, Pixel, Refresh, Exit, Press, Reset } type = None;
        char key[32]{}, value[385]{};
        // Cell: logical1..80; Pixel: ZERO-based physical span from this snapshot.
        uint16_t index = 0;
        uint8_t count = 0;
        uint32_t groupLayoutRevision = 0;
        bool group = false;
        Action() = default;
        Action(const Action &) = default;
        Action &operator=(const Action &) = default;
        ~Action() {
            volatile char *p = value;
            for (size_t i = 0; i < sizeof(value); ++i)
                p[i] = 0;
        }
    };
    ~DeviceView() {
        clearDraft();
    } // No terminal output, SDK call, or allocation on teardown.
    void begin(bool ansi = true) {
        clearDraft();
        active_ = true;
        ansi_ = ansi;
        page_ = Device;
        selected_ = 0;
        sectionFocus_ = false;
        grid_ = false;
        dirty_ = full_ = true;
        lastHash_ = 0;
        escape_ = escapeBytes_ = attribute_ = 0;
        mouseDrag_=false;
        crPending_ = false;
        cell_ = 1;
        pixel_ = 0;
        group_ = false;
        pixelColor_ = 1;
        notice_[0] = 0;
    }
    bool active() const {
        return active_;
    }
    template<class Write> void end(Write write) {
        if(ansi_&&PS_MOUSE){const char *p="\x1b[?1002l\x1b[?1006l";write((const uint8_t*)p,ps_text_length(p));}
        active_=false;mouseDrag_=false;clearDraft();
    }
    bool editing() const {
        return editing_;
    }
    bool escapePending() const {
        return escape_ == 1;
    }
    Page page() const {
        return page_;
    }
    uint8_t selectedCell() const {
        return cell_;
    }
    uint16_t selectedPixel() const {
        return pixel_;
    }
    bool group() const {
        return group_;
    }
    const char *draft() const {
        return draft_;
    } // Secret draft is NEVER rendered.
    void invalidate() {
        if (!editing_)
            dirty_ = true;
    }
    void setAnsi(bool enabled) {
        if (ansi_ != enabled) {
            ansi_ = enabled;
            dirty_ = full_ = true;
        }
    }
    void setTheme(Theme theme) { if (theme <= Default && theme_ != theme) {theme_=theme;dirty_=full_=true;} }
    Theme theme() const { return theme_; }
    void setTabLayout(TabLayout layout) {
        if (layout <= VerticalTabs && tabs_ != layout) { tabs_=layout;dirty_=full_=true; }
    }
    TabLayout tabLayout() const { return tabs_; }
    void setGroupLayout(GroupLayout layout) {
        if (!editing_ && layout <= CategoryColumn && groups_ != layout) {
            groups_=layout;sectionFocus_=false;dirty_=full_=true;
        }
    }
    GroupLayout groupLayout() const { return groups_; }
    bool sectionSelected() const { return sectionFocus_; }
    void setSectionBorders(bool enabled) {
        if(!editing_ && sectionBorders_!=enabled){sectionBorders_=enabled;dirty_=full_=true;}
    }
    bool sectionBorders() const { return sectionBorders_; }
    void setSectionBorderTitles(bool enabled) {
        if(!editing_&&sectionBorderTitles_!=enabled){sectionBorderTitles_=enabled;dirty_=full_=true;}
    }
    bool sectionBorderTitles() const { return sectionBorderTitles_; }
    void setSectionBottomBorder(bool enabled) {
        if(!editing_ && sectionBottomBorder_!=enabled){sectionBottomBorder_=enabled;dirty_=full_=true;}
    }
    bool sectionBottomBorder() const { return sectionBottomBorder_; }
    void setTabsInsideTable(bool enabled) {
        if(!editing_ && tabsInsideTable_!=enabled){tabsInsideTable_=enabled;dirty_=full_=true;}
    }
    bool tabsInsideTable() const { return tabsInsideTable_; }
    void setChoiceMenus(bool enabled) { if(!editing_)choiceMenus_=enabled; }
    void setFooterKeycaps(bool enabled){keycaps_=enabled;dirty_=full_=true;}
    // Indicators: 0 chevrons (ASCII +/-), 1 +/-, 2 none.
    void setSectionIndicators(unsigned style){if(!editing_&&style<3){sectionIndicators_=style;dirty_=full_=true;}}
    void setSectionIndicatorsAlways(bool enabled){if(!editing_){sectionIndicatorsAlways_=enabled;dirty_=full_=true;}}
    void setSectionsCollapsible(bool enabled){if(!editing_){sectionsCollapsible_=enabled;dirty_=full_=true;}}
    void setReserveScrollbar(bool enabled){reserveScrollbar_=enabled;dirty_=full_=true;}
    void setCompactScrollbar(bool enabled){compactScrollbar_=enabled;dirty_=full_=true;}
    // Caller retains the marker string for the lifetime of this setting.
    void setSelectionMarker(const char *marker){selectionMarker_=marker;dirty_=full_=true;}
    void setFooterGrid(bool enabled){footerGrid_=enabled;dirty_=full_=true;}
    void setSynchronizedOutput(bool enabled){syncOutput_=enabled;}
    void setFooterEmphasis(bool enabled){footerEmphasis_=enabled;dirty_=full_=true;}
    void setTabBrackets(bool enabled){tabBrackets_=enabled;dirty_=full_=true;}
    void setTrueColor(bool enabled) { if (trueColor_!=enabled) {trueColor_=enabled;dirty_=full_=true;} }
    bool trueColor() const { return trueColor_; }
    void feedback(const char *text) {
        setNotice(text);
    }
    // A standalone ESC needs an idle delimiter (~35ms); CSI may span callbacks.
    // Only the transport owner decides that delimiter. Never flush each chunk.
    bool flushEscape(Action &action) {
        action = Action{};
        if (escape_ != 1)
            return false;
        escape_ = escapeBytes_ = 0;
        if (editing_) {
            clearDraft();
            dirty_ = full_ = true;
            return false;
        }
        if (!active_) {
            return false;
        }
        active_ = false;
        action.type = Action::Exit;
        return true;
    }
    bool feed(uint8_t ch, const Snapshot &snapshot, Action &action) {
        action = Action{};
        if (!active_)
            return false;
        syncSections(snapshot);
        if(!validateChoice(snapshot))return false;
        if (crPending_ && (ch == '\n' || ch == 0)) {
            crPending_ = false;
            return false;
        }
        if (ch != '\n' && ch != '\r')
            crPending_ = false;
        if (escape_) {
            if(escape_==4){
                if(unsigned(escapeBytes_)+1>=sizeof(mousePacket_)){escape_=5;return false;}
                mousePacket_[escapeBytes_++]=char(ch);mousePacket_[escapeBytes_]=0;
                if(ch>=0x40&&ch<=0x7e){
                    ps_mouse_event event;escape_=0;
                    if(PS_MOUSE&&ps_mouse_parse(mousePacket_,&event))return mouse(event,snapshot,action);
                }
                return false;
            }
            if(escape_==5){if(ch>=0x40&&ch<=0x7e)escape_=0;return false;}
            if (escape_ == 1) {
                if (ch == '[' || ch == 'O') {
                    escape_ = 2;
                    return false;
                }
                escape_ = escapeBytes_ = 0;
                if (ch >= 'A' && ch <= 'D')
                    return arrow(ch, snapshot, action);
                return false;
            }
            if(PS_MOUSE&&escape_==2&&ch=='<'&&escapeBytes_==0){
                escape_=4;escapeBytes_=1;mousePacket_[0]='<';return false;
            }
            if (++escapeBytes_ > 16) {
                escape_ = escapeBytes_ = 0;
                return false;
            }
            if (ch >= '0' && ch <= '9')
                attribute_ = attribute_ > 24 ? 255 : uint8_t(attribute_ * 10 + ch - '0');
            if (ch >= 0x40 && ch <= 0x7e) {
                const uint8_t attribute = attribute_;
                escape_ = escapeBytes_ = attribute_ = 0;
                if (snapshot.lines < 16 || snapshot.columns < 40)
                    return false;
                if (PS_DEVICE_VIEW_DIALOGS && ch == 'Q')
                    return startDialog(snapshot, action);
                if (ch == '~') {
                    if (PS_DEVICE_VIEW_DIALOGS && attribute == 12)
                        return startDialog(snapshot,action);
                    if (attribute == 1 || attribute == 7)
                        ch = 'H';
                    else if (attribute == 4 || attribute == 8)
                        ch = 'F';
                    else if (attribute == 5)
                        ch = 'P';
                    else if (attribute == 6)
                        ch = 'N';
                    else if (attribute == 3 && editing_)
                        return edit(4, snapshot, action);
                    else
                        return false;
                }
                if (ch == 'Z' && !editing_) {
                    nextPage(snapshot, -1);
                    return false;
                }
                return arrow(ch, snapshot, action);
            }
            return false;
        }
        if (ch == 27) {
            escape_ = 1;
            escapeBytes_ = attribute_ = 0;
            return false;
        }
        if (ch == 3) {
            if (editing_) {
                clearDraft();
                dirty_ = full_ = true;
                return false;
            }
            active_ = false;
            action.type = Action::Exit;
            return true;
        }
        if (ch == 12) {
            dirty_ = full_ = true;
            return false;
        }
        if (snapshot.lines < 16 || snapshot.columns < 40)
            return false;
        if (editing_)
            return edit(ch, snapshot, action);
        if (sectionFocus_ && sections() && !grid_ && (ch==' ' || ch=='\r' || ch=='\n')) {
            toggleSection(snapshot,0);crPending_=ch=='\r';return false;
        }
        if (PS_DEVICE_VIEW_WIDGETS && !grid_ && (ch == ' ' || ch == '+' || ch == '-'))
            return adjust(ch, snapshot, action);
        if (ch == '\t') {
            nextPage(snapshot);
            return false;
        }
        if (ch == 18) {
            action.type = Action::Refresh;
            return true;
        }
        if (page_ == Game && grid_) {
            if (ch >= '1' && ch <= '9') {
                if (!snapshot.canGame || !snapshot.gameReady ||
                    !(snapshot.enabledKeys & (1U << (ch - '1')))) {
                    setNotice("Box unavailable: check AVR, enabled mask or maintenance");
                    return false;
                }
                action.type = Action::Press;
                action.index = ch - '0';
                return true;
            }
            if (ch == 'r') {
                if (!snapshot.canGame || !snapshot.gameReady) {
                    setNotice("Game unavailable");
                    return false;
                }
                action.type = Action::Reset;
                return true;
            }
        }
        if ((page_ == Led && !snapshot.canLeds) || (page_ == Pixel && !pixels(snapshot)))
            return false;
        if (page_ == Pixel)
            syncPixel(snapshot);
        if (ch == 'f' && (page_ == Led || page_ == Pixel || page_ == Game)) {
            grid_ = !grid_;
            dirty_ = full_ = true;
            return false;
        }
        if (page_ == Led && grid_ && snapshot.canLeds && (ch == 'w' || ch == 'r' || ch == 'o')) {
            action.type = Action::Cell;
            action.index = cell_;
            copy(action.key, sizeof(action.key), "cell");
            copy(action.value, sizeof(action.value),
                 ch == 'w'   ? "white"
                 : ch == 'r' ? "red"
                             : "off");
            return true;
        }
        if (page_ == Pixel && pixels(snapshot)) {
            if (ch == 'g') {
                group_ = !group_;
                syncPixel(snapshot);
                dirty_ = true;
                return false;
            }
            if (ch == 'w' || ch == 'r' || ch == 'o') {
                pixelColor_ = ch == 'w' ? 0 : ch == 'r' ? 1 : 2;
                dirty_ = true;
                return false;
            }
            if (grid_ && (ch == '+' || ch == '-'))
                return stepPixel(ch == '+' ? 1 : -1, snapshot, action);
        }
        if (ch == '\r' || ch == '\n') {
            crPending_ = ch == '\r';
            if (grid_ && page_ == Pixel && pixels(snapshot))
                return pixelAction(snapshot, action);
            if (grid_ && (page_ == Led || page_ == Game)) {
                grid_ = false;
                dirty_ = true;
                return false;
            }
            const Row *row = currentRow(snapshot);
            if (row && row->editable && !row->pending && keySafe(row->key)) {
                if (!row->secret && !plain(row->value, 384)) {
                    setNotice("Value is not editable ASCII");
                    return false;
                }
                copy(editKey_, sizeof(editKey_), row->key);
                copy(editLabel_, sizeof(editLabel_), row->label);
                secret_ = row->secret;
                observed_ = rowHash(*row);
                copy(draft_, sizeof(draft_), secret_ ? "" : row->value);
                cursor_ = ps_text_length(draft_);
                editing_ = true;
                draftChanged_ = false;
                choiceMenu_=PS_DEVICE_VIEW_CHOICE_MENU&&choiceMenus_&&ansi_&&!secret_&&row->kind==Choice&&row->choices&&row->choiceCount;
                if(choiceMenu_){
                    choiceIndex_=0;
                    for(unsigned i=0;i<row->choiceCount;++i)if(ps_text_equal(row->value,row->choices[i]))choiceIndex_=uint8_t(i);
                    dialog_=true;
                }
                dirty_ = full_ = true;
                notice_[0] = 0;
            } else
                setNotice("Read only");
        }
        return false;
    }
    template <typename Write>
    bool render(const Snapshot &snapshot, Write write, bool force = false) {
        if (!active_)
            return false;
        syncSections(snapshot);
        validateChoice(snapshot);
        if (columns_ != snapshot.columns || lines_ != snapshot.lines) {
            columns_ = snapshot.columns;
            lines_ = snapshot.lines;
            dirty_ = full_ = true;
        }
        if (!editing_) {
            const uint32_t next = hash(snapshot);
            if (next != lastHash_)
                dirty_ = true;
            lastHash_ = next;
        }
        if (!dirty_ && !force)
            return false;
        /* Direct sink writes keep the closing marker outside the frame byte
           budget, including early returns and partial inline-editor updates. */
        struct Frame {
            Write &sink;bool enabled;
            Frame(Write &w,bool on):sink(w),enabled(on){if(enabled)sink((const uint8_t*)"\x1b[?2026h",8);}
            ~Frame(){if(enabled)sink((const uint8_t*)"\x1b[?2026l",8);}
        } frame(write,ansi_&&syncOutput_);
        dirty_ = false;
        if(PS_MOUSE&&ansi_&&full_){const char *p="\x1b[?1002h\x1b[?1006h";write((const uint8_t*)p,ps_text_length(p));}
        if (force) full_=true;
        if (editing_ && ansi_ && columns_ >= 40 && lines_ >= 16 && !full_ && !force) {
            editLine(write);
            return true;
        }
        return renderTable(snapshot, write);
    }

  private:
    template <typename Write> class Writer {
      public:
        explicit Writer(Write write, bool ansi = false) : write_(write), ansi_(ansi) {}
        void indent(unsigned column) { indent_=column; }
        void position(unsigned row, unsigned column=1) {
            if(ansi_){char b[32];ps_terminal_format(b,sizeof(b),"\x1b[%u;%uH",row,column+indent_);raw(b);}
        }
        void raw(const char *text) {
            while (*text) {
                const char *end = ansi_ ? ps_text_find(text, "\r\n") : nullptr;
                const size_t n = end ? size_t(end - text) : ps_text_length(text);
                if (n + (end ? 5 : 0) > remaining_)
                    return;
                write_(reinterpret_cast<const uint8_t *>(text), n);
                remaining_ -= n;
                if (!end)
                    return;
                write_(reinterpret_cast<const uint8_t *>("\x1b[K\r\n"), 5);
                remaining_ -= 5;
                if(indent_){char b[24];ps_terminal_format(b,sizeof(b),"\x1b[%uG",indent_+1);raw(b);}
                text = end + 2;
            }
        }
        void safe(const char *text, size_t width) {
            char clean[241];clean[0]=0;
            terminalText(clean, width + 1 < sizeof(clean) ? width + 1 : sizeof(clean), text);
            raw(clean);
        }
        void field(const char *text, size_t width) {
            char clean[241];clean[0]=0;
            const size_t n =
                terminalText(clean, width + 1 < sizeof(clean) ? width + 1 : sizeof(clean), text);
            raw(clean);
            for (size_t i = n; i < width; ++i)
                raw(" ");
        }

      private:
        Write write_;
        // This limits emitted bytes, not storage. Both layouts can fill a tall viewport.
        size_t remaining_ = PS_DEVICE_VIEW_FRAME_BYTES;
        bool ansi_;
        unsigned indent_ = 0;
    };
    static void scrub(char *p, size_t n) {
        volatile char *out = p;
        while (n--)
            *out++ = 0;
    }
    static void copy(char *out, size_t size, const char *in) {
        size_t i = 0;
        if (!size)
            return;
        while (in && in[i] && i + 1 < size) {
            out[i] = in[i];
            ++i;
        }
        out[i] = 0;
    }
    static bool plain(const char *text, size_t maximum) {
        if (!text)
            return true;
        size_t n = 0;
        while (text[n] && n <= maximum) {
            if (text[n] < 32 || text[n] > 126)
                return false;
            ++n;
        }
        return n <= maximum;
    }
    static bool keySafe(const char *key) {
        if (!key || !key[0])
            return false;
        size_t i = 0;
        while (key[i] && i < 32) {
            const char c = key[i++];
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                  c == '_' || c == '.' || c == '-'))
                return false;
        }
        return i < 32;
    }
    static uint8_t rowCount(const Snapshot &s) {
        return s.rows ? (s.rowCount > PS_DEVICE_VIEW_MAX_ROWS ? PS_DEVICE_VIEW_MAX_ROWS : s.rowCount) : 0;
    }
    const Row *currentRow(const Snapshot &s) const {
        return !sectionFocus_ && selected_ < rowCount(s) ? s.rows + selected_ : nullptr;
    }
    bool sections() const { return PS_DEVICE_VIEW_SECTIONS && groups_==SectionRows; }
    static bool sectionStart(const Snapshot &s,unsigned i) {
        return s.rows[i].category && *s.rows[i].category &&
            (!i || !ps_text_equal(s.rows[i].category,s.rows[i-1].category));
    }
    bool sectionClosed(unsigned i) const {
        return sectionsCollapsible_ && sections() && (closed_[page_][i/8] & (1U<<(i%8)));
    }
    void syncSections(const Snapshot &s) {
        if (!sections()) return;
        uint32_t signature=2166136261U;
        const unsigned count=rowCount(s);
        for(unsigned i=0;i<count;++i){
            for(unsigned field=0;field<2;++field) {
                const char *part=field?s.rows[i].category:s.rows[i].key;
                for(unsigned j=0;part&&part[j]&&j<512;++j){signature^=uint8_t(part[j]);signature*=16777619U;}
                signature^=255;signature*=16777619U;
            }
        }
        if(sectionSchema_[page_]!=signature){
            sectionSchema_[page_]=signature;
            for(unsigned i=0;i<sizeof closed_[0];++i)closed_[page_][i]=0;
            sectionFocus_=false;dirty_=full_=true;
        }
        if(selected_>=count){selected_=count?uint8_t(count-1):0;sectionFocus_=false;}
        if(count && sectionFocus_ && !sectionStart(s,selected_))sectionFocus_=false;
        unsigned start=0;bool closed=false;
        for(unsigned i=0;i<count;++i){
            if(!i || !ps_text_equal(s.rows[i].category,s.rows[i-1].category)){
                start=i;closed=sectionStart(s,i)&&sectionClosed(i);
            }
            if(i==selected_ && closed){selected_=uint8_t(start);sectionFocus_=true;break;}
        }
    }
    unsigned sectionItems(const Snapshot &s,uint16_t *items,unsigned &focus,bool frame=false) const {
        unsigned total=0;bool closed=false;focus=0;
        for(unsigned i=0;i<rowCount(s);++i){
            if(sections() && (!i || !ps_text_equal(s.rows[i].category,s.rows[i-1].category))){
                closed=false;
                if(sectionStart(s,i)){
                    // Adjacent headings share a rule; the column header frames the first.
                    if(frame&&!sectionBorderTitles_&&sectionBorders_&&total&&!(items[total-1]&0x2000))items[total++]=uint16_t(i|0x4000);
                    if(selected_==i&&sectionFocus_)focus=total;
                    items[total++]=uint16_t(i|0x8000);closed=sectionClosed(i);
                    if(frame&&sectionBorders_&&sectionBottomBorder_)items[total++]=uint16_t(i|0x2000);
                }
            }
            if(!closed){
                if(selected_==i&&!sectionFocus_)focus=total;
                items[total++]=uint16_t(i);
            }
        }
        return total;
    }
    void toggleSection(const Snapshot &s,int direction) {
        if(!sectionsCollapsible_||!sections()||!sectionFocus_||selected_>=rowCount(s)||!sectionStart(s,selected_))return;
        uint8_t &bits=closed_[page_][selected_/8];const unsigned mask=1U<<(selected_%8);
        if(direction<0)bits|=uint8_t(mask);else if(direction>0)bits&=uint8_t(~mask);else bits^=uint8_t(mask);
        notice_[0]=0;dirty_=full_=true;
    }
    static bool pixels(const Snapshot &s) {
        if (!PS_DEVICE_VIEW_DIAGNOSTICS || !s.canPixels || !s.pixelCount || s.pixelCount > 480 ||
            !s.usedPixels || s.usedPixels > s.pixelCount || !s.groupCount || s.groupCount > 80 ||
            !s.groupCounts || !s.groupStarts)
            return false;
        uint32_t prefix = 0;
        for (uint8_t i = 0; i < s.groupCount; ++i) {
            if (!s.groupCounts[i] || s.groupStarts[i] != prefix)
                return false;
            prefix += s.groupCounts[i];
            if (prefix > s.pixelCount)
                return false;
        }
        return prefix == s.usedPixels;
    }
    // Call only after pixels(s) succeeds. Never synthesize six/twelve-pixel spans.
    static uint8_t groupAt(const Snapshot &s, uint16_t pixel) {
        for (uint8_t i = 0; i < s.groupCount; ++i)
            if (pixel < s.groupStarts[i] + s.groupCounts[i])
                return i;
        return s.groupCount - 1;
    }
    void syncPixel(const Snapshot &s) {
        if (pixel_ >= s.usedPixels)
            pixel_ = s.usedPixels - 1;
        if (group_)
            pixel_ = s.groupStarts[groupAt(s, pixel_)];
    }
    const char *color() const {
        return pixelColor_ == 0 ? "white" : pixelColor_ == 1 ? "red" : "off";
    }
    void clearDraft() {
        scrub(draft_, sizeof(draft_));
        scrub(editKey_, sizeof(editKey_));
        scrub(editLabel_, sizeof(editLabel_));
        cursor_ = 0;
        editing_ = secret_ = draftChanged_ = false;
        dialog_ = false;
        choiceMenu_=false;dialogFocus_=DraftFocus;choiceIndex_=0;
    }
    void setNotice(const char *text) {
        terminalText(notice_, sizeof(notice_), text);
        dirty_ = true;
    }
    void nextPage(const Snapshot &s, int direction = 1) {
        for (uint8_t i = 0; i < PageCount; ++i) {
            page_ = static_cast<Page>((page_ + PageCount + direction) % PageCount);
            if ((page_ != Led || (PS_DEVICE_VIEW_DIAGNOSTICS && s.canLeds)) &&
                (page_ != Pixel || pixels(s)) &&
                (page_ != Game || (PS_DEVICE_VIEW_DIAGNOSTICS && s.canGame)) &&
                (page_ != TerminalPage || s.terminalSettings) &&
                (page_ != Cloud || s.cloudSettings) &&
                (page_ != Notifications || s.notificationSettings) &&
                (page_ != Clock || s.clockSettings))
                break;
        }
        selected_ = 0;
        grid_ = page_ == Led || page_ == Pixel || page_ == Game;
        sectionFocus_ = false;
        dirty_ = full_ = true;
        notice_[0] = 0;
    }
    bool pixelAction(const Snapshot &s, Action &a) {
        if (!pixels(s))
            return false;
        syncPixel(s);
        a.type = Action::Pixel;
        a.index = pixel_;
        a.group = group_;
        a.count = group_ ? s.groupCounts[groupAt(s, pixel_)] : 1;
        a.groupLayoutRevision = s.groupLayoutRevision;
        copy(a.key, sizeof(a.key), "pixel");
        copy(a.value, sizeof(a.value), color());
        return true;
    }
    bool stepPixel(int direction, const Snapshot &s, Action &a) {
        if (!pixels(s))
            return false;
        syncPixel(s);
        int next = static_cast<int>(pixel_) + direction;
        if (group_) {
            const int group = static_cast<int>(groupAt(s, pixel_)) + direction;
            if (group < 0 || group >= s.groupCount) {
                setNotice("Pixel boundary");
                return false;
            }
            next = s.groupStarts[group];
        }
        if (next < 0 || next >= s.usedPixels) {
            setNotice("Pixel boundary");
            return false;
        }
        pixel_ = static_cast<uint16_t>(next);
        dirty_ = true;
        return pixelAction(s, a);
    }
    bool arrow(uint8_t key, const Snapshot &s, Action &a) {
        if (editing_) {
            if(choiceMenu_)return choiceKey(key,s,a);
            if (key == 'D' && cursor_)
                --cursor_;
            else if (key == 'C' && cursor_ < ps_text_length(draft_))
                ++cursor_;
            else if (key == 'H')
                cursor_ = 0;
            else if (key == 'F')
                cursor_ = ps_text_length(draft_);
            dirty_ = true;
            return false;
        }
        if (grid_ && page_ == Led && s.canLeds) {
            if(PS_DEVICE_VIEW_MATRIX&&s.cellStaggered&&(key=='A'||key=='B'||key=='C'||key=='D')){
                const unsigned n=cell_-1,x=2*(n%10)+(n/10)%2,y=n/20;
                const int nx=int(x)+(key=='C'?1:key=='D'?-1:0),ny=int(y)+(key=='B'?1:key=='A'?-1:0);
                if(nx>=0&&nx<20&&ny>=0&&ny<4)cell_=uint8_t(ny*20+(nx%2)*10+nx/2+1);
                dirty_=true;return false;
            }
            int next = cell_ + (key == 'D'   ? -1
                                : key == 'C' ? 1
                                : key == 'A' ? -10
                                : key == 'B' ? 10
                                             : 0);
            if (key == 'H')
                next = 1;
            else if (key == 'F')
                next = 80;
            else if (key == 'P')
                next = cell_ > 40 ? cell_ - 40 : 1;
            else if (key == 'N')
                next = cell_ < 40 ? cell_ + 40 : 80;
            if (next >= 1 && next <= 80) {
                cell_ = static_cast<uint8_t>(next);
            }
            dirty_ = true;
            return false;
        }
        if (grid_ && page_ == Pixel && pixels(s) &&
            (key == 'D' || key == 'C' || key == 'A' || key == 'B'))
            return stepPixel(key == 'D' || key == 'A' ? -1 : 1, s, a);
        if (grid_ && page_ == Pixel && pixels(s) &&
            (key == 'H' || key == 'F' || key == 'P' || key == 'N')) {
            syncPixel(s);
            const uint16_t old = pixel_;
            if (key == 'H')
                pixel_ = 0;
            else if (key == 'F')
                pixel_ = s.usedPixels - 1;
            else if (group_) {
                const int next = int(groupAt(s, pixel_)) + (key == 'P' ? -4 : 4);
                pixel_ = s.groupStarts[next < 0               ? 0
                                       : next >= s.groupCount ? s.groupCount - 1
                                                              : next];
            } else if (key == 'P')
                pixel_ = pixel_ > 8 ? pixel_ - 8 : 0;
            else
                pixel_ = pixel_ + 8 < s.usedPixels ? pixel_ + 8 : s.usedPixels - 1;
            syncPixel(s);
            dirty_ = true;
            return pixel_ != old && pixelAction(s, a);
        }
        const uint8_t count = rowCount(s);
        if(sections() && !grid_){
            if(sectionFocus_ && (key=='C'||key=='D')){toggleSection(s,key=='C'?1:-1);return false;}
            if(key=='A'||key=='B'||key=='H'||key=='F'||key=='P'||key=='N'){
                uint16_t items[PS_DEVICE_VIEW_MAX_ROWS*2];unsigned focus;
                const unsigned total=sectionItems(s,items,focus);
                if(total){
                    if(key=='A')focus=focus?focus-1:0;
                    else if(key=='B')focus=focus+1<total?focus+1:total-1;
                    else if(key=='H')focus=0;else if(key=='F')focus=total-1;
                    else if(key=='P')focus=focus>4?focus-4:0;
                    else focus=focus+4<total?focus+4:total-1;
                    selected_=uint8_t(items[focus]&0x7fff);sectionFocus_=(items[focus]&0x8000)!=0;
                    dirty_=true;
                }
                return false;
            }
        }
        if (key == 'C' || key == 'D') {
            const Row *row=currentRow(s);
            if(PS_DEVICE_VIEW_WIDGETS&&row&&(row->kind==Level||row->kind==Choice))
                return adjust(key=='C'?'+':'-',s,a);
            nextPage(s, key == 'C' ? 1 : -1);
            return false;
        }
        if (key == 'A' && selected_)
            --selected_;
        else if (key == 'B' && selected_ + 1 < count)
            ++selected_;
        else if (key == 'H')
            selected_ = 0;
        else if (key == 'F')
            selected_ = count ? count - 1 : 0;
        else if (key == 'P')
            selected_ = selected_ > 4 ? selected_ - 4 : 0;
        else if (key == 'N')
            selected_ = count ? uint8_t(selected_ + 4 < count ? selected_ + 4 : count - 1) : 0;
        dirty_ = true;
        return false;
    }
    bool edit(uint8_t ch, const Snapshot &s, Action &action) {
        if(choiceMenu_&&ch!='\r'&&ch!='\n')return choiceKey(ch,s,action);
        if(dialog_&&!choiceMenu_&&ch=='\t'){
            dialogFocus_=uint8_t((dialogFocus_+1)%DialogFocusCount);dirty_=full_=true;return false;
        }
        if(dialog_&&!choiceMenu_&&dialogFocus_==CancelFocus&&(ch=='\r'||ch=='\n')){
            clearDraft();dirty_=full_=true;return false;
        }
        if (ch == '\r' || ch == '\n') {
            crPending_ = ch == '\r';
            const Row *row = currentRow(s);
            if ((page_ == Led && !s.canLeds) || (page_ == Pixel && !pixels(s)) ||
                (page_ == Game && !s.canGame) || !row || !row->editable || row->pending || !keySafe(row->key) ||
                !ps_text_equal(row->key, editKey_) || row->secret != secret_) {
                clearDraft();
                setNotice("Field changed; edit cancelled");
                full_ = true;
                return false;
            }
            if (!draftChanged_) {
                clearDraft();
                dirty_ = full_ = true;
                return false;
            }
            if (observed_ != rowHash(*row)) { setNotice("State changed; reopen editor"); full_=true; return false; }
            if (!validValue(*row, draft_)) { setNotice("Invalid value or outside range"); full_=true; return false; }
            action.type = Action::Set;
            copy(action.key, sizeof(action.key), editKey_);
            copy(action.value, sizeof(action.value), draft_);
            clearDraft();
            dirty_ = full_ = true;
            return true;
        }
        if (ch == 1)
            cursor_ = 0;
        else if (ch == 5)
            cursor_ = ps_text_length(draft_);
        else if (ch == 21) {
            if (cursor_) {
                ps_bytes_move(draft_, draft_ + cursor_, ps_text_length(draft_ + cursor_) + 1);
                cursor_ = 0;
                draftChanged_ = true;
            }
        } else if (ch == 23) {
            size_t target = cursor_;
            while (target && draft_[target - 1] == ' ')
                --target;
            while (target && draft_[target - 1] != ' ')
                --target;
            if (target < cursor_) {
                ps_bytes_move(draft_ + target, draft_ + cursor_,
                              ps_text_length(draft_ + cursor_) + 1);
                cursor_ = target;
                draftChanged_ = true;
            }
        } else if (ch == 8 || ch == 127) {
            if (cursor_) {
                --cursor_;
                ps_bytes_move(draft_ + cursor_, draft_ + cursor_ + 1,
                              ps_text_length(draft_ + cursor_));
                draftChanged_ = true;
            }
        } else if (ch == 4) {
            if (cursor_ < ps_text_length(draft_)) {
                ps_bytes_move(draft_ + cursor_, draft_ + cursor_ + 1,
                              ps_text_length(draft_ + cursor_));
                draftChanged_ = true;
            }
        } else if (ch >= 32 && ch < 127) {
            const size_t size = ps_text_length(draft_);
            if (size < 384) {
                ps_bytes_move(draft_ + cursor_ + 1, draft_ + cursor_, size - cursor_ + 1);
                draft_[cursor_++] = static_cast<char>(ch);
                draftChanged_ = true;
            }
        } else
            return false; // Form owns keys: 'r'/Tab cannot invoke global shortcuts.
        dirty_ = true;
        return false;
    }
    size_t editCapacity() const { return editWidth_; }
    size_t editStart(size_t capacity) const {
        return cursor_ >= capacity ? cursor_ - capacity + 1 : 0;
    }
    template <typename Write> void editValue(Writer<Write> &out, size_t capacity) const {
        const size_t length = ps_text_length(draft_), start = editStart(capacity);
        char visible[241]{};
        for (size_t i = 0; i < capacity && start + i < length; ++i)
            visible[i] = secret_ ? '*' : draft_[start + i];
        if (ansi_)
            out.raw(ps_style_surface(theme_,trueColor_,1));
        out.field(visible, capacity);
        if (ansi_)
            out.raw(ps_style_base(theme_,trueColor_));
    }
    template <typename Write> void editLine(Write write) const {
        Writer<Write> out(write,ansi_);
        if(choiceMenu_||(dialog_&&dialogFocus_!=DraftFocus)){out.raw("\x1b[?25l");return;}
        char position[24];
        ps_terminal_format(position,sizeof(position),"\x1b[%u;%uH",editRow_,editColumn_);
        out.raw(position);editValue(out,editWidth_);
        ps_terminal_format(position,sizeof(position),"\x1b[%u;%uH",editRow_,editColumn_+unsigned(cursor_-editStart(editWidth_)));
        out.raw(position);out.raw("\x1b[?25h");
    }
    static uint32_t hash(const Snapshot &s) {
        uint32_t value = 2166136261U;
        auto add = [&](const char *text) {
            for (size_t i = 0; text && text[i] && i < 384; ++i) {
                value ^= static_cast<uint8_t>(text[i]);
                value *= 16777619U;
            }
        };
        add(s.title);
        value ^= (s.terminalSettings?1U:0U)|(s.cloudSettings?2U:0U)|(s.notificationSettings?4U:0U)|(s.clockSettings?8U:0U);
        value *= 16777619U;
        value ^= s.canLeds ? 1 : 0;
        value ^= s.cellStaggered ? 32 : 0;
        value ^= s.canPixels ? 2 : 0;
        value ^= s.pixelCount;
        value ^= s.enabledKeys;
        value *= 16777619U;
        value ^= s.winner;
        value *= 16777619U;
        value ^= s.columns;
        value *= 16777619U;
        value ^= s.lines;
        value *= 16777619U;
        value ^= (s.canGame ? 16 : 0) | (s.gameReady ? 32 : 0) | (s.unicode ? 64 : 0);
        value ^= s.usedPixels;
        value *= 16777619U;
        value ^= s.groupLayoutRevision;
        value *= 16777619U;
        if (pixels(s))
            for (uint8_t i = 0; i < s.groupCount; ++i) {
                value ^= s.groupCounts[i];
                value *= 16777619U;
                value ^= s.groupStarts[i];
                value *= 16777619U;
            }
        for (uint8_t i = 0; i < rowCount(s); ++i) {
            add(s.rows[i].label);
            add(s.rows[i].key);
            add(s.rows[i].category);
            add(s.rows[i].unit);add(s.rows[i].onLabel);add(s.rows[i].offLabel);
            for(unsigned j=0;s.rows[i].choiceLabels&&j<s.rows[i].choiceCount;++j)add(s.rows[i].choiceLabels[j]);
            if (!s.rows[i].secret)
                add(s.rows[i].value);
            value ^= (s.rows[i].editable ? 4 : 0) | (s.rows[i].secret ? 8 : 0);
            value ^= (unsigned(s.rows[i].kind)<<8)|(s.rows[i].pending?16:0);
            value ^= s.rows[i].meter?32:0;
            value ^= s.rows[i].meterStyle;value *= 16777619U;
            value *= 16777619U;
            value ^= s.rows[i].minimum;value *= 16777619U;
            value ^= s.rows[i].maximum;value *= 16777619U;
        }
        if (s.canLeds && s.cellStates)
            for (uint8_t i = 0; i < 80; ++i) {
                value ^= s.cellStates[i];
                value *= 16777619U;
            }
        if(s.canLeds&&s.cellColors)for(unsigned i=0;i<80;++i){value^=s.cellColors[i];value*=16777619U;}
        if(s.cellStateNames)for(unsigned i=0;i<4;++i)add(s.cellStateNames[i]);
        return value;
    }
    Page page_ = Device;
#include "portable_device_table.hpp"
    Theme theme_ = Default;
    TabLayout tabs_ = HorizontalTabs;
    GroupLayout groups_ = PS_DEVICE_VIEW_CATEGORY_COLUMN ? CategoryColumn : SectionRows;
    bool sectionFocus_ = false;
    bool sectionBorders_ = true;
    bool sectionBottomBorder_ = false;
    bool sectionBorderTitles_ = PS_SECTION_BORDER_TITLES!=0;
    bool tabsInsideTable_ = false;
    uint8_t closed_[PS_DEVICE_VIEW_SECTIONS?PageCount:1][PS_DEVICE_VIEW_SECTIONS?(PS_DEVICE_VIEW_MAX_ROWS+7)/8:1]{};
    uint32_t sectionSchema_[PS_DEVICE_VIEW_SECTIONS?PageCount:1]{};
    enum DialogFocus : uint8_t { DraftFocus, ApplyFocus, CancelFocus, DialogFocusCount };
    bool trueColor_ = true, dialog_ = false;
    bool choiceMenus_=true,choiceMenu_=false;
    uint8_t choiceIndex_=0,dialogFocus_=DraftFocus;
    bool keycaps_=PS_FOOTER_KEYCAPS!=0,tabBrackets_=PS_TAB_BRACKETS!=0;
    bool compactScrollbar_=false;
    const char *selectionMarker_=nullptr;
    bool sectionsCollapsible_=true,reserveScrollbar_=false;
    bool sectionIndicatorsAlways_=false;
    unsigned sectionIndicators_=0,paintedGutter_=0;
    bool footerGrid_=PS_FOOTER_GRID!=0,footerEmphasis_=false;
    bool syncOutput_=PS_SYNC_OUTPUT!=0;
    unsigned editRow_ = 6, editColumn_ = 30, editWidth_ = 40;
    uint32_t observed_ = 0;
    uint32_t painted_[PS_DEVICE_VIEW_RETAIN_ROWS ? PS_DEVICE_VIEW_MAX_ROWS : 1]{};
    unsigned paintedSelection_ = 255;
    unsigned paintedRows_ = 0;
    bool paintedGrid_ = false;
    uint8_t selected_ = 0, cell_ = 1, pixelColor_ = 1;
    uint16_t pixel_ = 0, columns_ = 80, lines_ = 24;
    size_t cursor_ = 0;
    bool active_ = false, ansi_ = true, dirty_ = true, full_ = true, editing_ = false,
         secret_ = false, draftChanged_ = false, grid_ = false, group_ = false, crPending_ = false;
    uint8_t escape_ = 0, escapeBytes_ = 0, attribute_ = 0;
    char mousePacket_[PS_MOUSE?24:1]{};
    bool mouseDrag_=false;
    unsigned mouseStart_=0,mouseVisible_=0;
    uint32_t lastHash_ = 0;
    char draft_[385]{}, editKey_[32]{}, editLabel_[49]{}, notice_[65]{};
};
} // namespace PortableShell

#endif
