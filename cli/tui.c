#define _POSIX_C_SOURCE 200809L
#include "tui.h"
#include "protocol.h"
#include "portable_shell.h"
#include "portable_style.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#include <sys/ioctl.h>
#endif
#define HISTORY_SIZE 8192
#define DEVICE_LIMIT 128
/* Native I/O remains in this host adapter. PortableShell owns only UI bytes. */
typedef struct workspace workspace;
typedef struct { workspace *w; unsigned action,index; } binding;
enum { MENU=1,PORTS,SETTINGS,CONNECT,TERMINAL,DETAILS,HISTORY,HEX_HISTORY,SEND_HEX,SIGNALS,QUIT,SELECT_PORT,BREAK_LINE,DTR_LOW,DTR_HIGH,RTS_LOW,RTS_HIGH,HELP };
struct workspace {
    sc_session *features; suart_selector selector; suart_config config; suart_port *port;
    suart_device_info identity,devices[DEVICE_LIMIT];
    ps_session *shell; void *memory; ps_command commands[18];
    ps_menu menu,picker; ps_menu_item items[18],port_items[DEVICE_LIMIT];
    binding actions[32],port_actions[DEVICE_LIMIT];
    ps_panel panel; unsigned revision,action,index,count,overflow,cols,rows;
    unsigned reconnect,write_timeout; uint64_t retry,rx,tx;
    int quit,error,output_error,raw,prefix,wanted,pinned,ascii;
    char path[512],instance[1024],serial[256],argument[PS_LINE],notice[256];
    char labels[DEVICE_LIMIT][640],report[PS_EDITOR_CAPACITY]; size_t report_size;
    unsigned char history[HISTORY_SIZE]; size_t history_head,history_size;
    ps_form form; ps_form_field form_field; char hex[PS_VALUE],detail_label[1024];
};
static void safe_text(char *dst,size_t size,const char *src) {
    size_t n=0; while(*src && n+1<size) { unsigned char c=(unsigned char)*src++; dst[n++]=(c<32 || c==127)?' ':(char)c; } dst[n]=0;
}
static void emit(void *ctx,const ps_byte *bytes,unsigned n) {
    workspace *w=(workspace *)ctx;
    if(fwrite(bytes,1,n,stdout)!=n || fflush(stdout)) w->output_error=1;
}
static void dimensions(unsigned *cols,unsigned *rows) {
    *cols=100; *rows=30;
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO info;
    if(GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE),&info)) { *cols=(unsigned)(info.srWindow.Right-info.srWindow.Left+1); *rows=(unsigned)(info.srWindow.Bottom-info.srWindow.Top+1); }
#else
    struct winsize size;
    if(!ioctl(1,TIOCGWINSZ,&size) && size.ws_col && size.ws_row) { *cols=size.ws_col; *rows=size.ws_row; }
