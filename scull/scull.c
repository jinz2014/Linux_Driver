#include <linux/module.h>
#include <linux/kernel.h>   /* printk() */
#include <linux/init.h>     /* MODULE_PARM */
#include <linux/slab.h>     /* kmalloc() */
#include <asm/uaccess.h>    /* copy_to/from_user() */
//#include <linux/fs.h>       /* everything... */
#include <linux/errno.h>    /* error codes */
#include <linux/types.h>    /* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h>    /* O_ACCMODE */
#include <asm/system.h>     /* cli(), *_flags */

#include "scull.h"          /* local definitions */

/*
 * static symbols not used as we export no symbols
 */

int scull_major =   SCULL_MAJOR;
int scull_nr_devs = SCULL_NR_DEVS;    /* number of bare scull devices */
int scull_quantum = SCULL_QUANTUM;
int scull_qset =    SCULL_QSET;

// Note Macro MODULE_PARM() not supported 
module_param(scull_major,int,0000);
module_param(scull_nr_devs,int,0000);
module_param(scull_quantum,int, 0000);
module_param(scull_qset,int, 0000);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("JZM");
MODULE_DESCRIPTION("Test a scull device that supports open,read,write,release"); 
 

Scull_Dev *scull_devices; /* allocated in scull_init_module */

//struct file_operations *scull_fop_array[]={
//  &scull_fops,      /* type 0 */
//  &scull_priv_fops, /* type 1 */
//  &scull_pipe_fops, /* type 2 */
//  &scull_sngl_fops, /* type 3 */
//  &scull_user_fops, /* type 4 */
//  &scull_wusr_fops  /* type 5 */
//};
struct file_operations *scull_fop_array[]={
  &scull_fops,      /* type 0 */
  NULL, /* type 1 */
  NULL, /* type 2 */
  NULL, /* type 3 */
  NULL, /* type 4 */
  NULL  /* type 5 */
};
#define SCULL_MAX_TYPE 5

int scull_trim(Scull_Dev *dev) {
  Scull_Dev *next, *dptr;
  int qset = dev->qset;   /* "dev" is not-null */
  int i;

  for (dptr = dev; dptr; dptr = next) { /* all the list items */
    if (dptr->data) {
      for (i = 0; i < qset; i++)
        if (dptr->data[i])
          kfree(dptr->data[i]);
      kfree(dptr->data);
      dptr->data=NULL;
    }
    next=dptr->next;
    if (dptr != dev) kfree(dptr); /* all of them but the first */
  }
  dev->size = 0;
  dev->quantum = scull_quantum;
  dev->qset = scull_qset;
  dev->next = NULL;
  return 0;
}

int scull_open(struct inode *inode, struct file *filp)
{
  Scull_Dev *dev; /* device information */
  int num = NUM(inode->i_rdev);
  int type = TYPE(inode->i_rdev);

  // just type 0
  printk(KERN_ALERT "num=%d type=%d\n", num, type);

  // check device number
  dev = (Scull_Dev *) filp->private_data;
  if (!dev) {
    if (num >= scull_nr_devs) return -ENODEV;
    dev = &scull_devices[num];
    filp->private_data = dev;
  }

  if ((filp->f_flags & O_ACCMODE) == O_WRONLY) {
    if (down_interruptible(&dev->sem)) {
      //MOD_DEC_USE_COUNT;
        module_put(THIS_MODULE);
      return -ERESTARTSYS;
    }
    scull_trim(dev);
    up(&dev->sem);
  }

  try_module_get(THIS_MODULE);
  return 0; // success
}

int scull_release(struct inode *inode, struct file *filp)
{
  //MOD_DEC_USE_COUNT;
  module_put(THIS_MODULE);
  return 0;
}

Scull_Dev *scull_follow(Scull_Dev *dev, int n)
{
  while (n--) {
    if (!dev->next) {
      dev->next = kmalloc(sizeof(Scull_Dev), GFP_KERNEL);
      memset(dev->next, 0, sizeof(Scull_Dev));
    }
    dev = dev->next;
  }
  return dev;
}

/*
 * Data management: read and write
 */

