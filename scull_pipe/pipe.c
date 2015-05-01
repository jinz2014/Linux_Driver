/*
 * pipe.c -- fifo driver for scull
 *
 * Copyright (C) 2001 Alessandro Rubini and Jonathan Corbet
 * Copyright (C) 2001 O'Reilly & Associates
 *
 * The source code in this file can be freely used, adapted,
 * and redistributed in source or binary form, so long as an
 * acknowledgment appears in derived source files.  The citation
 * should list that the code comes from the book "Linux Device
 * Drivers" by Alessandro Rubini and Jonathan Corbet, published
 * by O'Reilly & Associates.   No warranty is attached;
 * we cannot take responsibility for errors or fitness for use.
 *
 */
 
#ifndef __KERNEL__
#  define __KERNEL__
#endif
#ifndef MODULE
#  define MODULE
#endif

#define __NO_VERSION__
#include <linux/module.h>  /* get MOD_DEC_USE_COUNT, not the version string */
#include <linux/kernel.h> /* printk() */
#include <linux/version.h> /* needed for the conditionals in scull.h */
#include <asm/uaccess.h>    /* copy_to/from_user() */
#include <linux/slab.h> /* kmalloc() */
#include <linux/fs.h>     /* everything... */
#include <linux/proc_fs.h>
#include <linux/errno.h>  /* error codes */
#include <linux/types.h>  /* size_t */

#include "scull.h"        /* local definitions and sysdep.h */

#ifndef min
#  define min(a,b) ((a)<(b) ? (a) : (b)) /* we use it in this file */
#endif

// 
// To model the sleep/wakeup, a process generates the data and wake the reading
// process; similarly, reading processes are used to wake writer processes that are waiting
// for buffer space to become available.
typedef struct Scull_Pipe {
    wait_queue_head_t inq, outq;    /* read and write queues */
    char *buffer, *end;             /* begin of buf, end of buf */
    int buffersize;                 /* used in pointer arithmetic */
    char *rp, *wp;                  /* where to read, where to write in the buf */
    int nreaders, nwriters;         /* number of openings for r/w */
    struct fasync_struct *async_queue; /* asynchronous readers */
    struct semaphore sem;           /* mutual exclusion semaphore */
    devfs_handle_t handle;         /* only used if devfs is there */
} Scull_Pipe;

/* parameters */
int scull_p_nr_devs = SCULL_P_NR_DEVS;  /* number of pipe devices */
int scull_p_buffer =  SCULL_P_BUFFER;   /* buffer size */

module_param(scull_p_nr_devs,int, 0000);
module_param(scull_p_buffer,int, 0000);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("JZM");
MODULE_DESCRIPTION("Test a blocking I/O device"); 

Scull_Pipe *scull_p_devices;


/*
 * Open and close
 */

int scull_p_open(struct inode *inode, struct file *filp)
{
    Scull_Pipe *dev;
    int num = NUM(inode->i_rdev);

    if (!filp->private_data) {
        if (num >= scull_p_nr_devs) return -ENODEV;
        dev = &scull_p_devices[num];
        filp->private_data = dev;
    } else {
        dev = filp->private_data;
    }
    if (down_interruptible(&dev->sem))
        return -ERESTARTSYS;
    if (!dev->buffer) { /* allocate the buffer */
        dev->buffer = kmalloc(scull_p_buffer, GFP_KERNEL);
        if (!dev->buffer) {
            up(&dev->sem);
            return -ENOMEM;
        }
    }
    dev->buffersize = scull_p_buffer;
    dev->end = dev->buffer + dev->buffersize;
    dev->rp = dev->wp = dev->buffer; /* rd and wr from the beginning */

    /* use f_mode,not  f_flags: it's cleaner (fs/open.c tells why) */
    if (filp->f_mode & FMODE_READ)
        dev->nreaders++;
    if (filp->f_mode & FMODE_WRITE)
        dev->nwriters++;
    up(&dev->sem);
    
    filp->private_data = dev;
    MOD_INC_USE_COUNT;
    return 0;
}

