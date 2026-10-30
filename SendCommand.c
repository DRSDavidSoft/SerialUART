#define _POSIX_C_SOURCE 200809L
#include "SerialUART.h"
#include "protocol.h"
#include "network.h"
#include "session.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <io.h>
#include <fcntl.h>
#else
#include <unistd.h>
#include <termios.h>
#include <poll.h>
#endif
static volatile sig_atomic_t stopping;
static void stop(int sig) { (void)sig; stopping=1; }
static int terminal;
static unsigned write_timeout=2000;
static sc_session features;
#ifdef _WIN32
static DWORD console_mode;
static HANDLE input_handle;
#else
static struct termios console_mode;
#endif
static void restore(void) {
    if(!terminal) return;
#ifdef _WIN32
    SetConsoleMode(input_handle,console_mode);
#else
    tcsetattr(0,TCSANOW,&console_mode);
#endif
    terminal=0;
}
static int input_init(int raw) {
#ifdef _WIN32
    input_handle=GetStdHandle(STD_INPUT_HANDLE);
    _setmode(_fileno(stdin),_O_BINARY); _setmode(_fileno(stdout),_O_BINARY);
    if(raw) {
        if(!GetConsoleMode(input_handle,&console_mode)) return -1;
        if(!SetConsoleMode(input_handle,console_mode & ~(ENABLE_LINE_INPUT|ENABLE_ECHO_INPUT|ENABLE_PROCESSED_INPUT))) return -1;
        terminal=1;
    }
#else
    if(raw) {
        struct termios t;
        if(tcgetattr(0,&console_mode)) return -1;
        t=console_mode; t.c_lflag &= ~(ICANON|ECHO|ISIG|IEXTEN);
        t.c_iflag &= ~(IXON|ICRNL|INLCR|IGNCR|ISTRIP); t.c_cc[VMIN]=1; t.c_cc[VTIME]=0;
        if(tcsetattr(0,TCSANOW,&t)) return -1;
        terminal=1;
    }
#endif
    return 0;
}
/* Nonblocking input, -1=error, 0=idle, 1=data, 2=EOF. */
static int input_read(unsigned char *b,size_t cap,size_t *n) {
    *n=0;
#ifdef _WIN32
    DWORD mode,got;
    if(GetConsoleMode(input_handle,&mode)) {
        INPUT_RECORD record; DWORD events;
        static unsigned lead,repeats; static unsigned char pending[8]; static size_t pending_size;
        while(cap-*n>=8) {
            if(repeats) {
                memcpy(b+*n,pending,pending_size); *n+=pending_size; --repeats;
                if(!features.bulk) break;
                continue;
            }
            if(!GetNumberOfConsoleInputEvents(input_handle,&events)) return -1;
            if(!events) break;
            if(!ReadConsoleInputW(input_handle,&record,1,&got)) return -1;
            if(record.EventType==KEY_EVENT && record.Event.KeyEvent.bKeyDown) {
                WCHAR c=record.Event.KeyEvent.uChar.UnicodeChar;
                if(c) {
                    pending_size=sc_utf16(&lead,c,pending);
                } else {
                    const char *seq=sc_key_sequence(record.Event.KeyEvent.wVirtualKeyCode);
                    pending_size=0;
                    if(seq) { pending_size=strlen(seq); memcpy(pending,seq,pending_size); }
                    else if(record.Event.KeyEvent.wVirtualKeyCode==VK_SPACE &&
                            (record.Event.KeyEvent.dwControlKeyState&(LEFT_CTRL_PRESSED|RIGHT_CTRL_PRESSED))) {
                        pending[0]=0; pending_size=1;
                    }
                }
                if(pending_size) repeats=record.Event.KeyEvent.wRepeatCount?record.Event.KeyEvent.wRepeatCount:1;
            }
        }
        return *n?1:0;
    }
    if(GetFileType(input_handle)==FILE_TYPE_PIPE) {
        DWORD available=0;
        if(!PeekNamedPipe(input_handle,NULL,0,NULL,&available,NULL)) return GetLastError()==ERROR_BROKEN_PIPE?2:-1;
        if(!available) return 0;
        if(cap>available) cap=available;
    }
    if(!ReadFile(input_handle,b,(DWORD)cap,&got,NULL)) return GetLastError()==ERROR_BROKEN_PIPE?2:-1;
    *n=got; return got?1:2;
#else
    struct pollfd f; int rc; ssize_t got;
    if(!features.bulk) cap=1;
    f.fd=0; f.events=POLLIN; f.revents=0; rc=poll(&f,1,0);
    if(rc<0) return errno==EINTR?0:-1;
    if(!rc) return 0;
    got=read(0,b,cap); if(got<0) return errno==EINTR?0:-1;
    *n=(size_t)got; return got?1:2;
#endif
}
static int offline_terminal(int *escape) {
    unsigned char b[256]; size_t n=0,i; int result=input_read(b,sizeof(b),&n);
    if(result<0) return -1;
    if(result==2) { stopping=1; return 0; }
    for(i=0;i<n;++i) {
        if(features.exit_key && b[i]==features.exit_key) { stopping=1; break; }
        if(*escape) {
            *escape=0;
            if(b[i]=='q') stopping=1;
            else if(b[i]=='?') fputs("\r\nWaiting for serial device; Ctrl+] q quits. Input while offline is discarded.\r\n",stderr);
        } else if(b[i]==29) *escape=1;
    }
    return 0;
}
static void help(void) {
    puts("SerialUART " SUART_VERSION " | portable serial library and terminal\n"
         "Usage: SendCommand [selection] [configuration] [mode]\n\n"
         "Select a device (filters combine; ambiguous matches are rejected):\n"
         "  --list                    List paths, friendly names, VID:PID, serial\n"
         "  -p, --port PATH            COM12, /dev/ttyACM0, /dev/cu.usbmodem...\n"
         "  --name NAME               Exact friendly name (quote spaces)\n"
         "  --vid HEX --pid HEX        USB vendor and product IDs\n"
         "  --serial TEXT             USB serial number\n\n"
         "Configure (default 57600 baud, 8N1, no flow control, keep DTR):\n"
         "  -b, --baud RATE            Baud rate\n"
         "  --data-bits 5..8 --parity N|E|O|M|S --stop-bits 1|1.5|2\n"
         "  --rts-cts                 Hardware flow control\n"
         "  --dtr keep|off|on|reset    Reset deasserts 100ms then asserts DTR\n"
         "  --boot-wait MS            Wait after every successful open (0..60000)\n\n"
         "Modes (default: binary stdin -> serial -> stdout):\n"
         "  -s, --send TEXT            Send text, receive until idle timeout\n"
         "  --sendline TEXT            Send text followed by LF\n"
         "  --hex HEX                 Send binary, e.g. \"00 41 FF 0D 0A\"\n"
         "  --until HEX               Stop at a reply byte (e.g. 0A, includes it);\n"
         "                            --timeout is then a total reply deadline\n"
         "  --write-timeout MS        Write/TX queue deadline (1..60000, default 2000)\n"
         "  --discard-input           Explicitly discard RX after opening\n"
         "  --show-config             Report driver settings and modem bits to stderr\n"
         "  --terminal                Raw terminal; Ctrl+] then ? for shortcuts\n"
         "  --receive                 Receive only; stdin is ignored\n"
         "  --timeout MS              Idle timeout after send/EOF (default 1000)\n"
         "  --reconnect MS            Retry missing/lost port (10..60000)\n"
         "  --listen tcp|ws|socketio HOST PORT   Serve one network peer\n"
         "  --connect tcp HOST PORT    Connect serial bridge to a TCP server\n"
         "  --origin ORIGIN            Exact browser Origin allowed for WS\n"
         "  -q, --quiet               Suppress connection status (stderr)\n"
         "  -h, --help | --version\n\n"
         "Examples:\n"
         "  SendCommand --list\n"
         "  SendCommand -p COM12 -b 115200 --dtr reset --boot-wait 2000 -s STATUS\n"
         "  SendCommand --vid 2341 --pid 0043 --terminal --reconnect 1000\n"
         "  SendCommand -p /dev/ttyUSB0 --listen ws 127.0.0.1 8765\n\n"
         "Exit: 0 normal, 1 I/O failure, 2 usage/selection, 124 no response.\n"
         "Network modes have no authentication/TLS; use a trusted local tunnel.\n"
         "DTR off/keep cannot prevent all driver-induced Arduino reset pulses.\n"
         "Mark/space parity requires driver support; 1.5 stop bits requires Windows/5 data bits.");
}
static unsigned number(const char *s,unsigned max,int base,int *ok) {
    char *end; unsigned long n;
    if(!*s || *s=='-' || *s=='+' || *s==' ') { *ok=0; return 0; }
    errno=0; n=strtoul(s,&end,base);
    if(errno || *end || n>max) { *ok=0; return 0; }
    return (unsigned)n;
}
static int write_all(suart_port *p,const unsigned char *b,size_t n) {
    uint64_t end=suart_millis()+write_timeout;
    while(n && !stopping) {
        size_t sent=0; uint64_t now=suart_millis();
        if(now>=end) return -1;
        if(suart_write(p,b,n,&sent,(unsigned)(end-now>100?100:end-now))!=SUART_OK) return -1;
        b+=sent; n-=sent;
        if(n && suart_millis()>=end) return -1;
    }
    return n?-1:0;
}
static int write_terminal(suart_port *p,const unsigned char *b,size_t n) {
    if(!write_all(p,b,n)) return 0;
    fputs("Write failed; delivery uncertain, terminal bytes not replayed\n",stderr); return -1;
}
static void show_config(suart_port *p) {
    suart_config c; suart_modem m;
    if(!suart_get_config(p,&c)) fprintf(stderr,"Driver: %u baud, %u%c%s, RTS/CTS %s\n",c.baud,c.data_bits,c.parity,c.stop_bits==15?"1.5":c.stop_bits==2?"2":"1",c.rts_cts?"on":"off");
    else fprintf(stderr,"Driver configuration unavailable (native %lu)\n",suart_native_error(p));
    if(!suart_get_modem(p,&m)) fprintf(stderr,"Modem logical bits: known=0x%02X asserted=0x%02X (CTS=01 DSR=02 DCD=04 RI=08 DTR=10 RTS=20)\n",m.known,m.asserted);
    else fprintf(stderr,"Modem status unavailable (native %lu)\n",suart_native_error(p));
}
int main(int argc,char **argv) {
    suart_config c; suart_selector sel={0}; suart_port *port=NULL; suart_device device;
    suart_net *net=NULL; const char *send_text=NULL,*hex=NULL,*mode=NULL,*host=NULL,*origin=NULL;
    unsigned timeout=1000,reconnect=0,net_port=0; int raw=0,receive=0,quiet=0,client=0,ok=1,i,rc=0;
    int eof=0,escape=0,received=0,sent_command=0; uint64_t last=0,retry=0;
    int newline=0,until=-1,discard_input=0,diagnostics=0,tx_waited=0;
    uint64_t reply_end=0,tx_end=0;
    unsigned char buffer[4096],command[65536]; size_t command_len=0;
    if(sc_windows_arguments(&argc,&argv)) { fputs("Cannot decode command line\n",stderr); return 1; }
    sc_init(&features);
    if(sc_arguments(&argc,&argv)) return 2;
    suart_config_init(&c);
    for(i=1;i<argc;++i) {
        const char *a=argv[i],*v=NULL;
        int feature=sc_option(&features,a,NULL);
        if(feature==1) continue;
        if(!strcmp(a,"--help") || !strcmp(a,"-h")) { help(); sc_help(); return 0; }
        if(!strcmp(a,"--version")) { puts(SUART_VERSION); return 0; }
        if(!strcmp(a,"--terminal")) { raw=1; continue; }
        if(!strcmp(a,"--receive")) { receive=1; continue; }
        if(!strcmp(a,"--rts-cts")) { c.rts_cts=1; continue; }
        if(!strcmp(a,"--discard-input")) { discard_input=1; continue; }
        if(!strcmp(a,"--show-config")) { diagnostics=1; continue; }
        if(!strcmp(a,"--quiet") || !strcmp(a,"-q")) { quiet=1; continue; }
        if(i+1>=argc) { fprintf(stderr,"Missing value for %s\n",a); return 2; }
        v=argv[++i];
        if(!strcmp(a,"--port") || !strcmp(a,"-p")) sel.path=v;
        else if(!strcmp(a,"--name")) sel.name=v;
        else if(!strcmp(a,"--serial")) sel.serial=v;
        else if(!strcmp(a,"--vid")) sel.vid=number(v,65535,16,&ok);
        else if(!strcmp(a,"--pid")) sel.pid=number(v,65535,16,&ok);
        else if(!strcmp(a,"--baud") || !strcmp(a,"-b")) { c.baud=number(v,4000000,10,&ok); if(!c.baud) ok=0; }
        else if(!strcmp(a,"--data-bits")) { c.data_bits=number(v,8,10,&ok); if(c.data_bits<5) ok=0; }
        else if(!strcmp(a,"--stop-bits")) { if(!strcmp(v,"1.5")) c.stop_bits=SUART_STOP_ONE_POINT_FIVE; else { c.stop_bits=number(v,2,10,&ok); if(!c.stop_bits) ok=0; } }
        else if(!strcmp(a,"--parity")) { if(strlen(v)!=1 || !strchr("NEOMS",*v)) ok=0; else c.parity=*v; }
        else if(!strcmp(a,"--boot-wait")) c.boot_wait_ms=number(v,60000,10,&ok);
        else if(!strcmp(a,"--timeout")) timeout=number(v,60000,10,&ok);
        else if(!strcmp(a,"--write-timeout")) { write_timeout=number(v,60000,10,&ok); if(!write_timeout) ok=0; }
        else if(!strcmp(a,"--until")) until=(int)number(v,255,16,&ok);
        else if(!strcmp(a,"--reconnect")) { reconnect=number(v,60000,10,&ok); if(reconnect<10) ok=0; }
        else if(!strcmp(a,"--dtr")) {
            if(!strcmp(v,"keep")) c.dtr=SUART_DTR_KEEP;
            else if(!strcmp(v,"off")) c.dtr=SUART_DTR_OFF;
            else if(!strcmp(v,"on")) c.dtr=SUART_DTR_ON;
            else if(!strcmp(v,"reset")) c.dtr=SUART_DTR_RESET;
            else ok=0;
        }
        else if(!strcmp(a,"--send") || !strcmp(a,"-s")) { if(send_text || hex) ok=0; send_text=v; }
        else if(!strcmp(a,"--sendline")) { if(send_text || hex) ok=0; send_text=v; newline=1; }
        else if(!strcmp(a,"--hex")) { if(send_text || hex) ok=0; hex=v; }
        else if(!strcmp(a,"--origin")) origin=v;
        else if(!strcmp(a,"--listen") || !strcmp(a,"--connect")) {
            if(mode || i+2>=argc) { ok=0; break; }
            mode=v; host=argv[++i]; net_port=number(argv[++i],65535,10,&ok);
            client=!strcmp(a,"--connect"); if(!net_port) ok=0;
        }
        else {
            feature=sc_option(&features,a,v);
            if(feature==0) { fprintf(stderr,"Unknown option: %s (see --help)\n",a); return 2; }
            if(feature<0) ok=0;
            if(!strcmp(a,"--autoconnect")) {
                if(features.auto_connect && !reconnect) reconnect=1000;
                if(!features.auto_connect) reconnect=0;
            }
        }
        if(!ok) { fprintf(stderr,"Invalid value for %s: %s\n",a,v); return 2; }
    }
    if(features.list_kind) {
        if(features.list_kind<=2) return sc_list(&features)?1:0;
        if(features.list_kind==3) sc_boards(&features);
        if(features.list_kind==4) sc_filters(&features);
        if(features.list_kind==5) sc_settings(argc,argv);
        return 0;
    }
    if(!ok || (send_text && hex) || (raw+receive+(send_text!=NULL || hex!=NULL)+(mode!=NULL)>1) ||
       (origin && !mode) || (client && strcmp(mode,"tcp")) ||
       (until>=0 && (raw || mode || (!receive && !send_text && !hex)))) {
        fputs("Conflicting modes or invalid network arguments; see --help\n",stderr); return 2;
    }
    if((!raw && (features.echo || features.line_buffer || features.enter_size || features.exit_key || features.title || features.clear_screen)) ||
       (mode && (features.display || features.force_newline || features.no_status || features.log_display))) {
        fputs("Terminal controls require --terminal; display transforms require a stdout mode\n",stderr); return 2;
    }
    if(argc==1) { help(); return 2; }
    if(send_text) { command_len=strlen(send_text); if(command_len>sizeof(command)-(size_t)newline) return 2; memcpy(command,send_text,command_len); if(newline) command[command_len++]='\n'; }
    if(hex && suart_hex(hex,command,sizeof(command),&command_len)) { fputs("Invalid hex bytes\n",stderr); return 2; }
    signal(SIGINT,stop); signal(SIGTERM,stop);
#ifndef _WIN32
    signal(SIGPIPE,SIG_IGN);
#endif
    atexit(restore);
    if(input_init(raw)) { fputs("Cannot configure terminal\n",stderr); return 1; }
    setvbuf(stdout,NULL,_IONBF,0);
    if(sc_output_open(&features)) { rc=1; goto done; }
    if(mode) {
        net=suart_net_open(mode,host,net_port,client,origin);
        if(!net) { fputs("Cannot open network adapter\n",stderr); rc=1; goto done; }
        if(!quiet) fprintf(stderr,"%s %s %s:%u\n",client?"Connecting":"Listening",mode,host,net_port);
    }
    if(raw && !quiet) fputs("Ctrl+] then ?: help; q: quit; b: break; d: DTR; m: config; e: echo; c: clear; ]: literal Ctrl+]\n",stderr);
    while(!stopping) {
        size_t n=0; uint64_t now=suart_millis();
        if(!port) {
            int result; unsigned long open_error=0;
            if(raw) {
                if(offline_terminal(&escape)) { rc=1; break; }
                if(stopping) break;
            }
            if(now<retry) { suart_sleep(5); continue; }
            result=sc_open(&features,&sel,&c,&device,&port,&open_error);
            if(result==SUART_AMBIGUOUS || result==SUART_INVALID) {
                fputs("Selection is invalid, blocked or ambiguous; check filters and add --serial or --port\n",stderr); rc=2; break;
            }
            if(result) {
                if(!reconnect || result==SUART_INVALID || result==SUART_UNSUPPORTED) {
                    fprintf(stderr,"Cannot open device (status %d, native %lu); check path, permissions, ownership and baud\n",result,open_error); rc=1; break;
                }
                retry=now+reconnect; continue;
            }
            if(!quiet) fprintf(stderr,"Connected %s @ %u baud\n",device.path,c.baud);
            features.osc_state=0; features.osc_size=0;
            if(raw) sc_console(&features);
            if(discard_input && suart_discard(port,SUART_RX)) { fputs("Cannot discard input\n",stderr); rc=1; break; }
            if(diagnostics) show_config(port);
            last=suart_millis();
            reply_end=last+timeout;
            if((send_text || hex) && !sent_command) {
                sent_command=1;
                if(write_all(port,command,command_len)) { fputs("Write failed; delivery uncertain, command not replayed\n",stderr); rc=1; break; }
                eof=1; last=suart_millis(); reply_end=last+timeout;
            }
        }
        if(suart_read(port,buffer,until>=0?1:sizeof(buffer),&n,5)!=SUART_OK) {
            if(!quiet) fprintf(stderr,"Serial read failed (native %lu)\n",suart_native_error(port));
            suart_close(port); port=NULL;
            features.line_size=0;
            if(!reconnect || eof) { rc=1; break; }
            retry=suart_millis()+reconnect; continue;
        }
        if(n) {
            received=1; last=suart_millis();
            if(net) {
                if(features.log && !features.log_display && (fwrite(buffer,1,n,features.log)!=n || fflush(features.log))) { rc=1; break; }
                if(suart_net_write(net,buffer,n) && client) { rc=1; break; }
            } else if(sc_output(&features,buffer,n,0)) { fputs("Output or log write failed\n",stderr); rc=1; break; }
            if(until>=0 && buffer[n-1]==(unsigned char)until) break;
        }
        if(eof) {
            if(until>=0 && suart_millis()>=reply_end) { rc=124; break; }
            if(!tx_waited) {
                int result;
                if(!tx_end) tx_end=suart_millis()+write_timeout;
                result=suart_wait_tx(port,0);
                if(result==SUART_TIMEOUT && suart_millis()<tx_end) continue;
                if(result!=SUART_OK) { fputs("TX queue did not empty; delivery uncertain\n",stderr); rc=1; break; }
                tx_waited=1; last=suart_millis();
            }
            if(suart_millis()-last>=timeout) { rc=until>=0 || ((send_text || hex) && !received)?124:0; break; }
            continue;
        }
        if(receive) {
            if(until>=0 && suart_millis()>=reply_end) { rc=124; break; }
            continue;
        }
        if(net) {
            int result=suart_net_read(net,buffer,sizeof(buffer),&n);
            if(result<0 && client) { rc=1; break; }
        } else {
            int result=input_read(buffer,features.bulk?sizeof(buffer):16,&n);
            if(result<0) { rc=1; break; }
            if(result==2) { eof=1; last=suart_millis(); continue; }
        }
        if(raw && n) {
            unsigned char outgoing[8192]; size_t j,k=0;
            for(j=0;j<n;++j) {
                unsigned ch=buffer[j];
                int data=0;
                if(features.exit_key && ch==(unsigned)features.exit_key) {
                    if(k && write_terminal(port,outgoing,k)) rc=1;
                    stopping=1; k=0; break;
                }
                if(escape) {
                    escape=0;
                    if(ch=='q') { stopping=1; k=0; break; }
                    if(ch=='?') fputs("\r\nCtrl+] q quit | b break | d DTR reset | m config | e echo | c clear | ] literal escape\r\n",stderr);
                    else if(ch=='m') show_config(port);
                    else if(ch=='e') features.echo=!features.echo;
                    else if(ch=='c') { int previous=features.clear_screen; features.clear_screen=1; sc_console(&features); features.clear_screen=previous; }
                    else if(ch=='b') { if(suart_send_break(port,250)) rc=1; }
                    else if(ch=='d') {
                        if(suart_set_dtr(port,0)) rc=1;
                        suart_sleep(100); if(suart_set_dtr(port,1)) rc=1;
                    } else if(ch==']') { ch=29; data=1; }
                } else if(ch==29) {
                    if(k && write_terminal(port,outgoing,k)) { rc=1; break; }
                    k=0; escape=1;
                }
                else data=1;
                if(data) {
                    size_t added=0;
                    if(sizeof(outgoing)-k<4352) {
                        if(write_terminal(port,outgoing,k)) { rc=1; break; }
                        k=0;
                    }
                    if(sc_input(&features,(unsigned char)ch,outgoing+k,sizeof(outgoing)-k,&added)) {
                        fputs("Terminal line overflow or echo/log failure; unsubmitted input discarded\n",stderr); rc=1; break;
                    }
                    k+=added;
                    if(features.line_buffer && added) {
                        if(write_terminal(port,outgoing,k)) { rc=1; break; }
                        k=0;
                    }
                }
            }
            if(k && !rc && write_terminal(port,outgoing,k)) rc=1;
            n=0;
        }
        if(rc) break;
        if(n && write_all(port,buffer,n)) {
            fputs("Write failed; delivery uncertain, bytes not replayed\n",stderr); rc=1; break;
        }
    }
done:
    if(port && !rc && suart_wait_tx(port,write_timeout)!=SUART_OK) {
        fputs("TX queue did not empty; delivery uncertain\n",stderr); rc=1;
    }
    if(sc_output_close(&features)) { fputs("Cannot finish output/log\n",stderr); rc=1; }
    restore(); suart_close(port); suart_net_close(net); return rc;
}
