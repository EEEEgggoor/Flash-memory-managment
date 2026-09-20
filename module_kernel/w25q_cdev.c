#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/spi/spi.h>
#include <linux/of.h>

//SPI
#include <linux/spi/spi.h>

#include "w25q_ioctl.h"


#define DEVICE_NAME "W25Q64FV"
#define BUF_SIZE 4096

#define CHECK_FLASH 0x9F

#define WRITE_ENABLE 0x06
#define PAGE_PROGRAM 0x02
#define SECTOR_ERASE 0x20
#define READ_DATA 0x03
#define READ_STATUS 0x05
#define CHIP_ERASE 0xC7
#define WRITE_STATUS_REG 0x01


#define W25Q_EXPECTED_MFR_ID   0xEF
#define W25Q_EXPECTED_MEM_TYPE 0x40


struct w25q {
    struct spi_device *spi;
    dev_t dev_num;
    struct cdev flash_cdev;
    struct class *flash_class;
    struct device *flash_device;
    char *kernel_buffer;
    size_t data_size;
    struct mutex lock;
    u32 pending_addr;
    u32 pending_len;

    bool result_ready;
};


struct class *w25q_class;

static int w25q_cmd(struct w25q *priv, u8 opcode, u32 addr, bool use_addr,
                     const u8 *tx_data, size_t tx_len,
                     u8 *rx_data, size_t rx_len);



static int dev_open(struct inode *inode, struct file *file)
{
    struct w25q *priv = container_of(inode->i_cdev, struct w25q, flash_cdev);

    if(!mutex_trylock(&(priv->lock))){ return -EBUSY; }
    printk(KERN_INFO "w25q: opened\n");
    file->private_data = priv;
    return 0;
}


static int dev_release(struct inode *inode, struct file *file)
{
    struct w25q *priv = file->private_data;
    mutex_unlock(&priv->lock);
    printk(KERN_INFO "w25q: closed\n");
    return 0;
}


static ssize_t dev_read(struct file *file, char __user *user_buf,
                        size_t len, loff_t *off)
{
    struct w25q *priv = file->private_data;
    size_t len_data;
    int ret;

    if (priv->result_ready) {
        len_data = priv->data_size;
        priv->result_ready = false;
    } else {
        len_data = priv->pending_len;
        if (len_data == 0 || len_data > BUF_SIZE)
            return -EINVAL;

        ret = w25q_cmd(priv, READ_DATA, priv->pending_addr, true,
                        NULL, 0, priv->kernel_buffer, len_data);
        if (ret < 0)
            return ret;
    }

    if (len < len_data)
        return -EINVAL;

    if (copy_to_user(user_buf, priv->kernel_buffer, len_data))
        return -EFAULT;


        
    *off = 0;
    return len_data;
}

