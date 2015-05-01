// dynamic major number allocation
#define SCULL_MAJOR 0    

// scull0 to scull3
#define SCULL_NR_DEVS 4  

// The device is modelled as if it were a chained memory 
typedef struct Scull_Dev {
  void **data;
  struct Scull_Dev *next;
  int quantum;
  int qset;
  unsigned long size;
  struct semaphore sem;
} Scull_Dev;

// Scull_Dev->data points to an array of pointers. 
// The array is SCULL_QSET long. Each pointer refers
// to a memory area of SCULL_QUANTUM bytes. 
#define SCULL_QSET 10
#define SCULL_QUANTUM 40

#define TYPE(dev)  (MINOR(dev) >> 4)
#define NUM(dev)   (MINOR(dev) & 0xF)

extern struct file_operations scull_fops;        /* simplest: global */

/*
 * The different configurable parameters
 */
extern int scull_major;     /* main.c */
extern int scull_nr_devs;
extern int scull_quantum;
extern int scull_qset;

static void __exit scull_cleanup_module(void);
int     scull_trim(Scull_Dev *dev);
ssize_t scull_read(struct file *filp, char *buf, size_t count, loff_t *f_pos);
ssize_t scull_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos);

