/* Isolated no-auth regression: an earlier GCD timer must replace a long
 * deadline. Also exercise EV_ADD|EV_ENABLE, disable/enable-only and udata.
 * Run through tests/run_darling_timers.py in disposable Darling prefixes.
 */
typedef unsigned long long u64;
struct kevent64 {u64 ident;short filter;unsigned short flags;unsigned fflags;long long data;u64 udata,ext[2];};
struct timespec {long seconds,nanoseconds;};
extern int kqueue(void),close(int),kevent64(int,const struct kevent64 *,int,struct kevent64 *,int,unsigned,const struct timespec *);
extern int printf(const char *,...),fflush(void *),usleep(unsigned),strcmp(const char *,const char *);
extern unsigned alarm(unsigned);
extern u64 mach_absolute_time(void),dispatch_time(u64,long long);
extern const char _dispatch_source_type_timer;
extern void *dispatch_get_global_queue(long,unsigned long),*dispatch_source_create(const void *,unsigned long,unsigned long,void *);
extern void dispatch_source_set_timer(void *,u64,u64,u64),dispatch_source_set_event_handler_f(void *,void (*)(void *));
extern void dispatch_resume(void *),dispatch_source_cancel(void *),dispatch_after_f(u64,void *,void *,void (*)(void *));
static unsigned ticks,after;
static void tick(void *unused){(void)unused;__atomic_add_fetch(&ticks,1,__ATOMIC_RELAXED);}
static void later(void *unused){(void)unused;__atomic_store_n(&after,1,__ATOMIC_RELEASE);}
static int change(int queue,unsigned flags,long long delay,u64 udata) {
 struct kevent64 event={42,-7,flags,0,delay,udata,{0,0}};
 struct timespec zero={0,0};
 return kevent64(queue,&event,1,0,0,0,&zero);
}
static int receive(int queue,u64 udata) {
 struct kevent64 event={0};struct timespec timeout={0,250000000};
 u64 start=mach_absolute_time();
 int count=kevent64(queue,0,0,&event,1,0,&timeout);
 double ms=(mach_absolute_time()-start)/1000000.0;
 printf("TIMER count=%d elapsed_ms=%.2f udata=%llu\n",count,ms,count?event.udata:0);fflush(0);
 return count==1 && ms<200 && event.ident==42 && event.filter==-7 && event.data>=1 && event.udata==udata;
}
static int raw_timers(void) {
 int queue=kqueue();if(queue<0)return 2;
 if(change(queue,1|4|0x10,30000,11) || change(queue,1|4|0x10,20,22) || !receive(queue,22))return 3;
 if(change(queue,1|0x10,30000,33) || change(queue,1|0x10,20,44) || !receive(queue,44))return 4;
 if(change(queue,1|0x10,20,55) || change(queue,8,0,0))return 5;
 usleep(30000);
 struct kevent64 event;struct timespec zero={0,0};
 if(kevent64(queue,0,0,&event,1,0,&zero)!=0)return 6;
 /* Enable-only ignores a supplied new deadline and udata. */
 if(change(queue,4,30000,99) || !receive(queue,55))return 7;
 if(close(queue))return 8;
 printf("RAW_TIMER_PASS: rearm, udata and disable/enable-only\n");fflush(0);return 0;
}
static int dispatch_timers(void) {
 void *queue=dispatch_get_global_queue(0,0);
 void *long_timer=dispatch_source_create(&_dispatch_source_type_timer,0,0,queue);
 void *short_timer=dispatch_source_create(&_dispatch_source_type_timer,0,0,queue);
 if(!long_timer || !short_timer)return 2;
 dispatch_source_set_event_handler_f(long_timer,later);
 dispatch_source_set_timer(long_timer,dispatch_time(0,30000000000LL),~0ULL,1000000);
 dispatch_resume(long_timer);
 usleep(50000);
 dispatch_source_set_event_handler_f(short_timer,tick);
 dispatch_source_set_timer(short_timer,dispatch_time(0,16000000),16000000,1000000);
 dispatch_resume(short_timer);
 dispatch_after_f(dispatch_time(0,80000000),queue,0,later);
 u64 start=mach_absolute_time();
 for(int i=0;i<200 && (__atomic_load_n(&ticks,__ATOMIC_RELAXED)<8 || !__atomic_load_n(&after,__ATOMIC_ACQUIRE));i++)usleep(10000);
 double ms=(mach_absolute_time()-start)/1000000.0;
 dispatch_source_cancel(long_timer);dispatch_source_cancel(short_timer);
 printf("DISPATCH_TIMER ticks=%u after=%u elapsed_ms=%.2f\n",ticks,after,ms);fflush(0);
 if(ticks<8 || !after || ms>500)return 3;
 printf("DISPATCH_TIMER_PASS: eight short intervals and dispatch_after ahead of a long timer\n");fflush(0);return 0;
}
int main(int argc,char **argv) {
 alarm(6);
 return argc>1 && !strcmp(argv[1],"dispatch")?dispatch_timers():raw_timers();
}
