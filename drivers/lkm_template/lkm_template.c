#include <linux/module.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/version.h>
#include <linux/uaccess.h>

// require KPROBES when building as MODULE
#ifdef MODULE
#ifndef CONFIG_KPROBES
#error "Building as LKM requires KPROBES."
#endif
#endif

#ifdef CONFIG_KPROBES
#include <linux/kprobes.h>
#include "arch.h"
#define HANDLER_TYPE static int
#else
#define HANDLER_TYPE int // to allow extern def
#endif

// SYSCALL_DEFINE4(reboot, int, magic1, int, magic2, unsigned int, cmd,
//		void __user *, arg)
// lkm_handle_sys_reboot(magic1, magic2, cmd, arg);
// PLAN
// magic1 main magic
// magic2 command
// arg, data input


struct basic_payload {
	uint64_t reply_ptr;
	char text[256];
};

#define DEF_MAGIC 0x12345678
#define UNLOAD 0
#define PRINT_ARG 1
#define PRINT_ARG_FROM_STRUCT 2
#define PRINT_ARGS_FROM_ARRAY 3


// always use u64 for pointers regardless, this way we wont have any
// issues like that nasty 32-on-64 pointer mismatch.

// for manual hook, make sure to pass arg address
// template_handle_sys_reboot(magic1, magic2, cmd, &arg);
// ^ like that
HANDLER_TYPE template_handle_sys_reboot(int magic1, int magic2, unsigned int cmd, void __user **arg)
{
	int ok = DEF_MAGIC; 

	// grab a copy as we write the pointer on the pointer
	u64 reply = (u64)*arg;	

	if (magic1 != ok)
		return 0;

	pr_info("LKM: intercepted call! magic: 0x%x id: %d\n", magic1, magic2);

	if (magic2 == PRINT_ARG) {
		char buf[256] = {0};
		if (copy_from_user(buf, (const char __user *)*arg, 256))
			return 0;

		buf[255] = '\0';
		pr_info("LKM: print %s\n", buf);

		if (copy_to_user((void __user *)*arg, &reply, sizeof(reply)))
			return 0;
	}
	
	if (magic2 == PRINT_ARG_FROM_STRUCT) {
		struct basic_payload basic = {0};
		// make sure to dereference arg either in-use or on entry
		if (copy_from_user(&basic, (void __user *)*arg, sizeof basic))
			return 0;

		basic.text[255] = '\0';
		pr_info("LKM: print %s\n", basic.text);

		// now since we have a dedicated ptr for reply, write to that ptr
		if (copy_to_user((void __user *)basic.reply_ptr, &ok, sizeof(ok)))
			return 0;
	}

	if (magic2 == PRINT_ARGS_FROM_ARRAY) {

		if (cmd < 1)
			return 0;

		// grab a copy of arg ptr since we will use it to pointerwalk
		void __user *user_ptr = (void __user *)*arg;
		
		
		unsigned int count = 0;
		do {
			char buf[256] = {0};
			long len = strncpy_from_user(buf, (const char __user *)*arg, 256);
			if (len <= 0)
				return 0;

			buf[255] = '\0';
			pr_info("LKM: print %s\n", buf);

			// pointerwalk! move the pointer accordingly
			// basically arg ptr becomes arg + strlen of wwaht we copied
			// +1 for null terminator
			// this array is a flat blob like arg0\0arg1\0arg3
			*arg = *arg + strlen(buf) + 1; 
			
			// be safe.
			memzero_explicit(buf, 256);

			count++;
		} while (count < cmd);

		if (copy_to_user((void __user *)user_ptr, &reply, sizeof(reply)))
			return 0;
	}	

	return 0;
}

#ifdef CONFIG_KPROBES
static int sys_reboot_handler_pre(struct kprobe *p, struct pt_regs *regs)
{
	struct pt_regs *real_regs = PT_REAL_REGS(regs);
	int magic1 = (int)PT_REGS_PARM1(real_regs);
	int magic2 = (int)PT_REGS_PARM2(real_regs);
	int cmd = (int)PT_REGS_PARM3(real_regs);
	void __user **arg = (void __user **)&PT_REGS_SYSCALL_PARM4(real_regs);

	return template_handle_sys_reboot(magic1, magic2, cmd, arg);
}

static struct kprobe sys_reboot_kp = {
	.symbol_name = SYS_REBOOT_SYMBOL,
	.pre_handler = sys_reboot_handler_pre,
};
#endif

static int __init lkm_template_init(void) 
{
#ifdef CONFIG_KPROBES
	int ret = register_kprobe(&sys_reboot_kp);
	pr_info("LKM: register sys_reboot kprobe: %d\n", ret);
#endif
	pr_info("LKM: init with magic: 0x%x\n", (int)DEF_MAGIC);

	return 0;
}

static void __exit lkm_template_exit(void) 
{
#ifdef CONFIG_KPROBES
	unregister_kprobe(&sys_reboot_kp);
#endif
	pr_info("LKM: unload\n");
}

module_init(lkm_template_init);
module_exit(lkm_template_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("xx");
MODULE_DESCRIPTION("lkm template");

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
#endif