int scull_p_release(struct inode *inode, struct file *filp)
{
    Scull_Pipe *dev = filp->private_data;
    int scull_p_fasync(fasync_file fd, struct file *filp, int mode);

    /* remove this filp from the asynchronously notified filp's */
    scull_p_fasync(-1, filp, 0);
    down(&dev->sem);
    if (filp->f_mode & FMODE_READ)
        dev->nreaders--;
    if (filp->f_mode & FMODE_WRITE)
        dev->nwriters--;
    if (dev->nreaders + dev->nwriters == 0) {
        kfree(dev->buffer);
        dev->buffer = NULL; /* the other fields are not checked on open */
    }
    up(&dev->sem);
    MOD_DEC_USE_COUNT;
    return 0;
}

/*
 * Data management: read and write
 */

ssize_t scull_p_read (struct file *filp, char *buf, size_t count,
                loff_t *f_pos)
{
    Scull_Pipe *dev = filp->private_data;

    if (f_pos != &filp->f_pos) return -ESPIPE;

    if (down_interruptible(&dev->sem))
        return -ERESTARTSYS;

    while (dev->rp == dev->wp) { /* nothing to read as the buffer is empty */
        /* release the lock. If we were to sleep holding the lock, 
         * no writer would have the chance to wake us up */
        up(&dev->sem); 
        /* if the user has requested non-blocking I/O, and return if so */
        if (filp->f_flags & O_NONBLOCK)
            return -EAGAIN;
        PDEBUG("\"%s\" reading: going to sleep\n", current->comm);
        if (wait_event_interruptible(dev->inq, (dev->rp != dev->wp)))
            return -ERESTARTSYS; /* signal: tell the vfs layer to restart the system call */

        /* otherwise loop, but first reacquire the lock. 
         * As somebody else could have been waiting
         * for the data and they might win the race and 
         * get the data*/
        if (down_interruptible(&dev->sem))
            return -ERESTARTSYS;
    }

    /* ok, data is there, return something */
    if (dev->wp > dev->rp)
        count = min(count, dev->wp - dev->rp);
    else /* the write pointer has wrapped, return data up to dev->end */
        count = min(count, dev->end - dev->rp);

    if (copy_to_user(buf, dev->rp, count)) {
      /* release the lock if copy_to_user is not successful */
        up (&dev->sem);
        return -EFAULT;
    }
    dev->rp += count;
    if (dev->rp == dev->end)
        dev->rp = dev->buffer; /* wrapped */
    up (&dev->sem);

    /* finally, awake any writers and return */
    wake_up_interruptible(&dev->outq);
    PDEBUG("\"%s\" did read %li bytes\n",current->comm, (long)count);
    return count;
}

/* buffersize = 8
 * 
 * full:
 * wptr --> 0
 * rptr --> 1
 *
 * full:
 * wptr --> 7
 * rptr --> 0
 *
 * wptr --> 7
 * rptr --> 5
 *
 * wptr --> 1
 * rptr --> 5
 */
static inline int spacefree(Scull_Pipe *dev)
{
    if (dev->rp == dev->wp)
        return dev->buffersize - 1;
    return ((dev->rp + dev->buffersize - dev->wp) % dev->buffersize) - 1;
}

