/* ofd.c Our First Driver code */
#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>

static dev_t first;

static struct cdev c_dev; // char device structure

static struct class *cl; // device class

static char c;

static char *kbuf = NULL;
static char *more_kbuf = NULL;

static int my_open(struct inode *i, struct file *f)
{
    printk(KERN_INFO "Driver: open()\n");
      return 0;
}
  static int my_close(struct inode *i, struct file *f)
{
    printk(KERN_INFO "Driver: close()\n");
    //free(more_kbuf);
      return 0;
}

  static ssize_t my_read(struct file *f, char __user *buf, size_t
        len, loff_t *off)
{
    printk(KERN_INFO "Driver: read()\n");
// dst addr in user space
// src addr in kernel space

    if (copy_to_user(buf, &c, 1) != 0)
    //if (copy_to_user(buf, kbuf, len) != 0)
        return -EFAULT;
    else if (*off == 0) {
	*off += 1;
	return 1;
	}
	else {
	return 0;
	}
}

// buf is 
  static ssize_t my_write(struct file *f, const char __user *buf,
        size_t len, loff_t *off)
{
    printk(KERN_INFO "Driver: write()\n");
/*
    if (len > 0) {
      more_kbuf = (char *) realloc (kbuf, sizeof(char) * len);
      if (more_kbuf != NULL) kbuf = more_kbuf;
      else {
         free(kbuf);
         return -EFAULT;
      }
    }
*/
// dst addr in kernel space
// src addr in user space
    if (copy_from_user(&c, buf + len - 1, 1) != 0)
    //if (copy_from_user(kbuf, buf, len) != 0)
        return -EFAULT;
    else
        return len;
}
  static struct file_operations pugs_fops =
{
    .owner = THIS_MODULE,
      .open = my_open,
        .release = my_close,
          .read = my_read,
            .write = my_write
};
 
 
static int __init ofd_init(void) /* Constructor */
{
      printk(KERN_INFO "ofd registered");

      if (alloc_chrdev_region(&first, 0, 3, "shweta") < 0) return -1;

      if ((cl = class_create(THIS_MODULE, "chardrv")) == NULL)
          {
                unregister_chrdev_region(first, 1);
                    return -1;
                      }
          if (device_create(cl, NULL, first, NULL, "mynull") == NULL)
              {
                    class_destroy(cl);
                        unregister_chrdev_region(first, 1);
                            return -1;
                              }
              cdev_init(&c_dev, &pugs_fops);
                  if (cdev_add(&c_dev, first, 1) == -1)
                      {
                            device_destroy(cl, first);
                                class_destroy(cl);
                                    unregister_chrdev_region(first, 1);
                                        return -1;
                                          }

     printk(KERN_INFO "<Major, Minor>: <%d, %d>\n", MAJOR(first), MINOR(first));
          return 0;
}
 
static void __exit ofd_exit(void) /* Destructor */
{
    cdev_del(&c_dev);
      device_destroy(cl, first);
        class_destroy(cl);
      unregister_chrdev_region(first, 3);
      printk(KERN_INFO "ofd unregistered");
}
 
module_init(ofd_init);
module_exit(ofd_exit);
 
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anil Kumar Pugalia <email_at_sarika-pugs_dot_com>");
MODULE_DESCRIPTION("Our First Driver");