ssize_t scull_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
  Scull_Dev *dev = filp->private_data; /* the first listitem */
  Scull_Dev *dptr;
  int quantum = dev->quantum;
  int qset = dev->qset;
  int itemsize = quantum * qset; /* how many bytes in the listitem */
  int item, s_pos, q_pos, rest;
  ssize_t ret = 0;

  if (down_interruptible(&dev->sem))
    return -ERESTARTSYS;

  if (*f_pos >= dev->size)
    goto out;

  if (*f_pos + count > dev->size)
    count = dev->size - *f_pos;

  /* find listitem, qset index, and offset in the quantum */
  item = (long)*f_pos / itemsize;
  rest = (long)*f_pos % itemsize;
  s_pos = rest / quantum; 
  q_pos = rest % quantum;

  /* follow the list up to the right position */
  dptr = scull_follow(dev, item);

  if (!dptr->data)
    goto out; /* don't fill holes */

  if (!dptr->data[s_pos])
    goto out;

  /* read only up to the end of this quantum */
  if (count > quantum - q_pos)
    count = quantum - q_pos;

  printk(KERN_ALERT "copy_to_user: count=%zu\n", count);

  if (copy_to_user(buf, dptr->data[s_pos]+q_pos, count)) {
        ret = -EFAULT;
        goto out;
  }

  *f_pos += count;
  ret = count;

  out:
    up(&dev->sem);
    return ret;
}

ssize_t scull_write(struct file *filp, const char *buf, size_t count,
                    loff_t *f_pos)
{
  Scull_Dev *dev = filp->private_data;
  Scull_Dev *dptr;
  int quantum = dev->quantum;
  int qset = dev->qset;
  int itemsize = quantum * qset;
  int item, s_pos, q_pos, rest;
  ssize_t ret = -ENOMEM; /* value used in "goto out" statements */

  // exclusive write
  if (down_interruptible(&dev->sem)) return -ERESTARTSYS;

  /* find listitem, qset index and offset in the quantum */
  item = (long)*f_pos / itemsize;
  rest = (long)*f_pos % itemsize;
  s_pos = rest / quantum; 
  q_pos = rest % quantum;

  /* follow the list up to the right position */
  dptr = scull_follow(dev, item);
  if (!dptr->data) {
    dptr->data = kmalloc(qset * sizeof(char *), GFP_KERNEL);
    if (!dptr->data)
      goto out;
    memset(dptr->data, 0, qset * sizeof(char *));
  }

  // no allocation when already allocated
  if (!dptr->data[s_pos]) {
    dptr->data[s_pos] = kmalloc(quantum, GFP_KERNEL);
    if (!dptr->data[s_pos])
    goto out;
  }

  /* write only up to the end of this quantum */
  if (count > quantum - q_pos)
    count = quantum - q_pos;

  printk(KERN_ALERT "copy_from_user: count=%zu\n", count);
  if (copy_from_user(dptr->data[s_pos]+q_pos, buf, count)) {
    ret = -EFAULT;
    goto out;
  }

  /* update the file position */
  *f_pos += count;
  ret = count;

  /* update the size */
  if (dev->size < *f_pos)
    dev-> size = *f_pos;

  out:
  up(&dev->sem);
  return ret;
}

struct file_operations scull_fops = {
  //llseek:     scull_llseek,
  read:       scull_read,
  write:      scull_write,
  //ioctl:      scull_ioctl,
  open:       scull_open,
  release:    scull_release,
};

static int __init scull_init_module(void)
{
  int result, i;
  //SET_MODULE_OWNER(&scull_fops);
  result = register_chrdev(scull_major, "scull", &scull_fops);
  if (result < 0) {
    printk(KERN_WARNING "scull: can't get major %d\n",scull_major);
    return result;
  }
  if (scull_major == 0) 
    scull_major = result; /* dynamic */

 // allocate the devices - we can't have them static, as the number can be specified at load time
  scull_devices = kmalloc(scull_nr_devs * sizeof(Scull_Dev), GFP_KERNEL);
  if (!scull_devices) {
    result = -ENOMEM;
    goto fail;
  }
  memset(scull_devices, 0, scull_nr_devs * sizeof(Scull_Dev));
  for (i=0; i < scull_nr_devs; i++) {
    scull_devices[i].quantum = scull_quantum;
    scull_devices[i].qset = scull_qset;
    sema_init(&scull_devices[i].sem, 1);
  }

  return 0; 

fail:
  scull_cleanup_module();
  return result;
}

static void scull_cleanup_module(void)
{
  int i;

  // remove the scull device (/proc/devices/scull)
  unregister_chrdev(scull_major, "scull");

  if (scull_devices) {
    for (i=0; i<scull_nr_devs; i++) {
      scull_trim(scull_devices+i);
    }
    kfree(scull_devices);
  }
}

module_init(scull_init_module);
module_exit(scull_cleanup_module);


