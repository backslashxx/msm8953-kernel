#include <linux/lsm_hooks.h>
#include <linux/security.h>
#include <linux/binfmts.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/version.h>
#include <linux/printk.h>
#include <linux/jump_label.h>
#include <linux/string.h>
#include <linux/module.h>

#ifndef CONFIG_SECURITY_SELINUX
#error "need selinux!"
#endif

#ifdef pr_fmt
#undef pr_fmt
#define pr_fmt(fmt) "small_lsm_demo: " fmt
#endif

#define strcmp __builtin_strcmp
#define memcmp __builtin_memcmp

#define LINEAGE_LIBPERFMGR_BIN "/vendor/bin/hw/android.hardware.power-service.lineage-libperfmgr"
#define PIXEL_LIBPERFMGR_BIN "/vendor/bin/hw/android.hardware.power-service.pixel-libperfmgr"

DEFINE_STATIC_KEY_TRUE(init_branch_key);

static u32 libperfmgr_caller_sid = 0;

__attribute__((cold))
static noinline void try_get_sid(struct linux_binprm *bprm)
{
	static bool is_init_executed = false;

	if (!bprm->filename)
		return;

	if (!strcmp(bprm->filename, "/system/bin/init"))
		is_init_executed = true;

	if (!is_init_executed)
		return;

	if (!!strcmp(bprm->filename, LINEAGE_LIBPERFMGR_BIN) && !!strcmp(bprm->filename, PIXEL_LIBPERFMGR_BIN))
		return;

	pr_info("libperfmgr being executed! try grab sid!\n");

	security_task_getsecid(current, &libperfmgr_caller_sid);

	if (!libperfmgr_caller_sid)
		return;

	pr_info("got libperfmgr caller's sid: %d \n", libperfmgr_caller_sid);

	static_branch_disable(&init_branch_key);
	smp_mb();
}

static int hook_bprm_check(struct linux_binprm *bprm)
{
	/**
	 * we do this incase target is being executed repeatedly
	 * we store sid for faster compare
	 * this way we dont do string comapre a lot
	 */
	if (static_branch_likely(&init_branch_key))
		try_get_sid(bprm);

	u32 current_sid;
	security_task_getsecid(current, &current_sid);

	if (likely(current_sid != libperfmgr_caller_sid))
		return 0;

	if (!bprm->filename)
		return 0;

	// quick word compare
	if (!strstarts(bprm->filename, "/vendor/"))
		return 0;

	if (!!strcmp(bprm->filename, LINEAGE_LIBPERFMGR_BIN) && !!strcmp(bprm->filename, PIXEL_LIBPERFMGR_BIN))
		return 0;

	pr_info("exec found: %s\n", bprm->filename);
	
	/* do stuff */

	return 0;
}

static struct security_hook_list hooks[] __ro_after_init = {
	LSM_HOOK_INIT(bprm_check_security, hook_bprm_check),
};

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#define security_add_hooks_compat security_add_hooks
#else
#define security_add_hooks_compat(a, b, c) security_add_hooks(a, b)
#endif

static __init int small_lsm_init(void)
{
	security_add_hooks_compat(hooks, ARRAY_SIZE(hooks), "lsm");
	pr_info("initialized %d LSM \n", ARRAY_SIZE(hooks));

	return 0;
}
device_initcall(small_lsm_init);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("xx");
MODULE_DESCRIPTION("small lsm demo");