static ssize_t dev_write(struct file *file, const char __user *user_buf,
                        size_t len, loff_t *off)
{
    struct w25q *priv = file->private_data;
    u8 opcode;
    int ret;

    if(len < 1) { return -EINVAL; }

    size_t to_copy = min(len, (size_t)(BUF_SIZE));

    if(copy_from_user(priv->kernel_buffer, user_buf, to_copy)){ return -EFAULT; }

    opcode = priv->kernel_buffer[0];

    switch (opcode) {
    case WRITE_ENABLE:
        priv->result_ready = false;

        ret = w25q_cmd(priv, WRITE_ENABLE, 0, false, NULL, 0, NULL, 0);
        priv->result_ready = false;

        break;

    case CHIP_ERASE:
        priv->result_ready = false;

        ret = w25q_cmd(priv, WRITE_ENABLE, 0, false, NULL, 0, NULL, 0);
        if (ret < 0)
            break;
        ret = w25q_cmd(priv, CHIP_ERASE, 0, false, NULL, 0, NULL, 0);

        break;

    case SECTOR_ERASE: {
        priv->result_ready = false;

        u32 addr;

        if (len < 1 + 3)
            return -EINVAL;

        addr = (priv->kernel_buffer[1] << 16) |
               (priv->kernel_buffer[2] << 8)  |
                priv->kernel_buffer[3];

        ret = w25q_cmd(priv, WRITE_ENABLE, 0, false, NULL, 0, NULL, 0);
        if (ret < 0)
            break;
        ret = w25q_cmd(priv, SECTOR_ERASE, addr, true, NULL, 0, NULL, 0);
        priv->result_ready = false;

        break;
    }

    case PAGE_PROGRAM: {
        priv->result_ready = false;

        u32 addr;
        const u8 *payload = priv->kernel_buffer + 4;
        size_t payload_len = len - 4;

        if (len < 1 + 3 + 1)
            return -EINVAL;

        addr = (priv->kernel_buffer[1] << 16) |
               (priv->kernel_buffer[2] << 8)  |
                priv->kernel_buffer[3];

        ret = w25q_cmd(priv, WRITE_ENABLE, 0, false, NULL, 0, NULL, 0);
        if (ret < 0)
            break;
        ret = w25q_cmd(priv, PAGE_PROGRAM, addr, true,
                        payload, payload_len, NULL, 0);
        priv->result_ready = false;

        break;
    }

    case CHECK_FLASH: {
        priv->result_ready = true;

        u8 id[3];
        ret = w25q_cmd(priv, CHECK_FLASH, 0, false, NULL, 0, id, 3);
        if (ret < 0)
            break;

        memcpy(priv->kernel_buffer, id, sizeof(id));
        priv->data_size = sizeof(id);

        break;
    }

    case READ_STATUS: {
        u8 status;
        priv->result_ready = true;

        ret = w25q_cmd(priv, READ_STATUS, 0, false, NULL, 0, &status, 1);
        if (ret < 0) {
            priv->result_ready = false;
            break;
        }

        priv->kernel_buffer[0] = status;
        priv->data_size = 1;
        break;
    }
    case WRITE_STATUS_REG: {
        u8 new_status = 0x00;
        priv->result_ready = false;

        ret = w25q_cmd(priv, WRITE_ENABLE, 0, false, NULL, 0, NULL, 0);
        if (ret < 0)
            break;

        ret = w25q_cmd(priv, WRITE_STATUS_REG, 0, false, &new_status, 1, NULL, 0);
        break;
    }
    default:
        return -EINVAL;
    }

    if (ret < 0)
        return ret;

    return len;
}


static long dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct w25q *priv = file->private_data;
    struct w25q_read_req req;

    switch (cmd) {
    case W25Q_READ_DATA:
        if (copy_from_user(&req, (void __user *)arg, sizeof(req)))
            return -EFAULT;

        if (req.len == 0 || req.len > BUF_SIZE)
            return -EINVAL;

        priv->pending_addr = req.addr;
        priv->pending_len  = req.len;
        return 0;

    default:
        return -ENOTTY;
    }
}


static const struct file_operations fops = {
    .owner      = THIS_MODULE,
    .open       = dev_open,
    .release    = dev_release,
    .read       = dev_read,
    .write      = dev_write,
    .unlocked_ioctl = dev_ioctl,
};


static int w25q_probe(struct spi_device *spi)
{
    struct w25q *priv;
    u8 id[3];
    int ret;

    priv = devm_kzalloc(&spi->dev, sizeof(*priv), GFP_KERNEL);
    if(!priv) { return -ENOMEM; }

    priv->spi = spi;
    priv->flash_class = w25q_class;
    mutex_init(&priv->lock);
    spi_set_drvdata(spi, priv);

    spi->mode = SPI_MODE_1;
    spi->bits_per_word = 8;
    if(spi_setup(spi) < 0){
        dev_err(&spi->dev, "spi_setup fail\n");
        return -EIO;
    }

    priv->kernel_buffer = devm_kmalloc(&spi->dev, BUF_SIZE, GFP_KERNEL);
    if(!priv->kernel_buffer) { return -ENOMEM; }

    
    ret = w25q_cmd(priv, CHECK_FLASH, 0, false, NULL, 0, id, sizeof(id));
    if (ret < 0) {
        dev_err(&spi->dev, "CHECK_FLASH failed: %d\n", ret);
        return ret;
    }

    dev_info(&spi->dev, "JEDEC ID: mfr=0x%02x type=0x%02x capacity=0x%02x\n",
              id[0], id[1], id[2]);

    if (id[0] != W25Q_EXPECTED_MFR_ID || id[1] != W25Q_EXPECTED_MEM_TYPE) {
        dev_err(&spi->dev, "unexpected flash chip (mfr=0x%02x type=0x%02x), aborting probe\n",
                 id[0], id[1]);
        return -ENODEV;
    }


    int ret_alloc_chrdev = alloc_chrdev_region(&priv->dev_num, 0, 1, DEVICE_NAME);

    if(ret_alloc_chrdev < 0){ 
        return ret_alloc_chrdev;
    }

    cdev_init(&priv->flash_cdev, &fops);
    priv->flash_cdev.owner = THIS_MODULE;

    ret = cdev_add(&priv->flash_cdev, priv->dev_num, 1);
    if (ret < 0) {
        unregister_chrdev_region(priv->dev_num, 1);
        return ret;
    }

    priv->flash_device = device_create(priv->flash_class, NULL, priv->dev_num, NULL, DEVICE_NAME);
    if (IS_ERR(priv->flash_device)) {
        cdev_del(&priv->flash_cdev);
        unregister_chrdev_region(priv->dev_num, 1);
        return PTR_ERR(priv->flash_device);
    }


    dev_info(&spi->dev, "probed, major=%d minor=%d\n",
              MAJOR(priv->dev_num), MINOR(priv->dev_num));

    return 0;

}