ssize_t scull_p_write(struct file *filp, const char *buf, size_t count,
                loff_t *f_pos)
{
    Scull_Pipe *dev = filp->private_data;
    
    if (f_pos != &filp->f_pos) return -ESPIPE;

    if (down_interruptible(&dev->sem))
        return -ERESTARTSYS;
    
    /* Make sure there's space to write */
    while (spacefree(dev) == 0) { /* full */
        up(&dev->sem);
        if (filp->f_flags & O_NONBLOCK)
            return -EAGAIN;
        PDEBUG("\"%s\" writing: going to sleep\n",current->comm);
        if (wait_event_interruptible(dev->outq, spacefree(dev) > 0))
            return -ERESTARTSYS; /* signal: tell the fs layer to handle it */
        if (down_interruptible(&dev->sem))
            return -ERESTARTSYS;
    }
    /* ok, space is there, accept something */
    count = min(count, spacefree(dev));
    if (dev->wp >= dev->rp)
        count = min(count, dev->end - dev->wp); /* up to end-of-buffer */
    else /* the write pointer has wrapped, fill up to rp-1 */
        count = min(count, dev->rp - dev->wp - 1);
    PDEBUG("Going to accept %li bytes to %p from %p\n",
           (long)count, dev->wp, buf);
    if (copy_from_user(dev->wp, buf, count)) {
        up (&dev->sem);
        return -EFAULT;
    }
    dev->wp += count;
    if (dev->wp == dev->end)
        dev->wp = dev->buffer; /* wrapped */

    up(&dev->sem);

    /* finally, awake any reader */
    wake_up_interruptible(&dev->inq);  /* blocked in read() and select() */

    /* and signal asynchronous readers, explained late in chapter 5 */
    if (dev->async_queue)
        kill_fasync(&dev->async_queue, SIGIO, POLL_IN);
    PDEBUG("\"%s\" did write %li bytes\n",current->comm, (long)count);
    return count;
}

#ifdef __USE_OLD_SELECT__
int scull_p_poll(struct inode *inode, struct file *filp,
                  int mode, select_table *table)
{
    Scull_Pipe *dev = filp->private_data;

    if (mode == SEL_IN) {
        if (dev->rp != dev->wp) return 1; /* readable */
        PDEBUG("Waiting to read\n");
        select_wait(&dev->inq, table); /* wait for data */
        return 0;
    }
    if (mode == SEL_OUT) {
        /*
         * The buffer is circular; it is considered full
         * if "wp" is right behind "rp". "left" is 0 if the
         * buffer is empty, and it is "1" if it is completely full.
         */
        int left = (dev->rp + dev->buffersize - dev->wp) % dev->buffersize;
        if (left != 1) return 1; /* writable */
        PDEBUG("Waiting to write\n");
        select_wait(&dev->outq, table); /* wait for free space */
        return 0;
    }
    return 0; /* never exception-able */
}
#else /* Use poll instead, already shown */

unsigned int scull_p_poll(struct file *filp, poll_table *wait)
{
    Scull_Pipe *dev = filp->private_data;
    unsigned int mask = 0;

    /*
     * The buffer is circular; it is considered full
     * if "wp" is right behind "rp". "left" is 0 if the
     * buffer is empty, and it is "1" if it is completely full.
     */
    int left = (dev->rp + dev->buffersize - dev->wp) % dev->buffersize;

    poll_wait(filp, &dev->inq,  wait);
    poll_wait(filp, &dev->outq, wait);
    if (dev->rp != dev->wp) mask |= POLLIN | POLLRDNORM;  /* readable */
    if (left != 1)          mask |= POLLOUT | POLLWRNORM; /* writable */

    return mask;
}

#endif




int scull_p_fasync(fasync_file fd, struct file *filp, int mode)
{
    Scull_Pipe *dev = filp->private_data;

    return fasync_helper(fd, filp, mode, &dev->async_queue);
}


loff_t scull_p_llseek(struct file *filp,  loff_t off, int whence)
{
    return -ESPIPE; /* unseekable */
}

#ifdef SCULL_DEBUG
void scullp_proc_offset(char *buf, char **start, off_t *offset, int *len)
{
    if (*offset == 0)
        return;
    if (*offset >= *len) {      /* Not there yet */
        *offset -= *len;
        *len = 0;
    }
    else {                      /* We're into the interesting stuff now */
        *start = buf + *offset;
        *offset = 0;
    }
}


