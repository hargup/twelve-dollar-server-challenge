#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include "out.c"
int main(void) {
  const i64 values[]={LLONG_MIN,LLONG_MIN+1,-1,0,1,LLONG_MAX};
  for(int k=0;k<1000;k++)for(unsigned i=0;i<sizeof values/sizeof *values;i++) {
    out_t o;out_begin_inline(&o);out_i64(&o,values[i]);
    char want[32];int n=snprintf(want,sizeof want,"%lld",values[i]);
    assert(o.p-o.start==n&&!memcmp(o.start,want,n));
    static const u8 header[]="HTTP/1.1 200 OK\r\nContent-Length: ";
    obuf *b=out_finish(&o,header,sizeof header-1);
    b->data+=3;b->len-=3; /* short-send bookkeeping must not affect ownership */
    obuf_release(b);
  }
  for(int k=0;k<1000;k++) {
    out_t o;out_begin_inline(&o);out_lit(&o,"prefix",6);out_need(&o,20000);
    assert(o.b->cap!=OUT_INLINE&&!memcmp(o.start,"prefix",6));
    memset(o.p,'x',20000);o.p+=20000;out_abort(&o);
  }
  puts("output: signed extrema, inline reuse, partial sends, promotion and abort passed");
}
