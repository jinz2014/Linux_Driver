#include <linux/module.h>  // for all modules
#include <linux/kernel.h>  // for KERN_ALERT 
#include <linux/init.h>    // for the macros

static short v1 = 1;
static int v2 = 2;
static long v3 = 3;
static char *v4 = "s4";
//static int a1[4];

// module_param (var's name, var's type, perm bits)
// MODULE_PARAM(var's name, var's type) var's type: b,h,i,l,s
module_param(v1, short, 0000);
MODULE_PARM_DESC(v1, "a var of type short");

module_param(v2, int, 0000);
MODULE_PARM_DESC(v2, "a var of type int");

module_param(v3, long, 0000);
MODULE_PARM_DESC(v3, "a var of type long");

module_param(v4, charp, 0000);
MODULE_PARM_DESC(v4, "a var of type char*");

//module_param?
//MODULE_PARM(a1, "2-4i");
//MODULE_PARM_DESC(a1, "an integer array");

static int __init hello_init(void) {
  printk(KERN_ALERT "Hello %hd %d %ld %s\n", v1,v2,v3,v4);
  return 0;
}

static int hello_exit(void) {
  printk(KERN_ALERT "Bye\n");
}

// the macros allow user-defined names (e.g  hello_init)
module_init(hello_init);
module_exit(hello_exit);
