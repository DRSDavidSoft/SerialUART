#include "json.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"JSON check failed at %d\n",__LINE__);return 1; } } while(0)
int main(void) {
    j_doc d;char output[256],decoded[64];j_out o={output,0,sizeof(output),0};
    const char *sample="{\"unicode\":\"\\ud83d\\ude80\",\"items\":[0,true,null,{\"x\":3}]}";
    size_t i,n=strlen(sample);unsigned b;
    CHECK(!j_parse(&d,sample,n));
    CHECK(j_text(&d,j_field(&d,0,"unicode"),decoded,sizeof(decoded))==4);
    CHECK(!memcmp(decoded,"\xf0\x9f\x9a\x80",4));
    j_quote(&o,"ascii\n\"\\\xe2\x82\xac\xff\xc0\xed\xa0\x80");
    CHECK(!o.failed && !j_parse(&d,output,o.size));
    CHECK(strstr(output,"\\ufffd")!=NULL);
    /* Every truncated prefix and byte substitution must be safely bounded.
       Valid mutations must produce a traversable root token. ASan/UBSan runs this. */
    for(i=0;i<n;++i) {
        CHECK(j_parse(&d,sample,i)!=0);
        for(b=0;b<256;++b) {
            char mutation[256];memcpy(mutation,sample,n);mutation[i]=(char)b;
            if(!j_parse(&d,mutation,n)) CHECK(d.tokens[0].next==d.used && d.used<=1024);
        }
    }
    o.size=0;o.capacity=4;o.failed=0;j_quote(&o,"too long");CHECK(o.failed);
    return 0;
}
