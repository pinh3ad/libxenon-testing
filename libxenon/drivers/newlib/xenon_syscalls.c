/* 
 * File:   xenon_syscalls.c
 * Author: cc
 *
 * Created on 4 avril 2012, 16:48
 */

#include <_ansi.h>
#include <_syslist.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/iosupport.h>
#include <sys/dirent.h>
#include <sys/errno.h>
#include <sys/time.h>

#include <assert.h>

#include <ppc/atomic.h>
#include <ppc/register.h>

#include <xenon_soc/xenon_power.h>
#include <xenon_smc/xenon_smc.h>

#include <threads/threads.h>
#include <threads/mutex.h>


// prototype
static void xenon_exit(int status);
static void *xenon_sbrk(struct _reent *ptr, ptrdiff_t incr);
static int xenon_gettimeofday(struct _reent *ptr, struct timeval *tp, struct timezone *tz);
static void xenon_malloc_unlock ( struct _reent *_r );
static void xenon_malloc_lock ( struct _reent *_r );
static void xenon_thread_malloc_lock ( struct _reent *_r );
static void xenon_thread_malloc_unlock ( struct _reent *_r );

ssize_t vfs_console_write(struct _reent *r, int fd, const char *src, size_t len);

#define XELL_FOOTER_OFFSET (256*1024-16)
#define XELL_FOOTER_LENGTH 16
#define XELL_FOOTER "xxxxxxxxxxxxxxxx"
extern void return_to_xell(unsigned int nand_addr, unsigned int phy_loading_addr);


extern void enet_quiesce();
extern void usb_shutdown(void);

//---------------------------------------------------------------------------------
__syscalls_t __syscalls = {
    //---------------------------------------------------------------------------------
    xenon_sbrk, // sbrk
    NULL, // lock_init
    NULL, // lock_close
    NULL, // lock_release
    NULL, // lock_acquire
    xenon_malloc_lock, // malloc_lock
    xenon_malloc_unlock, // malloc_unlock
    xenon_exit, // exit
    xenon_gettimeofday // gettod_r
};


// 22 nov 2005
#define	RTC_BASE	1132614024UL//1005782400UL

static int xenon_gettimeofday(struct _reent *ptr, struct timeval *tp, struct timezone *tz) {
    unsigned char msg[16] = {0x04};
    unsigned long long msec;
    unsigned long sec;

    xenon_smc_send_message(msg);
    xenon_smc_receive_response(msg);

    msec = msg[1] | (msg[2] << 8) | (msg[3] << 16) | (msg[4] << 24) | ((unsigned long long) msg[5] << 32);

    sec = msec / 1000;
    tp->tv_sec = sec + RTC_BASE;
    msec -= sec * 1000;
    tp->tv_usec = msec * 1000;

    return 0;
}


extern unsigned char heap_begin;
unsigned char *heap_ptr;

static void *xenon_sbrk(struct _reent *ptr, ptrdiff_t incr){
    unsigned char *res;
    if (!heap_ptr)
        heap_ptr = &heap_begin;
    res = heap_ptr;
    heap_ptr += incr;
    return res;
}

void shutdown_drivers() {
    // some drivers require a shutdown
    enet_quiesce();
    usb_shutdown();
}

void try_return_to_xell(unsigned int nand_addr, unsigned int phy_loading_addr) {
    if (!memcmp((void*) (nand_addr + XELL_FOOTER_OFFSET), XELL_FOOTER, XELL_FOOTER_LENGTH))
        return_to_xell(nand_addr, phy_loading_addr);
}

static void xenon_exit(int status) {
    char s[256];
    int i, stuck = 0;
	
	// shutdown threading
	threading_shutdown();

    sprintf(s, "[Exit] with code %d\n", status);
    vfs_console_write(NULL, 0, s, strlen(s));

    for (i = 0; i < 6; ++i) {
        if (xenon_is_thread_task_running(i)) {
            sprintf(s, "Thread %d is still running !\n", i);
            vfs_console_write(NULL, 0, s, strlen(s));
            stuck = 1;
        }
    }

    shutdown_drivers();

    if (stuck) {
        sprintf(s, "Can't reload Xell, looping...");
        vfs_console_write(NULL, 0, s, strlen(s));
    } else {
        sprintf(s, "Reloading Xell...");
        vfs_console_write(NULL, 0, s, strlen(s));
        xenon_set_single_thread_mode();

        try_return_to_xell(0xc8070000, 0x1c000000); // xell-gggggg (ggboot)
        try_return_to_xell(0xc8095060, 0x1c040000); // xell-2f (freeboot)
        try_return_to_xell(0xc8100000, 0x1c000000); // xell-1f, xell-ggggggg
    }

    for (;;);
}


static unsigned int spinlock=0;
static volatile int lockcount=0;
static volatile unsigned int lockowner=-1;

static void xenon_malloc_lock ( struct _reent *_r )
{
    assert(lockcount >= 0);

	if( lockcount == 0 || lockowner != mfspr(pir) )
    {
		lock(&spinlock);
		lockowner=mfspr(pir);
    }

	++lockcount;
}

static void xenon_malloc_unlock ( struct _reent *_r )
{
    assert(lockcount > 0);
	assert(lockowner == mfspr(pir));

	--lockcount;

    if ( lockcount == 0 )
    {
		unlock(&spinlock);  
		lockowner=-1;
    }
}


// thread version

static PTHREAD thread_lock_owner = NULL;
static MUTEX*  thread_mutex=0;
static volatile int thread_lock_count=0;


void newlib_thread_init(){
	// create our mutex first
	thread_mutex = mutex_create(1);
	
	// use threaded func
	__syscalls.malloc_lock = xenon_thread_malloc_lock;
	__syscalls.malloc_unlock = xenon_thread_malloc_unlock;
}

static void xenon_thread_malloc_lock ( struct _reent *_r )
{
    assert(thread_lock_count >= 0);

	if( thread_lock_count == 0 || thread_lock_owner != thread_get_current() )
    {
		mutex_acquire(thread_mutex,INFINITE);
		thread_lock_owner=thread_get_current();
    }

	++thread_lock_count;
}

static void xenon_thread_malloc_unlock ( struct _reent *_r )
{
    assert(thread_lock_count > 0);
	assert(thread_lock_owner == thread_get_current());

	--thread_lock_count;

    if ( thread_lock_count == 0 )
    {
		mutex_release(thread_mutex);
		thread_lock_owner=NULL;
    }
}