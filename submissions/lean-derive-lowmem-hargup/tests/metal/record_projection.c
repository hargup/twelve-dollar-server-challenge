/* Compare the projection with the original prefix decoder on valid SQLite records,
 * and exercise malformed/truncated input under ASAN/UBSAN. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "record.c"
static u32 rng=42;
static u32 rnd(void) { rng^=rng<<13; rng^=rng>>17; rng^=rng<<5; return rng; }
static int varint(u8 *p,u64 v) {
  u8 tmp[10]; int n=0;
  do {tmp[n++]=(u8)(v&127);v>>=7;} while(v);
  for(int i=0;i<n;i++)p[i]=tmp[n-i-1]|(i+1<n?128:0);
  return n;
}
int main(void) {
  for(int trial=0;trial<20000;trial++) {
    u8 data[4096]; u64 types[8]; u32 ncols=1+rnd()%8,head=1;
    for(u32 i=0;i<ncols;i++){types[i]=(rnd()%3)?13+2*(rnd()%400):rnd()%10;head+=varint(data+head,types[i]);}
    assert(head<128);data[0]=(u8)head;
    u32 size=head;
    for(u32 i=0;i<ncols;i++)for(u32 j=0;j<serial_len(types[i]);j++)data[size++]=(u8)rnd();
    for(u32 col=0;col<ncols;col++) {
      rec_t r={data,size};col_t expected[8],actual;
      assert(!rec_cols(&r,(int)col+1,expected)); assert(!rec_field(&r,col,&actual));
      assert(expected[col].t==actual.t&&expected[col].p==actual.p);
      if(col==1) { const u8 *text; u32 len; int rc=rec_text_after_rowid(&r,&text,&len);
        assert((rc==0)==col_is_text(&actual)); if(!rc)assert(text==actual.p&&len==col_len(&actual)); }
      for(int k=0;k<3;k++) {r.n=rnd()%(size+1);int a=rec_cols(&r,(int)col+1,expected),b=rec_field(&r,col,&actual);assert((a==0)==(b==0));}
    }
    for(u32 i=0;i<size;i++)data[i]=(u8)rnd();
    rec_t r={data,rnd()%(size+1)};col_t c;
    if(!rec_field(&r,rnd()%9,&c)){assert(c.p>=data&&c.p<=data+r.n);assert(col_len(&c)<=(u32)(data+r.n-c.p));}
  }
  puts("record projection: 20000 valid/random records and truncated prefixes passed");
}
