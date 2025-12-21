#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

SYSCALL_DEFINE2(revstr, char __user *, str, size_t, n){
    char* buf = kmalloc(n+1, GFP_KERNEL);
    size_t i;
    if(copy_from_user(buf, str, n)){
        return -EFAULT;
    }
    buf[n] = '\0';
    printk(KERN_INFO "The origin string: %s\n", buf);
    for(i = 0; i < n/2; i++){
        char tmp = buf[i];
        buf[i] = buf[n-i-1];
        buf[n-i-1] = tmp;
    }
    printk(KERN_INFO "The reversed string: %s\n", buf);
    if(copy_to_user(str, buf, n)){
        return -EFAULT;
    }
    kfree(buf);
    return 0;
}