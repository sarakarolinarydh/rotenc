#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <linux/of_platform.h>
#include <linux/uaccess.h>

MODULE_DESCRIPTION("Rotary Encoder Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sara Rydh");

#define DEVICE_DRIVER_NAME "rotenc"
#define BUF 12

// Module, init and exit
static int __init rotenc_init(void);
static void __exit rotenc_exit(void);

// Probe
static int dt_probe(struct platform_device *pdev);
static int dt_remove(struct platform_device *pdev);
static irqreturn_t irq_handler_ch_a(int irq, void *lp);
static irqreturn_t irq_handler_sw(int irq, void *lp);

// Device
static struct class *device_class;
static struct device *device_struct;
static int device_number;
static int device_file_major_number = 0;

// File operations
static ssize_t rotenc_read(struct file *filp, char __user *buf, size_t len, loff_t *off);
static ssize_t rotenc_open(struct inode *inode, struct file *file);
static int rotenc_release(struct inode *inode, struct file *file);

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .read = rotenc_read,
    .open = rotenc_open,
    .release = rotenc_release,
};

// GPIOs
static struct gpio_desc *ch_a = NULL;
static struct gpio_desc *ch_b = NULL;
static struct gpio_desc *sw = NULL;

// IRQ
int irq_ch_a = -1;
int irq_sw = -1;

// Rotary encoder value
static int pos = 0;

static ssize_t rotenc_read(struct file *filp, char __user *buf, size_t len, loff_t *off) {
    
    char spos[BUF];
    snprintf(spos, BUF, "%d\n", pos);
    int bytes_read = 0;
    const char *data_ptr = spos;

    if (!*(data_ptr + *off)) {
        *off = 0;
        return 0;
    }

    data_ptr += *off;

    while (len && *data_ptr) {
        put_user(*(data_ptr++), buf++);
        len--; 
        bytes_read++;
    }

    *off += bytes_read;

    return bytes_read;
}

static ssize_t rotenc_open(struct inode *inode, struct file *file) {
    pr_info("rotenc: Hello from open\n");
    try_module_get(THIS_MODULE);
    return 0;
}

static int rotenc_release(struct inode *inode, struct file *file) { 
    pr_info("rotenc: Hello from release\n");
    module_put(THIS_MODULE);

    return 0;
}

static irqreturn_t irq_handler_ch_a(int irq, void *lp) {
    gpiod_get_value(ch_b) > 0 ? pos++ : pos--;
    pr_info("rotenc: pos: %d\n", pos);

    return IRQ_HANDLED;
}

static irqreturn_t irq_handler_sw(int irq, void *lp) {
    // Reset the rotary encoder value
    pos = 0;
    pr_info("rotenc: pos: %d\n", pos);

    return IRQ_HANDLED;
}

static int dt_probe(struct platform_device *pdev) {
    //  Register the character device
    int res = 0;
    res = register_chrdev(0, DEVICE_DRIVER_NAME, &fops);
    if (res < 0) {
        pr_err("rotenc: Could not allocate major number");
        return -1;
    }
    device_file_major_number = res;

    // Create class
    device_class = class_create(THIS_MODULE, DEVICE_DRIVER_NAME);
    if (IS_ERR(device_class)) {
        pr_err("rotenc: Can not create device class, error code = %i\n", PTR_ERR(device_class));
        goto r_unreg;
    }

    // Create device
    device_number = MKDEV(device_file_major_number, 0);
    device_struct = device_create(device_class, NULL, device_number, NULL, DEVICE_DRIVER_NAME);
    if(IS_ERR(device_struct)) {
        pr_err("rotenc: Can not create device, error code = %i\n", device_struct);
        goto r_class;
    }

    // Rotary encoder, channel A
    res = 0;
    res = gpiod_direction_input(ch_a);
    if (res < 0) {
        pr_err("rotenc: Error setting direction for GPIO ch_a");
        goto r_device;
    }

    // Rotary encoder, SW
    res = 0;
    res = gpiod_direction_input(sw);
    if (res < 0) {
        pr_err("rotenc: Error setting direction for GPIO sw");
        goto r_device;
    }

    // Rotary encoder, channel B
    gpiod_direction_input(ch_b);
    ch_b = gpiod_get(&pdev->dev, "ch_b", GPIOD_IN);
    if (IS_ERR(ch_b)) {
        pr_err("rotenc: Failed to setup channel B\n");
        goto r_device;
    }

    // IRQ channel A
    irq_ch_a = platform_get_irq(pdev, 0);
    if (irq_ch_a < 0) {
        pr_err("rotenc: Error reading the IRQ number %d", irq_ch_a);
        goto r_cb;
    }

    res = 0;
    res = request_irq(irq_ch_a, irq_handler_ch_a, IRQF_SHARED, DEVICE_DRIVER_NAME, &pdev->dev);
    if (res < 0) {
        pr_err("rotenc: Can not connect irq %d error: %d\n", irq_ch_a, res);
        goto r_cb;
    }

    // IRQ SW
    irq_sw = platform_get_irq(pdev, 1);
    if (irq_sw < 0) {
        pr_err("rotenc: Error reading the IRQ number %d", irq_sw);
        goto r_irq_ch_a;
    }

    res = 0;
    res = request_irq(irq_sw, irq_handler_sw, IRQF_SHARED, DEVICE_DRIVER_NAME, &pdev->dev);
    if (res < 0) {
        pr_err("rotenc: Can not connect irq %d error: %d\n", irq_sw, res);
        goto r_irq_ch_a;
    }

   pr_info("rotenc: Initialization completed\n");
   return 0;

r_irq_ch_a:
    free_irq(irq_ch_a, &pdev->dev);
r_cb: 
    gpiod_put(ch_b);
r_device:
    device_destroy(device_class, device_struct->devt);
r_class:
    class_destroy(device_class);
r_unreg:
    unregister_chrdev(device_file_major_number, DEVICE_DRIVER_NAME);

    return -1;
}

static int dt_remove(struct platform_device *pdev) {
    pr_info("rotenc: Removing device from device tree\n");
    free_irq(irq_sw, &pdev->dev);
    free_irq(irq_ch_a, &pdev->dev);
    gpiod_put(ch_b);
    device_destroy(device_class, device_struct->devt);
    class_destroy(device_class);
    unregister_chrdev(device_file_major_number, DEVICE_DRIVER_NAME);

    return 0;
}

static struct of_device_id rotenc_of_match[] = {
    {
        .compatible = "rotenc,rotenc",
    },
    {},
};

static struct platform_driver rotenc = {
    .probe = dt_probe,
    .remove = dt_remove,
    .driver = {
        .name = DEVICE_DRIVER_NAME,
        .owner = THIS_MODULE,
        .of_match_table = rotenc_of_match,
    },
};

static int __init rotenc_init(void) {
    pr_info("rotenc: Initialization started\n");
    return platform_driver_register(&rotenc);
}

static void __exit rotenc_exit(void) {
    pr_info("rotenc: Exiting\n");
    return platform_driver_unregister(&rotenc);
}

module_init(rotenc_init);
module_exit(rotenc_exit);
