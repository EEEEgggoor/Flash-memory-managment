#include <linux/ioctl.h>
#include <linux/types.h> 

struct w25q_read_req {
    __u32 addr;
    __u32 len;
};


#define W25Q_IOC_MAGIC 'w'
#define W25Q_READ_DATA _IOW(W25Q_IOC_MAGIC, 1, struct w25q_read_req)