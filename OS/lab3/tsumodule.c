#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/version.h>

#define PROCFS_NAME "tsulab"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("TSU Student");
MODULE_DESCRIPTION("TSU Lab Kernel Module");

static struct proc_dir_entry *our_proc_file = NULL;

static ssize_t procfile_read(struct file *file_pointer, char __user *buffer,
                             size_t buffer_length, loff_t *offset)
{
    char s[100];
    int len;
    len = snprintf(s, sizeof(s), "Hello from /proc/%s!\n", PROCFS_NAME);


    if (*offset >= len)
        return 0;

    if (copy_to_user(buffer, s, len))
        return -EFAULT;

    *offset += len;
    return len;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static const struct proc_ops proc_file_fops = {
    .proc_read = procfile_read,
};
#else
static const struct file_operations proc_file_fops = {
    .read = procfile_read,
};
#endif

static int __init tsu_module_init(void)
{
    pr_info("Welcome to the Tomsk State University\n");

    our_proc_file = proc_create(PROCFS_NAME, 0644, NULL, &proc_file_fops);
    if (our_proc_file == NULL) {
        pr_alert("Error: Could not initialize /proc/%s\n", PROCFS_NAME);
        return -ENOMEM;
    }

    return 0;
}

static void __exit tsu_module_exit(void)
{
    proc_remove(our_proc_file);

    pr_info("Tomsk State University forever!\n");
}

module_init(tsu_module_init);
module_exit(tsu_module_exit);
