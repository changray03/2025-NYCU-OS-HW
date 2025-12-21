#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/list.h>
#include <linux/pid.h>
#include <linux/sched.h>


enum mode{
    PRINT = 0,
    ADD = 1,
    REMOVE = 2
};

typedef struct my_node{
    char* str;
    struct list_head p;
    pid_t pid;
} N;

static LIST_HEAD(tempbuf_list);

static void clean_up(void){
    N *entry, *tmp;
    list_for_each_entry_safe(entry, tmp, &tempbuf_list, p){
        struct task_struct *task = find_task_by_vpid(entry->pid);
        if(!task){
            list_del(&entry->p);
            kfree(entry->str);
            kfree(entry);
        }
    }
}

SYSCALL_DEFINE3(tempbuf, int, m, char __user*, str, size_t, n){
    clean_up();
    switch(m){
        case PRINT:{
            int len = 0;
            char *buf;
            int count = 0;
            N *entry;
            list_for_each_entry(entry, &tempbuf_list, p) {
                len += strlen(entry->str) + 1;
            }
            if(len == 0){
                if(copy_to_user(str, "", 1)){
                    return -EFAULT;
                }
            }
            len--;
            if(len >= n){
                return -ENOBUFS;
            }
            
            buf = kmalloc(len+1, GFP_KERNEL);
            if(!buf)    return -ENOMEM; 
            buf[0] = '\0';
            list_for_each_entry(entry, &tempbuf_list, p) {
                strcat(buf, entry->str);
                count += strlen(entry->str) + 1;
                if(count == len){
                    strcat(buf, "\0");
                }
                else{
                    strcat(buf, " ");
                }
            }
            buf[len] = '\0';

            printk(KERN_INFO "[tempbuf] %s\n", buf);
            if(copy_to_user(str, buf, len+1)){
                kfree(buf);
                return -EFAULT;
            }
            kfree(buf);
            return len;
        }
        case ADD:{
            N *new_node;
            char *buf;
            if(n == 0){
                return -EFAULT;
            }
            buf = kmalloc(n+1, GFP_KERNEL);
            
            if (!buf)   return -ENOMEM;
            if(copy_from_user(buf, str, n)){
                kfree(buf);
                return -EFAULT;
            }
            buf[n] = '\0';
            new_node = kmalloc(sizeof(*new_node), GFP_KERNEL);
            if(!new_node){
                kfree(buf);
                return -ENOMEM;
            }
            new_node->str = buf;
            new_node->pid = current->pid;
            list_add_tail(&new_node->p, &tempbuf_list);
            printk(KERN_INFO "[tempbuf] Added: %s\n", buf);
            return 0;
        }
        case REMOVE:{
            char *buf;
            N *entry, *tmp;
            int remove = 0;
            if(n == 0){
                return -EFAULT;
            }
            buf = kmalloc(n+1, GFP_KERNEL);

            if (!buf)   return -ENOMEM;
            if(copy_from_user(buf, str, n)){
                kfree(buf);
                return -EFAULT;
            }
            buf[n] = '\0';
            list_for_each_entry_safe(entry, tmp, &tempbuf_list, p) {
                if (strcmp(entry->str, buf) == 0) {
                    printk(KERN_INFO "[tempbuf] Removed: %s\n", buf);
                    list_del(&entry->p);
                    kfree(entry->str);
                    kfree(entry);
                    remove++;
                    break;
                }
            }
            kfree(buf);
            if(remove > 0) {
                return 0;
            } else {
                return -ENOENT;
            }
        }
    }

    return -EINVAL; 
}