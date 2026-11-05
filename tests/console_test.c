/* Drive the actual console model and PortableShell byte handlers without devices. */
#include "../cli/tui.c"
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"console line %d: %s\n",__LINE__,#x); return 1; } } while(0)
static char captured[1048576]; static size_t capture_size;
static void capture(void *ctx,const ps_byte *b,unsigned n) {
    (void)ctx;
    if(capture_size+n<sizeof(captured)) { memcpy(captured+capture_size,b,n); capture_size+=n; captured[capture_size]=0; }
}
int main(int argc,char **argv) {
    workspace *w=(workspace *)calloc(1,sizeof(*w)); sc_session *f=(sc_session *)calloc(1,sizeof(*f));
    ps_config cfg={capture,NULL,100,30,PS_INTERACTIVE|PS_COLOR|PS_UNICODE|PS_NO_OSC|PS_NO_BUILTINS};
    ps_control_value v; suart_selector_ex selector; suart_device_info device;
    CHECK(w && f); sc_init(f); w->features=f; suart_config_init(&w->config);
    w->memory=calloc(1,ps_sizeof_session()); CHECK(w->memory); w->shell=ps_init(w->memory,ps_sizeof_session(),&cfg); CHECK(w->shell);
    w->panel.title="Serial settings"; w->panel.status="Disconnected | changes apply on Connect";
    w->panel.pages=pages; w->panel.count=2; w->panel.read=read_setting; w->panel.write=write_setting; w->panel.context=w;
    CHECK(ps_panel_attach(w->shell,&w->panel)); ps_start(w->shell);
    memset(&v,0,sizeof(v)); strcpy(v.text,"115200"); CHECK(write_setting(w,0,&v,0)); CHECK(w->config.baud==115200);
    strcpy(v.text,"4000001"); CHECK(!write_setting(w,0,&v,1)); CHECK(w->config.baud==115200);
    strcpy(v.text,"9600"); CHECK(!write_setting(w,0,&v,0)); CHECK(w->config.baud==115200); /* stale edit */
    w->wanted=1; CHECK(!write_setting(w,0,&v,1)); CHECK(read_setting(w,0,&v) && (v.flags&PS_CONTROL_LOCKED)); w->wanted=0;
    memset(&v,0,sizeof(v)); strcpy(v.text,"00 FF 0D"); CHECK(write_setting(w,12,&v,1)); CHECK(f->enter_size==3 && f->enter[0]==0 && f->enter[1]==255);
    CHECK(validate_hex(w,0,"00 FF")==NULL); CHECK(validate_hex(w,0,"xy")!=NULL);
    { char pairs[129]; memset(pairs,'A',126); pairs[126]=0;
      CHECK(validate_hex(w,0,pairs)==NULL); pairs[126]='A'; pairs[127]='A'; pairs[128]=0;
      CHECK(validate_hex(w,0,pairs)!=NULL); }
    main_menu(w); CHECK(ps_menu_active(w->shell)); CHECK(strstr(captured,"Serial workspace"));
    CHECK(ps_menu_choose(w->shell,2)==0); CHECK(w->action==SETTINGS); run_action(w); CHECK(ps_panel_active(w->shell));
    CHECK(ps_panel_select(w->shell,0)); ps_key(w->shell,13); ps_key(w->shell,21);
    ps_feed(w->shell,"31250",5); ps_key(w->shell,13); CHECK(w->config.baud==31250); /* inline editor */
    ps_key(w->shell,27);
    memset(&selector,0,sizeof(selector)); memset(&device,0,sizeof(device)); strcpy(device.device.path,"COM18");
    strcpy(device.manufacturer,"QinHeng"); strcpy(device.device.name,"USB-SERIAL CH340"); strcpy(device.instance_id,"USB\\VID_1A86&PID_7523\\A"); device.device.vid=0x1a86; device.device.pid=0x7523;
    selector.match="qinheng"; CHECK(suart_device_matches(&selector,&device)); selector.device.pid=0x1234; CHECK(!suart_device_matches(&selector,&device)); selector.device.pid=0;
    selector.instance_id="wrong"; CHECK(!suart_device_matches(&selector,&device)); selector.instance_id=device.instance_id; CHECK(suart_device_matches(&selector,&device));
    CHECK(suart_resolve_ex(NULL,&device)==SUART_INVALID); CHECK(suart_enumerate_ex(NULL,NULL)==SUART_INVALID);
    /* Every retained byte remains visible even when all need [XX] escaping. */
    memset(w->history,0xFF,sizeof(w->history)); w->history_size=HISTORY_SIZE;
    w->action=HISTORY; run_action(w);
    CHECK(strlen(w->report)>HISTORY_SIZE*4 && strstr(w->report,"[FF]"));
    CHECK(strlen(ps_editor_text(w->shell))==strlen(w->report)); ps_key(w->shell,27);
    w->history_size=0;
    if(argc==3 && (!strcmp(argv[1],"--preview") || !strcmp(argv[1],"--preview-settings"))) {
        FILE *out; capture_size=0; ps_close(w->shell); ps_start(w->shell);
        w->config.baud=115200; w->identity=device; snprintf(w->notice,sizeof(w->notice),"Disconnected | USB-SERIAL CH340 (fixture) | 115200 baud");
        ps_menu_set_notice(w->shell,notice,w); ps_menu_set_description(w->shell,description,w); main_menu(w);
        if(!strcmp(argv[1],"--preview-settings")) { CHECK(ps_panel_open(w->shell,0)); ps_tick(w->shell,100); }
        out=fopen(argv[2],"wb"); CHECK(out); fwrite(captured,1,capture_size,out); fclose(out);
    }
    ps_close(w->shell); free(w->memory); free(w); free(f); return 0;
}
