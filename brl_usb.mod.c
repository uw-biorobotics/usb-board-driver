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
	{ 0x8163e13b, "module_layout" },
	{ 0x3d7a4a97, "per_cpu__current_task" },
	{ 0xf9a482f9, "msleep" },
	{ 0x8db3975f, "usb_buffer_alloc" },
	{ 0x9706af16, "malloc_sizes" },
	{ 0x105e2727, "__tracepoint_kmalloc" },
	{ 0xbbdf4c0f, "usb_buffer_free" },
	{ 0x1eeea317, "usb_kill_urb" },
	{ 0x8bb568a2, "usb_deregister_dev" },
	{ 0x4be822e9, "usb_unlink_urb" },
	{ 0x16a4069b, "rt_spin_lock" },
	{ 0x43c5851b, "rt_spin_unlock" },
	{ 0x2878b96c, "usb_string" },
	{ 0x5360dcca, "usb_deregister" },
	{ 0xb72397d5, "printk" },
	{ 0x81cab68c, "usb_register_dev" },
	{ 0xc3aaf0a9, "__put_user_1" },
	{ 0x707f93dd, "preempt_schedule" },
	{ 0x825c0808, "__rt_spin_lock_init" },
	{ 0x7f75a00a, "usb_submit_urb" },
	{ 0xfb684501, "kmem_cache_alloc" },
	{ 0xf0fdf6cb, "__stack_chk_fail" },
	{ 0xd62c833f, "schedule_timeout" },
	{ 0x65f4f085, "usb_find_interface" },
	{ 0xaacf8ce6, "dev_driver_string" },
	{ 0xe3467d7e, "wake_up_process" },
	{ 0x37a0cba, "kfree" },
	{ 0x461d2206, "usb_register_driver" },
	{ 0xd6c963c, "copy_from_user" },
	{ 0x15b81bb4, "usb_free_urb" },
	{ 0x18910a3a, "usb_alloc_urb" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";

MODULE_ALIAS("usb:v04B4p4000d*dc*dsc*dp*ic*isc*ip*");

MODULE_INFO(srcversion, "3B53C607B02CE3A45B7BD81");
