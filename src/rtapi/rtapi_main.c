/********************************************************************
 * Description:  rtapi_main.c
 *
 *               This file, 'rtapi_main.c', implements the RTAPI
 *               rtapi_app_main() and rtapi_app_exit() functions
 *               for userspace thread systems.
 *
 *               It should not be used for kernel thread systems.
 *
 ********************************************************************/

#include "config.h"
#include "rtapi.h"
#include "rtapi_common.h"
#include "rtapi_support.h"
#include "rtapi/shmdrv/shmdrv.h"

#include <unistd.h>		/* ssize_t, _SC_PAGESIZE */
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h> 

#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */
#include "rtapi_common.h"       /* global_data_t */
#include "rtapi_kdetect.h"      /* environment autodetection */

MODULE_AUTHOR("Michael Haberler");
MODULE_DESCRIPTION("RTAPI module support - userland threads");
MODULE_LICENSE("GPL");

int rtapi_instance;
static char *instance_name = "";
static int hal_size = HAL_SIZE;                 // default size of HAL data segment
static int rt_msg_level = RTAPI_MSG_INFO;	/* initial RT message printing level */ 
static int user_msg_level = RTAPI_MSG_INFO;	/* initial User message printing level */ 

RTAPI_MP_STRING(instance_name,"name of this instance")
RTAPI_MP_INT(rtapi_instance, "instance ID");
RTAPI_MP_INT(hal_size, "size of the HAL data segment");
RTAPI_MP_INT(rt_msg_level, "RT debug message level (default=3)");
RTAPI_MP_INT(user_msg_level, "user debug message level (default=3)");
EXPORT_SYMBOL(rtapi_instance);
EXPORT_SYMBOL(global_data);

global_data_t *global_data = NULL;
ringbuffer_t rtapi_message_buffer;   // error ring access strcuture

static int flavor_id = THREAD_FLAVOR_ID;
static int check_compatible();

global_data_t *get_global_handle(void)
{
    return global_data;
}
EXPORT_SYMBOL(get_global_handle);

int rtapi_app_main(void)
{
    int retval, compatible;
    int globalkey = OS_KEY(GLOBAL_KEY, rtapi_instance);
    int rtapikey = OS_KEY(RTAPI_KEY, rtapi_instance);

    page_size = sysconf(_SC_PAGESIZE);
    shmdrv_loaded  = shmdrv_available();

    rtapi_print_msg(RTAPI_MSG_DBG,"RTAPI:%d %s %s init\n",
		    rtapi_instance,
		    rtapi_get_handle()->thread_flavor_name,
		    GIT_VERSION);

    retval = shm_common_new(globalkey, sizeof(global_data_t), 
			    rtapi_instance, (void **) &global_data, 1);
    if (retval ==  0) {
	// the global_data segment already existed.
	rtapi_print_msg(RTAPI_MSG_ERR,
			"RTAPI:%d ERROR: global segment 0x%x already exists!\n",
			rtapi_instance, globalkey);
	return -EBUSY;
    }
    if (retval < 0) {
	 rtapi_print_msg(RTAPI_MSG_ERR,
			 "RTAPI:%d ERROR: shm_common_new() failed key=0x%x %s\n",
			 rtapi_instance, globalkey, strerror(-retval));
	 return retval;
    }
    retval = shm_common_new(rtapikey, sizeof(rtapi_data_t), 
			    rtapi_instance, (void **) &rtapi_data, 1);
   if (retval ==  0) {
	// the rtapi_data segment already existed.
	rtapi_print_msg(RTAPI_MSG_ERR,
			"RTAPI:%d ERROR: rtapi segment 0x%x already exists!\n",
			rtapi_instance, rtapikey);
	return -EBUSY;
    }
    if (retval < 0) {
	 rtapi_print_msg(RTAPI_MSG_ERR,
			 "RTAPI:%d ERROR: shm_common_new() failed key=0x%x %s\n",
			 rtapi_instance, rtapikey, strerror(-retval));
	 return retval;
    }

    /* perform a global init */
    init_global_data(global_data, rtapi_instance,
		     hal_size, rt_msg_level, user_msg_level, 
		     instance_name);
    init_rtapi_data(rtapi_data);

    compatible = check_compatible();
    if (!compatible) {
	if ((retval = shm_common_detach(sizeof(global_data_t), global_data)) != 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "RTAPI:%d ERROR: shm_common_detach(0x8.8%x) failed: %s\n",
			    rtapi_instance, globalkey, strerror(-retval));
	}
	global_data = NULL;
	if ((retval = shm_common_detach(sizeof(rtapi_data_t), rtapi_data)) != 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "RTAPI:%d ERROR: shm_common_detach(0x8.8%x) failed: %s\n",
			    rtapi_instance, rtapikey, strerror(-retval));
	}
	rtapi_data = NULL;
    }
    return 0;
}

void rtapi_app_exit(void)
{
    int retval;

    rtapi_print_msg(RTAPI_MSG_DBG,"RTAPI:%d exit\n",
		    rtapi_instance);

    retval = shm_common_detach(sizeof(rtapi_data_t), rtapi_data);
    if (retval) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"RTAPI:%d ERROR: munmap(0x8.8%x) failed: %s\n",
			rtapi_instance,  OS_KEY(GLOBAL_KEY, rtapi_instance), 
			strerror(-retval));
    }
    rtapi_data = NULL;

    retval = shm_common_detach(sizeof(global_data_t), global_data);
    if (retval) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"RTAPI:%d ERROR: munmap(0x8.8%x) failed: %s\n",
			rtapi_instance,  OS_KEY(GLOBAL_KEY, rtapi_instance), 
			strerror(-retval));
    }
    global_data = NULL;
}

static int check_compatible()
{
    int retval = 1;
    const char *flavor_name = rtapi_get_handle()->thread_flavor_name;

    if (flavor_id == RTAPI_POSIX_ID)
	return 1; // no prerequisites

    if (kernel_is_xenomai() &&
	(flavor_id != RTAPI_XENOMAI_USER_ID)) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"RTAPI:%d started %s RTAPI on a Xenomai kernel\n",
			rtapi_instance,flavor_name);
	return 0;
    }

    if (kernel_is_rtai() &&
	(flavor_id != RTAPI_RTAI_KERNEL_ID)) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"RTAPI:%d started %s RTAPI on an RTAI kernel\n",
			rtapi_instance,flavor_name);
	return 0;
    }
    if (kernel_is_rtpreempt() &&
	(flavor_id != RTAPI_RT_PREEMPT_USER_ID)) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"RTAPI:%d started %s RTAPI on an RT PREEMPT kernel\n",
			rtapi_instance,flavor_name);
	return 0;
    }
    return retval;
}

