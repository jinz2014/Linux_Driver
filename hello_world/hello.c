#include <linux/module.h>  // for all modules
#include <linux/kernel.h>  // for KERN_ALERT 
#include <linux/init.h>    // for the macros

static int data __initdata = 3;

static int __init hello_init(void) {
  printk(KERN_ALERT "Hello %d\n", data);
  return 0;
}

static int __exit hello_exit(void) {
  printk(KERN_ALERT "Bye\n");
}

// the macros allow user-defined names (e.g  hello_init)
module_init(hello_init);
module_exit(hello_exit);