int scull_read_p_mem(char *buf, char **start, off_t offset,
                   int count, int *eof, void *data)
{
    int i, len;
    Scull_Pipe *p;

    #define LIMIT (PAGE_SIZE-200) /* don't print any more after this size */
    *start = buf;
    len = sprintf(buf, "Default buffersize is %i\n", scull_p_buffer);
    for(i = 0; i<scull_p_nr_devs && len <= LIMIT; i++) {
        p = &scull_p_devices[i];
        if (down_interruptible(&p->sem))
            return -ERESTARTSYS;
        len += sprintf(buf+len, "\nDevice %i: %p\n", i, p);
/*        len += sprintf(buf+len, "   Queues: %p %p\n", p->inq, p->outq);*/
        len += sprintf(buf+len, "   Buffer: %p to %p (%i bytes)\n",
                       p->buffer, p->end, p->buffersize);
        len += sprintf(buf+len, "   rp %p   wp %p\n", p->rp, p->wp);
        len += sprintf(buf+len, "   readers %i   writers %i\n",
                       p->nreaders, p->nwriters);
        up(&p->sem);
        scullp_proc_offset(buf, start, &offset, &len);
    }
    *eof = (len <= LIMIT);
    return len;
}


#ifdef USE_PROC_REGISTER

static int scull_p_get_info(char *buf, char **start, off_t offset, int len,
                int unused)
{
    int eof = 0;
    return scull_read_p_mem(buf, start, offset, len, &eof, NULL);
}

struct proc_dir_entry scull_proc_p_entry = {
        0,                  /* low_ino: the inode -- dynamic */
        9, "scullpipe",     /* len of name and name */
        S_IFREG | S_IRUGO,  /* mode */
        1, 0, 0,            /* nlinks, owner, group */
        0, NULL,            /* size - unused; operations -- use default */
        &scull_p_get_info,  /* function used to read data */
        /* nothing more */
    };

static inline void create_proc_read_entry(const char *name, mode_t mode,
                struct proc_dir_entry *base, void *read_func, void *data)
{
    proc_register_dynamic(&proc_root, &scull_proc_p_entry);
}

static inline void remove_proc_entry(char *name, void *parent)
{
    proc_unregister(&proc_root, scull_proc_p_entry.low_ino);
}

#endif /* USE_PROC_REGISTER */

#endif


/*
 * The file operations for the pipe device
 * (some are overlayed with bare scull)
 */
struct file_operations scull_pipe_fops = {
    llseek:     scull_p_llseek,
    read:       scull_p_read,
    write:      scull_p_write,
    poll:       scull_p_poll,
    ioctl:      scull_ioctl,
    open:       scull_p_open,
    release:    scull_p_release,
    fasync:     scull_p_fasync,
};


char pipename[8]; /* only used for devfs names */

int scull_p_init(void)
{
    int i;

    SET_MODULE_OWNER(&scull_pipe_fops);
    scull_p_devices = kmalloc(scull_p_nr_devs * sizeof(Scull_Pipe),
                              GFP_KERNEL);
    if (scull_p_devices == NULL)
        return -ENOMEM;

    memset(scull_p_devices, 0, scull_p_nr_devs * sizeof(Scull_Pipe));
    for (i = 0; i < scull_p_nr_devs; i++) {
        init_waitqueue_head(&(scull_p_devices[i].inq));
        init_waitqueue_head(&(scull_p_devices[i].outq));
        sema_init(&scull_p_devices[i].sem, 1);
    }
#ifdef SCULL_DEBUG
    create_proc_read_entry("scullpipe", 0, NULL, scull_read_p_mem, NULL);
#endif
    return 0;
}

/*
 * This is called by cleanup_module or on failure.
 * It is required to never fail, even if nothing was initialized first
 */
void scull_p_cleanup(void)
{
    int i;

#ifdef SCULL_DEBUG
    remove_proc_entry("scullpipe", 0);
#endif

    if (!scull_p_devices) return; /* nothing else to release */
    for (i=0; i < scull_p_nr_devs; i++) {
        if (scull_p_devices[i].buffer)
            kfree(scull_p_devices[i].buffer);
    }
    kfree(scull_p_devices);
    scull_p_devices = NULL; /* pedantic */
    return;
}