#endif
}
static void status(workspace *w,const char *format,...) {
    va_list args; va_start(args,format); vsnprintf(w->notice,sizeof(w->notice),format,args); va_end(args);
    ps_message(w->shell,w->error?PS_STYLE_ERROR:PS_STYLE_MUTED,w->notice);
}
static void report(workspace *w,const char *format,...) {
    int n; va_list args; size_t room=sizeof(w->report)-w->report_size;
    if(room<2) return;
    va_start(args,format); n=vsnprintf(w->report+w->report_size,room,format,args); va_end(args);
    if(n>0) w->report_size+=(size_t)n<room?(size_t)n:room-1;
}
static void fact(workspace *w,const char *name,const char *value) {
    char clean[2048]; safe_text(clean,sizeof(clean),value); report(w,"%s: %s\n",name,*clean?clean:"Unknown");
}
static void open_report(workspace *w,const char *title) {
    ps_editor_set_style(w->shell,PS_EDITOR_HEADER|PS_EDITOR_BORDER|PS_EDITOR_READ_ONLY);
    ps_editor_open(w->shell,title,w->report,NULL,NULL);
}
static int menu_action(ps_session *s,void *ctx) {
    binding *b=(binding *)ctx; (void)s; b->w->action=b->action; b->w->index=b->index; return 0;
}
static void add_item(workspace *w,const char *label,unsigned action) {
    unsigned n=w->menu.count++; binding *b=&w->actions[n];
    b->w=w; b->action=action; w->items[n].label=label; w->items[n].run=menu_action; w->items[n].context=b;
}
static const char *notice(void *ctx,const ps_menu *menu,unsigned *role) {
    workspace *w=(workspace *)ctx; (void)menu; *role=w->error?PS_INK_ERROR:w->port?PS_INK_SUCCESS:PS_INK_INACTIVE; return w->notice;
}
static const char *description(void *ctx,const ps_menu *menu,unsigned index) {
    workspace *w=(workspace *)ctx;
    if(menu==&w->picker && index<w->count) { safe_text(w->detail_label,sizeof(w->detail_label),w->devices[index].instance_id); return w->detail_label; }
    if(menu==&w->menu) {
        unsigned action=w->actions[index].action;
        if(action==PORTS) return "Search with /. Select first, then Connect.";
        if(action==SETTINGS) return "Edit values in their cells. Serial framing is locked while connected.";
        if(action==CONNECT) return w->port?"Close the port and cancel reconnect.":"Open the selected device with the configured DTR policy.";
        if(action==TERMINAL) return "Full terminal byte stream. Ctrl+] then m returns here; Ctrl+] q quits.";
        if(action==HISTORY || action==HEX_HISTORY) return "Snapshot of the bounded receive history. Capture continues while viewing.";
        if(action==SEND_HEX) return "Compose and validate binary bytes before sending; nothing is replayed.";
        if(action==SIGNALS) return "Driver-reported modem state; unavailable signals remain Unknown.";
        if(action==DETAILS) return "Native identity, USB model, source, counters and driver configuration.";
        if(action==HELP) return "Commands, shortcuts, logging, reconnect and buffer limits.";
    }
    return "";
}
static void main_menu(workspace *w) {
    w->menu.title="SerialUART | Serial workspace"; w->menu.items=w->items; w->menu.count=0;
    memset(w->items,0,sizeof(w->items));
    add_item(w,"Ports",PORTS); add_item(w,"Settings",SETTINGS);
    add_item(w,w->port?"Disconnect":w->wanted?"Cancel reconnect":"Connect",CONNECT);
    if(w->port) { add_item(w,"Terminal",TERMINAL); add_item(w,"Send hex bytes",SEND_HEX); add_item(w,"Modem signals",SIGNALS); }
    if(w->identity.device.path[0]) add_item(w,"Device details",DETAILS);
    if(w->history_size) { add_item(w,"Receive history",HISTORY); add_item(w,"Hex history",HEX_HISTORY); }
    if(w->port) { add_item(w,"Send break (250 ms)",BREAK_LINE); add_item(w,"Set DTR low",DTR_LOW); add_item(w,"Set DTR high",DTR_HIGH); add_item(w,"Set RTS low",RTS_LOW); add_item(w,"Set RTS high",RTS_HIGH); }
    add_item(w,"Help",HELP); add_item(w,"Quit",QUIT);
    ps_menu_set_style(w->shell,PS_MENU_TABLE|PS_MENU_DETAILS|PS_MENU_BREADCRUMBS|PS_MENU_MARKER);
    ps_menu_open(w->shell,&w->menu);
}
static int collect(const suart_device_info *d,void *ctx) {
    workspace *w=(workspace *)ctx; suart_selector_ex filter; memset(&filter,0,sizeof(filter));
    filter.device=w->selector; filter.device.path=NULL; filter.instance_id=w->features->instance_id; filter.match=w->features->match;
    if(!suart_device_matches(&filter,d) || sc_device_state(w->features,&d->device)) return 0;
    if(w->features->board_name) { char board[512]; sc_board(w->features,&d->device,board,sizeof(board)); if(strcmp(board,w->features->board_name)) return 0; }
    if(w->count==DEVICE_LIMIT) { w->overflow=1; return 1; }
    w->devices[w->count++]=*d; return 0;
}
static int compare_ports(const void *a,const void *b) {
    const suart_device_info *x=(const suart_device_info *)a,*y=(const suart_device_info *)b;
    if(!strncmp(x->device.path,"COM",3) && !strncmp(y->device.path,"COM",3)) {
        unsigned long n=strtoul(x->device.path+3,NULL,10),m=strtoul(y->device.path+3,NULL,10); if(n!=m) return n>m?1:-1;
    }
    return strcmp(x->device.path,y->device.path);
}
static void ports(workspace *w) {
    unsigned i; int rc; w->count=w->overflow=0; rc=suart_enumerate_ex(collect,w);
    if(rc || w->overflow) { w->error=1; status(w,"Discovery failed or exceeds %u devices (status %d)",DEVICE_LIMIT,rc); return; }
    if(!w->count) { status(w,"No matching ports. Use connect PATH for a virtual serial device."); return; }
    qsort(w->devices,w->count,sizeof(w->devices[0]),compare_ports);
    for(i=0;i<w->count;++i) {
        suart_device *d=&w->devices[i].device; char label[1700];
        snprintf(label,sizeof(label),"%.120s | %.350s | %04X:%04X | %.120s",d->path,d->name,d->vid,d->pid,d->serial);
        safe_text(w->labels[i],sizeof(w->labels[i]),label);
        w->port_actions[i].w=w; w->port_actions[i].action=SELECT_PORT; w->port_actions[i].index=i;
        w->port_items[i].label=w->labels[i]; w->port_items[i].run=menu_action; w->port_items[i].context=&w->port_actions[i];
    }
    w->picker.title="Serial ports | / filter | Esc command prompt"; w->picker.items=w->port_items; w->picker.count=w->count;
    ps_menu_set_style(w->shell,PS_MENU_TABLE|PS_MENU_LIST|PS_MENU_DETAILS|PS_MENU_MARKER); ps_menu_open(w->shell,&w->picker);
}
static const char *const parities[]={"None","Even","Odd","Mark","Space"};
static const char *const stops[]={"1","2","1.5 (Windows, 5 bits)"};
static const char *const dtrs[]={"Keep","Low","High","Reset pulse"};
static const char *const displays[]={"UTF-8","ASCII","Hex controls"};
static const ps_control serial_controls[]={
    {0,"Serial framing","Baud",PS_CONTROL_TEXT,0,0,0,NULL,0},
    {1,"Serial framing","Data bits",PS_CONTROL_LEVEL,5,8,1,NULL,0},
    {2,"Serial framing","Parity",PS_CONTROL_CHOICE,0,4,1,parities,5},
    {3,"Serial framing","Stop bits",PS_CONTROL_CHOICE,0,2,1,stops,3},
    {4,"Serial framing","RTS / CTS",PS_CONTROL_TOGGLE,0,1,1,NULL,0},
    {5,"Open policy","DTR",PS_CONTROL_CHOICE,0,3,1,dtrs,4},
    {6,"Open policy","Boot wait (ms)",PS_CONTROL_LEVEL,0,60000,100,NULL,0},
    {7,"Open policy","Reconnect ms (0 off)",PS_CONTROL_LEVEL,0,60000,100,NULL,0}};
