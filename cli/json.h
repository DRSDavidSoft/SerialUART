#ifndef SC_JSON_H
#define SC_JSON_H
#include <stddef.h>
enum { J_OBJECT=1,J_ARRAY,J_STRING,J_NUMBER,J_TRUE,J_FALSE,J_NULL };
typedef struct { unsigned type,start,end,next,count; } j_token;
typedef struct { const char *text; size_t size,pos; unsigned used; j_token tokens[1024]; } j_doc;
int j_parse(j_doc *,const char *,size_t);
int j_field(const j_doc *,int,const char *); /* -1 absent */
int j_text(const j_doc *,int,char *,size_t); /* decoded UTF-8 byte count */
int j_uint(const j_doc *,int,unsigned,unsigned *);
typedef struct { char *data; size_t size,capacity; int failed; } j_out;
void j_raw(j_out *,const char *);
void j_quote(j_out *,const char *);
void j_print(j_out *,const char *,...);
#endif
