#include <linux/module.h>
#include <linux/export-internal.h>
#include <linux/compiler.h>

MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};



static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x581a7cd, "single_open" },
	{ 0x8d522714, "__rcu_read_lock" },
	{ 0xd2450a5e, "init_task" },
	{ 0x2469810f, "__rcu_read_unlock" },
	{ 0x4dfa8d4b, "mutex_lock" },
	{ 0x3213f038, "mutex_unlock" },
	{ 0x2529ebe1, "seq_printf" },
	{ 0xca25e87b, "proc_remove" },
	{ 0xf99f317e, "seq_read" },
	{ 0x993ee334, "seq_lseek" },
	{ 0x768bc98e, "single_release" },
	{ 0xbdfb6dbb, "__fentry__" },
	{ 0xdc679947, "proc_create" },
	{ 0x92997ed8, "_printk" },
	{ 0x5b8239ca, "__x86_return_thunk" },
	{ 0x5002fa32, "module_layout" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "436C3E515FF495573B95192");
