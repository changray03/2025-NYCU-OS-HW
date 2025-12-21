#include <linux/module.h>
#include <linux/kernel.h>
//#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/utsname.h>
#include <linux/mm.h>
#include <linux/cpumask.h>
#include <linux/sched/signal.h>
#include <linux/timekeeping.h>
#include <linux/version.h>
#include <linux/mutex.h>


#define KFETCH_DEV_NAME "kfetch"
#define KFETCH_DEV_PATH "/dev/kfetch"
#define KFETCH_BUF_SIZE 1024
#define KFETCH_NUM_INFO 6
#define KFETCH_RELEASE   (1 << 0)
#define KFETCH_NUM_CPUS  (1 << 1)
#define KFETCH_CPU_MODEL (1 << 2)
#define KFETCH_MEM       (1 << 3)
#define KFETCH_UPTIME    (1 << 4)
#define KFETCH_NUM_PROCS (1 << 5)

#define KFETCH_FULL_INFO ((1 << KFETCH_NUM_INFO) - 1)

static int major;
static struct class *kfetch_class;
static struct cdev kfetch_cdev;
static dev_t dev;

static int kfetch_mask = KFETCH_FULL_INFO;
static DEFINE_MUTEX(kfetch_mutex);

static const char *logo_lines[] = {
    "        .-.        ",
    "       (.. |       ",
    "       <>  |       ",
    "      / --- \\      ",
    "     ( |   | |     ",
    "   |\\\\_)___/\\)/\\   ",
    "  <__)------(__/   ",
    NULL
};

static int kfetch_open(struct inode *inode, struct file *file)
{
    mutex_lock(&kfetch_mutex);
    return 0;
}

static int kfetch_release(struct inode *inode, struct file *file)
{
    mutex_unlock(&kfetch_mutex);
    return 0;
}

static ssize_t kfetch_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    int mask_info;

    if (count != sizeof(int)) {
        return -EINVAL;
    }

    if (copy_from_user(&mask_info, buf, count)) {
        return -EFAULT;
    }

    kfetch_mask = mask_info;

    return count;
}

static ssize_t kfetch_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    char *kbuf;
    int len = 0;
    
    struct sysinfo si;
    struct timespec64 uptime;
    struct task_struct *task;
    int num_procs = 0;
    
    char info_lines[8][64]; 
    int idx = 0;

    if (*ppos > 0)
        return 0;

    kbuf = kmalloc(KFETCH_BUF_SIZE, GFP_KERNEL);
    if (!kbuf)
        return -ENOMEM;

    memset(info_lines, 0, sizeof(info_lines));

    /* --- 1. Hostname --- */
    snprintf(info_lines[idx++], 64, "%s", utsname()->nodename);
    
    /* --- 2. Separator --- */
    {
        int hostname_len = strlen(utsname()->nodename);
        int i;
        for (i = 0; i < hostname_len && i < 11; i++) {
            info_lines[idx][i] = '-';
        }
        info_lines[idx][i] = '\0';
        idx++;
    }

    /* --- 3. Info Fetching --- */
    if (kfetch_mask & KFETCH_RELEASE) {
        snprintf(info_lines[idx++], 64, "Kernel: %s", utsname()->release);
    }

    if (kfetch_mask & KFETCH_CPU_MODEL) {
        snprintf(info_lines[idx++], 64, "CPU: RISC-V Processor");
        // snprintf(info_lines[idx++], 64, "CPU: %s", utsname()->machine);
    }

    if (kfetch_mask & KFETCH_NUM_CPUS) {
        snprintf(info_lines[idx++], 64, "CPUs: %d / %d", num_online_cpus(), num_possible_cpus());
    }

    if (kfetch_mask & KFETCH_MEM) {
        si_meminfo(&si);
        unsigned long total_mem_mb = (si.totalram * si.mem_unit) / 1024 / 1024; //si.totalram * si.mem_unit is the total bytes of RAM
        unsigned long free_mem_mb = (si.freeram * si.mem_unit) / 1024 / 1024;   //si.freeram * si.mem_unit is the total free bytes in RAM
        snprintf(info_lines[idx++], 64, "Mem: %lu MB / %lu MB", free_mem_mb, total_mem_mb);
    }

    if (kfetch_mask & KFETCH_NUM_PROCS) {
        rcu_read_lock();
        for_each_process(task) {
            num_procs++;
        }
        rcu_read_unlock();
        snprintf(info_lines[idx++], 64, "Procs: %d", num_procs);
    }

    if (kfetch_mask & KFETCH_UPTIME) {
        ktime_get_boottime_ts64(&uptime);
        snprintf(info_lines[idx++], 64, "Uptime: %lld mins", uptime.tv_sec / 60);
    }

    len += snprintf(kbuf + len, KFETCH_BUF_SIZE - len, "%s  %s\n", "                   ", info_lines[0]);
    /* --- 4. Formatting --- */
    {
        int i;
        int max_lines = (idx > 7) ? idx : 7; 
        
        for (i = 0; i < max_lines; i++) {
            const char *logo_str = (i < 7) ? logo_lines[i] : "                    ";
            const char *info_str = (i < idx) ? info_lines[i+1] : "";
            
            len += snprintf(kbuf + len, KFETCH_BUF_SIZE - len, "%s  %s\n", logo_str, info_str);
            
            if (len >= KFETCH_BUF_SIZE) break; 
        }
    }
    
    kbuf[len] = '\0';

    /* --- 5. Copy to user --- */
    if (count < len) len = count;
    
    if (copy_to_user(buf, kbuf, len)) {
        kfree(kbuf);
        return -EFAULT;
    }

    *ppos += len;
    
    /* 成功時也要釋放 kbuf */
    kfree(kbuf);
    
    return len;
}