static void w25q_remove(struct spi_device *spi)
{
    struct w25q *priv = spi_get_drvdata(spi);

    device_destroy(priv->flash_class, priv->dev_num);
    cdev_del(&priv->flash_cdev);
    unregister_chrdev_region(priv->dev_num, 1);


        dev_info(&spi->dev, "w25q removed");
}

static const struct of_device_id w25q_of_match[] = {
    { .compatible = "egor,w25q32fv" },
    { }
};

static const struct spi_device_id w25q_spi_id[] = {
    { "w25q32fv", 0 },
    { }
};
MODULE_DEVICE_TABLE(spi, w25q_spi_id);

static struct spi_driver w25q_spi_driver = {
    .driver = {
        .name = DEVICE_NAME,
        .of_match_table = w25q_of_match,
    },
    .probe = w25q_probe,
    .remove = w25q_remove,
    .id_table = w25q_spi_id,
};


static int __init w25q_module_init(void)
{
    int ret;

    w25q_class = class_create(DEVICE_NAME);
    if(IS_ERR(w25q_class)){
        return PTR_ERR(w25q_class);
    }

    ret = spi_register_driver(&w25q_spi_driver);
    if(ret < 0){
        class_destroy(w25q_class);
        return ret;
    }


    return 0;
}


static void __exit w25q_module_exit(void)
{
    spi_unregister_driver(&w25q_spi_driver);
    class_destroy(w25q_class);
}


static int w25q_cmd(struct w25q *priv, u8 opcode, u32 addr, bool use_addr,
                     const u8 *tx_data, size_t tx_len,
                     u8 *rx_data, size_t rx_len)
{
    struct spi_transfer xfer[2] = {0};
    struct spi_message msg;
    u8 cmd[4];              

    int cmd_len;
    int n_xfer = 0;

    cmd[0] = opcode;
    if (use_addr) {
        cmd[1] = (addr >> 16) & 0xFF;
        cmd[2] = (addr >> 8) & 0xFF;
        cmd[3] = addr & 0xFF;
        cmd_len = 4;
    } else {
        cmd_len = 1;
    }

    xfer[n_xfer].tx_buf = cmd;
    xfer[n_xfer].len = cmd_len;
    n_xfer++;

    if (tx_data && tx_len) {
        xfer[n_xfer].tx_buf = tx_data;
        xfer[n_xfer].len = tx_len;
        n_xfer++;
    } else if (rx_data && rx_len) {
        xfer[n_xfer].rx_buf = rx_data;
        xfer[n_xfer].len = rx_len;
        n_xfer++;
    }

    spi_message_init(&msg);
    spi_message_add_tail(&xfer[0], &msg);
    if (n_xfer > 1)
        spi_message_add_tail(&xfer[1], &msg);

    return spi_sync(priv->spi, &msg);
}


module_init(w25q_module_init);
module_exit(w25q_module_exit);


MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("W25Q32FV SPI flash char device driver");