/* Benchmark actual generated entry points. This file is never part of the submission binary. */
#define main server_main
#include "server.c"
#undef main
static u64 ns(void) { struct timespec t; clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t); return (u64)t.tv_sec*1000000000ull+t.tv_nsec; }
int main(int argc,char **argv) {
  if(argc!=4) return 2;
  const char *kind=argv[1]; int batch=atoi(argv[2]),rounds=atoi(argv[3]);
  if(batch<1||batch>128||rounds<1) return 2;
  db_open(getenv("SQLITE_PATH")); app_init(); app_check_inv();
  const char *key="twelve-dollar-challenge"; hmac_init((const u8*)key,strlen(key)); now_s=time(0);
  int decode=!strncmp(kind,"decode-",7);
  int authed=decode||!strcmp(kind,"like")||!strcmp(kind,"create");
  char paths[1024][48]; req_t rq[1024];
  const char *auth=getenv("BENCH_AUTH"); if(!auth)auth="";
  const char *body="{\"body\":\"typed code says hello\"}";
  for(int i=0;i<1024;i++) {
    const char *path;
    if(!strcmp(kind,"health")) path="/health";
    else if((!strcmp(kind,"decode-create")||!strcmp(kind,"create"))) path="/posts";
    else if((!strcmp(kind,"decode-like")||!strcmp(kind,"like"))) { snprintf(paths[i],sizeof paths[i],"/posts/%d/like",499000+i%1000); path=paths[i]; }
    else if(!strcmp(kind,"post")||(!strcmp(kind,"mixed")&&(i&1))) { snprintf(paths[i],sizeof paths[i],"/posts/%d",499000+i%1000); path=paths[i]; }
    else path="/feed";
    rq[i]=(req_t){.method=authed?M_POST:M_GET,.path=(const u8*)path,.path_len=(u32)strlen(path),
      .auth=(const u8*)auth,.auth_len=(u32)strlen(auth),.has_auth=authed,.body=(const u8*)body,.body_len=(u32)strlen(body)};
  }
  job_t js[128]; u64 start=0,checksum=0;
  for(int r=-100;r<rounds;r++) {
    if(!r)start=ns();
    memset(js,0,(size_t)batch*sizeof *js);
    for(int i=0;i<batch;i++) { app_decode(&rq[(r+100+i)%1024],&js[i]); if(!js[i].kind) {fprintf(stderr,"decode failed\n");return 3;} }
    if(!decode) app_batch(js,batch);
    for(int i=0;i<batch;i++) { checksum+=js[i].kind; if(js[i].resp){checksum+=js[i].resp->len; obuf_release(js[i].resp);} }
    if(decode) arena_used=0;
  }
  printf("%s batch=%d ns/request=%.2f checksum=%llu sizeof_job=%zu\n",kind,batch,(double)(ns()-start)/rounds/batch,(unsigned long long)checksum,sizeof(job_t));
  return 0;
}