static const struct file_operations kfetch_ops = {
    .owner   = THIS_MODULE,
    .open    = kfetch_open,
    .release = kfetch_release,
    .read    = kfetch_read,
    .write   = kfetch_write,
};

static int __init kfetch_mod_init(void)
{
    int ret;

    // 1. Alloc Region
    ret = alloc_chrdev_region(&dev, 0, 1, KFETCH_DEV_NAME);
    if (ret < 0) {
        pr_err("%s: Failed to allocate char device region\n", KFETCH_DEV_NAME);
        return ret;
    }
    major = MAJOR(dev);

    // 2. Init Cdev
    cdev_init(&kfetch_cdev, &kfetch_ops);

    // 3. Add Cdev
    ret = cdev_add(&kfetch_cdev, dev, 1);
    if (ret < 0) {
        pr_err("%s: Failed to add cdev\n", KFETCH_DEV_NAME);
        unregister_chrdev_region(dev, 1);
        return ret;
    }

    // 4. Create Class
    kfetch_class = class_create(KFETCH_DEV_NAME);

    if (IS_ERR(kfetch_class)) {
        pr_err("%s: Failed to create class\n", KFETCH_DEV_NAME);
        ret = PTR_ERR(kfetch_class);
        cdev_del(&kfetch_cdev);
        unregister_chrdev_region(dev, 1);
        return ret;
    }

    // 5. Create Device
    if (device_create(kfetch_class, NULL, dev, NULL, KFETCH_DEV_NAME) == NULL) {
        pr_err("%s: Failed to create device file\n", KFETCH_DEV_NAME);
        class_destroy(kfetch_class);
        cdev_del(&kfetch_cdev);
        unregister_chrdev_region(dev, 1);
        return -1;
    }

    pr_info("%s: Module loaded successfully. Major: %d\n", KFETCH_DEV_NAME, major);
    return 0;
}
static void __exit kfetch_mod_exit(void)
{
    device_destroy(kfetch_class, dev);
    class_destroy(kfetch_class);
    cdev_del(&kfetch_cdev);
    unregister_chrdev_region(dev, 1);
    pr_info("%s: Module unloaded\n", KFETCH_DEV_NAME);
}
module_init(kfetch_mod_init);
module_exit(kfetch_mod_exit);

MODULE_LICENSE("GPL");
