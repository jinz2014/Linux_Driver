#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
 .name = KBUILD_MODNAME,
 .init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
 .exit = cleanup_module,
#endif
 .arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0x14522340, "module_layout" },
	{ 0x6bc3fbc0, "__unregister_chrdev" },
	{ 0x5a34a45c, "__kmalloc" },
	{ 0x6980fe91, "param_get_int" },
	{ 0xd691cba2, "malloc_sizes" },
	{ 0xfc4f55f3, "down_interruptible" },
	{ 0xfa0d49c7, "__register_chrdev" },
	{ 0xff964b25, "param_set_int" },
	{ 0x9629486a, "per_cpu__cpu_number" },
	{ 0xde0bdcff, "memset" },
	{ 0xea147363, "printk" },
	{ 0xacdeb154, "__tracepoint_module_get" },
	{ 0x85f8a266, "copy_to_user" },
	{ 0xb4390f9a, "mcount" },
	{ 0xcbd81171, "module_put" },
	{ 0x2044fa9e, "kmem_cache_alloc_trace" },
	{ 0x32047ad5, "__per_cpu_offset" },
	{ 0x37a0cba, "kfree" },
	{ 0x3f1899f1, "up" },
	{ 0x3302b500, "copy_from_user" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "799EABF5808A99FC8771DE0");

static const struct rheldata _rheldata __used
__attribute__((section(".rheldata"))) = {
	.rhel_major = 6,
	.rhel_minor = 5,
};
