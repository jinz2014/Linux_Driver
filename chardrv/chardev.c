//
// a read-only device that reports count the number of read accesses
//
#include <linux/kernel.h>
#include <linux/module.h>
//#include <linux/init.h>    // for the macros
#include <linux/fs.h>     // fops,
#include <asm/uaccess.h>     // for put_user

int init_module(void);
void cleanup_module(void);
static int device_open(struct inode*, struct file*);
static int device_release(struct inode*, struct file*);
static ssize_t device_read(struct file*, char*, size_t, loff_t*);
static ssize_t device_write(struct file*, const char*, size_t, loff_t*);

#define SUCCESS 0
#define DEVICE_NAME "chardev" // as seen in /proc/devices
#define BUF_LEN 80 // max message length from the device

static int Major; // major number assigned to our device driver
static int Device_Open=0; // still open if it is not zero

static char msg[BUF_LEN]; // the msg the device will give when asked
static char *msg_ptr;

static struct file_operations fops = {
  .read = device_read,
  .write = device_write,
  .open = device_open,
  .release = device_release
};

int init_module(void) {
  // int register_chrdev(unsigned int major, const char *name, struct file_operations *fops);
  Major = register_chrdev(0, DEVICE_NAME, &fops);

  if (Major < 0) {
    printk("Error %d: Failed to register the char device\n", Major);
    return Major;
  }

  printk("<1>I was assigned major number %d.  To talk to\n", Major);
  printk("<1>the driver, create a dev file with\n");
  printk("'mknod /dev/hello c %d 0'.\n", Major);
  printk("<1>Try various minor numbers.  Try to cat and echo to\n");
  printk("the device file.\n");
  printk("<1>Remove the device file and module when done.\n");
  return 0;
}

void cleanup_module(void) {
  //unregister_chrdev returns void in new kernel version
  unregister_chrdev(Major, DEVICE_NAME);
}

/* 
 * Called when a process tries to open the device file, like
 * "cat /dev/mycharfile"
 */
static int device_open(struct inode *inode, struct file *file) {
  static int counter = 0;
  if (Device_Open)
    return -EBUSY;
  Device_Open++;
  sprintf(msg, "count %d\n", counter++);
  msg_ptr = msg;
  try_module_get(THIS_MODULE);
  return SUCCESS;
}

static int device_release(struct inode *inode, struct file *file) {
  Device_Open--;
  module_put(THIS_MODULE);
  return SUCCESS;
}

/* 
 * Called when a process, which already opened the dev file, attempts to
 * read from it.
 */
static ssize_t device_read(
    struct file *filp, /* see include/linux/fs.h   */
    char *buffer,  /* buffer to fill with data */
    size_t length, /* length of the buffer     */
    loff_t * offset) {
    /*
     * Number of bytes actually written to the buffer 
     */
    int bytes_read = 0;

      /*
       * If we're at the end of the message, return 0 signifying end of file 
       */
      if (*msg_ptr == 0) return 0;

        /* 
         * Actually put the data into the buffer 
         */
        while (length && *msg_ptr) {

              /* 
               * The buffer is in the user data segment, not the kernel 
               * segment so "*" assignment won't work.  We have to use 
               * put_user which copies data from the kernel data segment to
               * the user data segment. 
               */
              put_user(*(msg_ptr++), buffer++);
              length--;
              bytes_read++;
        }

          /* 
           * Most read functions return the number of bytes put into the buffer
           */
          return bytes_read;
}


/*
called when a process writes to dev file: echo "hi" > /dev/hello 
 */
 static ssize_t
 device_write(struct file *filp, const char *buff, size_t len, loff_t * off)
{
    printk("<1>Sorry, this operation isn't supported.\n");
      return -EINVAL;
}