static const ps_control terminal_controls[]={
    {8,"Session","Local echo",PS_CONTROL_TOGGLE,0,1,1,NULL,0},
    {9,"Session","Line editing",PS_CONTROL_TOGGLE,0,1,1,NULL,0},
    {10,"Session","Display",PS_CONTROL_CHOICE,0,2,1,displays,3},
    {11,"Session","Strip remote titles",PS_CONTROL_TOGGLE,0,1,1,NULL,0},
    {12,"Session","Enter bytes (hex)",PS_CONTROL_TEXT,0,0,0,NULL,0}};
static const ps_control_page pages[]={{"Connection",serial_controls,8},{"Terminal",terminal_controls,5}};
static int read_setting(void *ctx,unsigned id,ps_control_value *v) {
    workspace *w=(workspace *)ctx; const char *p; size_t i;
    v->flags=PS_CONTROL_VALID|PS_CONTROL_NUMBER_ONLY; v->revision=w->revision;
    if(id<8 && (w->port || w->wanted)) v->flags|=PS_CONTROL_LOCKED;
    switch(id) {
    case 0: snprintf(v->text,sizeof(v->text),"%u",w->config.baud); break;
    case 1: v->number=w->config.data_bits; break;
    case 2: p=strchr("NEOMS",w->config.parity); v->number=p?(unsigned)(p-"NEOMS"):0; break;
    case 3: v->number=w->config.stop_bits==15?2:w->config.stop_bits-1; break;
    case 4: v->number=(unsigned)w->config.rts_cts; break;
    case 5: v->number=(unsigned)w->config.dtr; break;
    case 6: v->number=w->config.boot_wait_ms; break;
    case 7: v->number=w->reconnect; break;
    case 8: v->number=(unsigned)w->features->echo; break;
    case 9: v->number=(unsigned)w->features->line_buffer; break;
    case 10: v->number=(unsigned)w->features->display; break;
    case 11: v->number=(unsigned)w->features->no_status; break;
    case 12:
        if(w->features->enter_size>21) { snprintf(v->text,sizeof(v->text),"Custom mapping from CLI (%u bytes)",(unsigned)w->features->enter_size); v->flags|=PS_CONTROL_LOCKED; }
        else for(i=0;i<w->features->enter_size;++i) snprintf(v->text+i*3,sizeof(v->text)-i*3,"%02X%s",w->features->enter[i],i+1<w->features->enter_size?" ":"");
        break;
    default:return 0;
    }
    return 1;
}
static int write_setting(void *ctx,unsigned id,const ps_control_value *v,ps_u32 revision) {
    workspace *w=(workspace *)ctx;
    if(revision!=w->revision || (id<8 && (w->port || w->wanted))) return 0;
    switch(id) {
    case 0: { char *end; unsigned long n=strtoul(v->text,&end,10); if(!*v->text || *end || n<1 || n>4000000) return 0; w->config.baud=(unsigned)n; break; }
    case 1: if(v->number<5 || v->number>8) return 0; w->config.data_bits=v->number; break;
    case 2: if(v->number>4) return 0; w->config.parity="NEOMS"[v->number]; break;
    case 3: if(v->number>2) return 0; w->config.stop_bits=v->number==2?15:v->number+1; break;
    case 4: w->config.rts_cts=!!v->number; break;
    case 5: if(v->number>3) return 0; w->config.dtr=(suart_dtr)v->number; break;
    case 6: if(v->number>60000) return 0; w->config.boot_wait_ms=v->number; break;
    case 7: if(v->number>60000 || (v->number && v->number<10)) return 0; w->reconnect=v->number; break;
    case 8: w->features->echo=!!v->number; break;
    case 9: if(w->features->line_size) return 0; w->features->line_buffer=!!v->number; break;
    case 10: if(v->number>2) return 0; w->features->display=(int)v->number; break;
    case 11: w->features->no_status=!!v->number; break;
    case 12: { unsigned char bytes[256]; size_t n=0; if(*v->text && suart_hex(v->text,bytes,sizeof(bytes),&n)) return 0; memcpy(w->features->enter,bytes,n); w->features->enter_size=n; break; }
    default:return 0;
    }
    ++w->revision; return 1;
}
static void disconnect(workspace *w) {
    suart_close(w->port); w->port=NULL; w->features->line_size=0; w->wanted=0; w->raw=0; ++w->revision;
}
static int send_bytes(workspace *w,const unsigned char *data,size_t size) {
    uint64_t end=suart_millis()+w->write_timeout; size_t pos=0;
    if(!w->port) { w->error=1; status(w,"Connect a device first"); return -1; }
    while(pos<size) {
        size_t n=0; unsigned left; int rc; uint64_t now=suart_millis();
        if(now>=end) break;
        left=(unsigned)(end-now); rc=suart_write(w->port,data+pos,size-pos,&n,left);
        pos+=n; w->tx+=n; if(rc) break;
    }
    if(pos==size) return 0;
    disconnect(w); w->error=1; status(w,"Write incomplete; delivery uncertain. Connection closed; bytes not replayed."); return -1;
}
static void open_device(workspace *w) {
    suart_selector selector=w->selector; suart_device d; unsigned long error=0; int rc; const char *old_instance=w->features->instance_id;
    int old_auto=w->features->auto_connect,old_pinned=w->features->pinned;
    if(w->pinned) {
        w->features->auto_connect=0; w->features->pinned=0;
        if(w->serial[0]) { selector.path=NULL; selector.serial=w->serial; }
        else if(w->instance[0]) { selector.path=NULL; w->features->instance_id=w->instance; }
        else selector.path=w->identity.device.path;
        selector.vid=w->identity.device.vid; selector.pid=w->identity.device.pid;
    }
    rc=sc_open(w->features,&selector,&w->config,&d,&w->port,&error); w->features->instance_id=old_instance;
    if(w->pinned) { w->features->auto_connect=old_auto; w->features->pinned=old_pinned; }
    if(rc) {
        w->error=1; status(w,"Open failed: status %d, native %lu%s",rc,error,w->reconnect?"; waiting for selected device":"");
        if(!w->reconnect || rc==SUART_AMBIGUOUS || rc==SUART_INVALID || rc==SUART_UNSUPPORTED) w->wanted=0;
        w->retry=suart_millis()+w->reconnect; return;
    }
    { suart_selector_ex exact; suart_device_info info; memset(&exact,0,sizeof(exact)); exact.device.path=d.path;
      memset(&info,0,sizeof(info)); if(suart_resolve_ex(&exact,&info)) info.device=d; w->identity=info; }
    if(!w->pinned) { snprintf(w->serial,sizeof(w->serial),"%s",w->identity.device.serial); snprintf(w->instance,sizeof(w->instance),"%s",w->identity.instance_id); w->pinned=1; }
    w->error=0; ++w->revision; w->features->osc_state=0; w->features->osc_size=0;
    { char path[128]; safe_text(path,sizeof(path),d.path); status(w,"Connected %s | %u baud | Ctrl+] m menu",path,w->config.baud); }
}
static void form_done(ps_session *s,void *ctx,int accepted) {
    workspace *w=(workspace *)ctx; (void)s;
    if(accepted) { w->action=SEND_HEX; snprintf(w->argument,sizeof(w->argument),"%s",w->hex); }
}
static const char *validate_hex(void *ctx,unsigned field,const char *text) {
    unsigned char bytes[256]; size_t n; (void)ctx; (void)field;
    return suart_hex(text,bytes,(PS_VALUE-1)/2,&n) || !n?"Enter 1..63 bytes as hex pairs (127 characters max)":NULL;
}
static void run_action(workspace *w) {
    unsigned a=w->action; w->action=0; w->error=0;
    if(a==MENU) main_menu(w);
    else if(a==PORTS) ports(w);
    else if(a==SELECT_PORT && w->index<w->count) {
        if(w->port || w->wanted) { w->error=1; status(w,"Disconnect before choosing another device"); }
        else { w->identity=w->devices[w->index]; snprintf(w->path,sizeof(w->path),"%s",w->identity.device.path); w->selector.path=w->path; w->pinned=0; w->features->pinned=0; status(w,"Device selected; choose Connect"); main_menu(w); }
    } else if(a==SETTINGS) ps_panel_open(w->shell,0);
    else if(a==CONNECT) {
        if(w->port || w->wanted) { disconnect(w); status(w,"Disconnected"); }
        else {
            if(w->argument[0]) {
                size_t n=strlen(w->argument);
                if(n>=sizeof(w->path)) { w->error=1; status(w,"Port path is too long"); w->argument[0]=0; return; }
                memcpy(w->path,w->argument,n+1); w->selector.path=w->path;
            }
            w->pinned=0; w->features->pinned=0; w->wanted=1; w->retry=0; open_device(w);
        }
        main_menu(w);
    } else if(a==TERMINAL) {
        if(!w->port) { w->error=1; status(w,"Connect a device first"); }
        else { ps_close(w->shell); w->raw=1; w->prefix=0; w->features->output_silent=0; fputs("\r\nSerial terminal | Ctrl+] m menu | Ctrl+] q quit | Ctrl+] ] literal prefix\r\n",stdout); }
    } else if(a==QUIT) w->quit=1;
    else if(a==SEND_HEX) {
        if(w->argument[0]) {
            unsigned char bytes[256]; size_t n;
            if(suart_hex(w->argument,bytes,sizeof(bytes),&n) || !n) { w->error=1; status(w,"Invalid hex; enter 1..256 bytes"); }
            else if(!send_bytes(w,bytes,n)) status(w,"Sent %u bytes",(unsigned)n);
        } else {
            w->hex[0]=0; w->form_field.label="Bytes (hex)"; w->form_field.value=w->hex; w->form_field.capacity=sizeof(w->hex); w->form_field.minimum=1;
            w->form.fields=&w->form_field; w->form.count=1; w->form.context=w; w->form.done=form_done; w->form.validate=validate_hex;
            ps_form_set_title(w->shell,"Send binary bytes"); ps_form_open(w->shell,&w->form);
        }
    } else if(a>=BREAK_LINE && a<=RTS_HIGH) {
        int rc=SUART_INVALID;
        if(w->port) rc=a==BREAK_LINE?suart_send_break(w->port,250):a<=DTR_HIGH?suart_set_dtr(w->port,a==DTR_HIGH):suart_set_rts(w->port,a==RTS_HIGH);
        w->error=rc!=0; status(w,"Modem action status %d",rc);
    } else {
        w->report_size=0; w->report[0]=0;
        if(a==DETAILS) {
            suart_device_info *i=&w->identity; suart_config actual; char board[512];
            fact(w,"Port",i->device.path); fact(w,"Friendly name",i->device.name); fact(w,"Product",i->product); fact(w,"Manufacturer",i->manufacturer);
            report(w,"USB: %s | VID:PID %04X:%04X\n",i->is_usb?"Yes":"No / unknown",i->device.vid,i->device.pid);
            fact(w,"Serial",i->device.serial); fact(w,"Instance",i->instance_id); fact(w,"Location",i->location);
            sc_board(w->features,&i->device,board,sizeof(board)); fact(w,"Board catalog",board); fact(w,"Source",suart_discovery_source());
            if(i->problem_known) report(w,"Device-manager problem: %u\n",i->problem_code);
            if(w->port && !suart_get_config(w->port,&actual)) report(w,"Driver: %u baud / %u%c%u / RTSCTS %s\n",actual.baud,actual.data_bits,actual.parity,actual.stop_bits,actual.rts_cts?"on":"off");
            report(w,"Session RX %llu bytes / TX %llu bytes\n",(unsigned long long)w->rx,(unsigned long long)w->tx); open_report(w,"Device details");
        } else if(a==SIGNALS) {
            suart_modem m; unsigned j; static const char *const names[]={"CTS","DSR","DCD","RI","DTR","RTS"};
            memset(&m,0,sizeof(m)); if(w->port) suart_get_modem(w->port,&m);
            for(j=0;j<6;++j) report(w,"%s: %s\n",names[j],!(m.known&(1u<<j))?"Unknown":m.asserted&(1u<<j)?"On":"Off");
            report(w,"\nDriver readback snapshot; not measured pin voltage.\n"); open_report(w,"Modem signals");
        } else if(a==HISTORY || a==HEX_HISTORY) {
            size_t n=w->history_size,start=(w->history_head+HISTORY_SIZE-n)%HISTORY_SIZE,j;
            report(w,"RX total %llu | retained %u bytes | snapshot\n\n",(unsigned long long)w->rx,(unsigned)n);
            if(a==HEX_HISTORY && n>1024) { start=(start+n-1024)%HISTORY_SIZE; n=1024; report(w,"Latest 1024 bytes in hex\n"); }
            for(j=0;j<n;++j) {
                unsigned c=w->history[(start+j)%HISTORY_SIZE];
                if(a==HEX_HISTORY) { if(j%16==0) report(w,"%04X  ",(unsigned)j); report(w,"%02X%s",c,j%16==15?"\n":" "); }
                else if(c==10 || c==9 || (c>=32 && c<127)) report(w,"%c",c);
                else report(w,"[%02X]",c);
            }
            open_report(w,a==HISTORY?"Receive history | control bytes escaped":"Hex receive history");
        } else if(a==HELP) {
            report(w,"SerialUART %s\n\n",SUART_VERSION);
            report(w,"Ports: / filters the list; Enter selects without opening.\nSettings: Enter edits a cell; Esc cancels; Tab changes page.\nEscape closes a view to the command prompt. Type menu to return.\nTerminal: Ctrl+] m menu, Ctrl+] q quit, Ctrl+] ] literal prefix.\nCtrl+C/D are passed to the board in Terminal.\n\nCommands: menu, ports, settings, connect [PATH], disconnect,\nterminal, details, signals, history, hex, sendhex HEX, help, quit.\n\nMenus keep receiving bytes into an 8192-byte ring; the oldest bytes\nare replaced. History is a snapshot; reopen to refresh. Hex shows\nthe latest 1024 bytes. --log FILE captures the full stream.\n\nReconnect follows the selected serial/instance/path identity.\nDisconnect cancels retries. No uncertain writes are replayed.\nNo shell commands or external programs are executed.\n"); open_report(w,"Console guide");
        }
    }
    w->argument[0]=0;
}
static int command(ps_session *s,int argc,const char *const *argv,void *ctx) {
    binding *b=(binding *)ctx; workspace *w=b->w; (void)s;
    w->action=b->action; w->argument[0]=0;
    if(argc>1) {
        int j; size_t used=0;
        for(j=1;j<argc;++j) { size_t n=strlen(argv[j]); if(used+n+(j>1)>=sizeof(w->argument)) { w->action=0; return 2; } if(j>1) w->argument[used++]=' '; memcpy(w->argument+used,argv[j],n+1); used+=n; }
    }
    if(!strcmp(argv[0],"disconnect")) { disconnect(w); w->action=MENU; snprintf(w->notice,sizeof(w->notice),"Disconnected"); }
    return 0;
}
int sc_tui_run(sc_session *features,const suart_selector *selector,const suart_config *config,unsigned reconnect,unsigned timeout,
               int (*input)(unsigned char *,size_t,size_t *),volatile sig_atomic_t *stopping) {
    workspace *w; ps_config cfg; unsigned j; int result=0;
    static const char *const names[]={"menu","ports","settings","connect","disconnect","terminal","details","history","hex","sendhex","signals","help","quit"};
    static const unsigned actions[]={MENU,PORTS,SETTINGS,CONNECT,CONNECT,TERMINAL,DETAILS,HISTORY,HEX_HISTORY,SEND_HEX,SIGNALS,HELP,QUIT};
#ifdef _WIN32
    DWORD output_mode; HANDLE output=GetStdHandle(STD_OUTPUT_HANDLE);
    if(!GetConsoleMode(output,&output_mode) || !SetConsoleMode(output,output_mode|ENABLE_VIRTUAL_TERMINAL_PROCESSING)) { fputs("--tui requires a VT-capable console\n",stderr); return 2; }
#else
    if(!isatty(1)) { fputs("--tui requires a terminal on stdout\n",stderr); return 2; }
#endif
    w=(workspace *)calloc(1,sizeof(*w)); if(!w) { result=1; goto cleanup; }
    w->memory=calloc(1,ps_sizeof_session()); if(!w->memory) { result=1; goto cleanup; }
    w->features=features; w->selector=*selector; w->config=*config; w->reconnect=reconnect; w->write_timeout=timeout;
    dimensions(&w->cols,&w->rows); cfg.write=emit; cfg.context=w; cfg.columns=w->cols; cfg.rows=w->rows;
    cfg.flags=PS_INTERACTIVE|PS_COLOR|PS_UNICODE|PS_NO_OSC|PS_NO_BUILTINS;
    if(getenv("NO_COLOR")) cfg.flags&=~PS_COLOR;
    if(getenv("SUART_ASCII")) cfg.flags&=~PS_UNICODE;
    w->shell=ps_init(w->memory,ps_sizeof_session(),&cfg); if(!w->shell) { result=1; goto cleanup; }
    for(j=0;j<sizeof(actions)/sizeof(actions[0]);++j) {
        binding *b=&w->actions[18+j]; b->w=w; b->action=actions[j];
        w->commands[j].name=names[j]; w->commands[j].usage=actions[j]==CONNECT?"[PATH]":actions[j]==SEND_HEX?"HEX":"";
        w->commands[j].category="Serial console"; w->commands[j].run=command; w->commands[j].context=b; ps_register(w->shell,&w->commands[j]);
    }
    ps_set_prompt(w->shell,"serial> ",PS_STYLE_PROMPT); ps_set_application_title(w->shell,"SerialUART");
    ps_menu_set_description(w->shell,description,w); ps_menu_set_notice(w->shell,notice,w);
    w->panel.title="Serial settings"; w->panel.status="Connection settings apply on the next explicit Connect";
    w->panel.pages=pages; w->panel.count=2; w->panel.read=read_setting; w->panel.write=write_setting; w->panel.context=w; ps_panel_attach(w->shell,&w->panel);
    ps_start(w->shell); snprintf(w->notice,sizeof(w->notice),"Disconnected | choose a port or use connect PATH"); main_menu(w);
    while(!w->quit && !*stopping && !w->output_error && !ps_exited(w->shell)) {
        unsigned char bytes[4096]; size_t n=0,i; unsigned cols,rows; int rc;
        features->output_silent=!w->raw;
        if(w->wanted && !w->port && suart_millis()>=w->retry) open_device(w);
        if(w->port) {
            rc=suart_read(w->port,bytes,sizeof(bytes),&n,0);
            if(rc) {
                int want=w->wanted,was_raw=w->raw; disconnect(w); w->wanted=want && w->reconnect; w->error=1; w->retry=suart_millis()+w->reconnect;
                if(was_raw) ps_start(w->shell);
                status(w,"Device disconnected%s",w->wanted?"; waiting for pinned identity":"");
                main_menu(w);
            } else if(n) {
                for(i=0;i<n;++i) { w->history[w->history_head]=bytes[i]; w->history_head=(w->history_head+1)%HISTORY_SIZE; if(w->history_size<HISTORY_SIZE) ++w->history_size; }
                w->rx+=n; if(sc_output(features,bytes,n,0)) { result=1; break; }
            }
        }
        rc=input(bytes,sizeof(bytes),&n); if(rc<0) { result=1; break; } if(rc==2) break;
        for(i=0;i<n && !w->quit;++i) {
            unsigned char ch=bytes[i];
            if(w->raw) {
                if(w->prefix) {
                    w->prefix=0;
                    if(ch=='m') { w->raw=0; features->line_size=0; features->output_silent=1; ps_start(w->shell); main_menu(w); continue; }
                    if(ch=='q') { w->quit=1; continue; }
                    if(ch!=']') continue;
                    ch=29;
                } else if(ch==29) { w->prefix=1; continue; }
                if(w->port) { unsigned char out[4352]; size_t count=0; if(sc_input(features,ch,out,sizeof(out),&count) || (count && send_bytes(w,out,count))) { result=1; w->quit=1; } }
            } else { ps_feed(w->shell,&ch,1); if(w->action) run_action(w); }
        }
        if(!w->raw) { dimensions(&cols,&rows); if(cols!=w->cols || rows!=w->rows) { w->cols=cols; w->rows=rows; ps_resize(w->shell,cols,rows); } ps_tick(w->shell,(ps_u32)suart_millis()); }
        suart_sleep(5);
    }
    if(w->port && suart_wait_tx(w->port,timeout)!=SUART_OK) result=1;
    disconnect(w); ps_close(w->shell); if(w->output_error) result=1;
cleanup:
    features->output_silent=1; if(w) { free(w->memory); free(w); }
#ifdef _WIN32
    SetConsoleMode(output,output_mode);
#endif
    return result;
}
