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
int     scull_ioctl (struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg);

/*
 * Ioctl definitions
 */

/* Use 'k' as magic number */
#define SCULL_IOC_MAGIC  'k'

#define SCULL_IOCRESET    _IO(SCULL_IOC_MAGIC, 0)

/*
 * S means "Set" through a ptr,
 * T means "Tell" directly with the argument value
 * G means "Get": reply by setting through a pointer
 * Q means "Query": response is on the return value
 * X means "eXchange": G and S atomically
 * H means "sHift": T and Q atomically
 */
#define SCULL_IOCSQUANTUM _IOW(SCULL_IOC_MAGIC,  1, scull_quantum)
#define SCULL_IOCSQSET    _IOW(SCULL_IOC_MAGIC,  2, scull_qset)
#define SCULL_IOCTQUANTUM _IO(SCULL_IOC_MAGIC,   3)
#define SCULL_IOCTQSET    _IO(SCULL_IOC_MAGIC,   4)
#define SCULL_IOCGQUANTUM _IOR(SCULL_IOC_MAGIC,  5, scull_quantum)
#define SCULL_IOCGQSET    _IOR(SCULL_IOC_MAGIC,  6, scull_qset)
#define SCULL_IOCQQUANTUM _IO(SCULL_IOC_MAGIC,   7)
#define SCULL_IOCQQSET    _IO(SCULL_IOC_MAGIC,   8)
#define SCULL_IOCXQUANTUM _IOWR(SCULL_IOC_MAGIC, 9, scull_quantum)
#define SCULL_IOCXQSET    _IOWR(SCULL_IOC_MAGIC,10, scull_qset)
#define SCULL_IOCHQUANTUM _IO(SCULL_IOC_MAGIC,  11)
#define SCULL_IOCHQSET    _IO(SCULL_IOC_MAGIC,  12)

/*
 * The other entities only have "Tell" and "Query", because they're
 * not printed in the book, and there's no need to have all six.
 * (The previous stuff was only there to show different ways to do it.
 */
#define SCULL_P_IOCTSIZE _IO(SCULL_IOC_MAGIC,   13)
#define SCULL_P_IOCQSIZE _IO(SCULL_IOC_MAGIC,   14)
/* ... more to come */
#define SCULL_IOCHARDRESET _IO(SCULL_IOC_MAGIC, 15) /* debugging tool */

#define SCULL_IOC_MAXNR 15


