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
#include <linux/kthread.h>

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
static int hook_bprm_check(struct linux_binprm *bprm);

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

static struct security_hook_list hooks[] __ro_after_init = {
	LSM_HOOK_INIT(bprm_check_security, hook_bprm_check),
};

/*
 * LSMs are actually unhookable, however, it requires CONFIG_SECURITY_SELINUX_DISABLE
 * ref: security_delete_hooks(), lsm_hooks.h
 *
 * when that is disabled, we get an issue as we will be writing to ro memory.
 * "Unable to handle kernel write to read-only memory at virtual address fffffffffffuckyou"
 *
 * however we can just do vmap-as-rw trick to create another reality where this memory segment is rw.
 *
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 17, 0) || defined(LSM_DEMO_COMPAT_SECURITY_DELETE_HOOKS_HLIST)
static void delete_lsm_entry(struct hlist_node *n)
{
	struct hlist_node *next = n->next;
	struct hlist_node **pprev = n->pprev;

	if (!pprev)
		return;

	// this is here so we don't get lost
	/**
	 *	original state
	 * n			ptr	*ptr
	 * H	hlist_head	0x1000	0xA000
	 *
	 * A	node->next	0xA000	0xB000
	 *	node->pprev	0xA008	0x1000
	 *
	 * B	node->next	0xB000	0xC000
	 *	node->pprev	0xB008	0xA000
	 *
	 * C	node->next	0xC000	0xFFFF
	 *	node->pprev	0xC008	0xB000
	 *
	 */

	// on hlist, pprev is the address of the 'next' pointer in the previous element
	// so what we do is:
	// 	write the value 0xC000 (next) into address 0xA000 (A->next)
	// 	write the value 0xA000 (pprev) into address 0xC008 (C->pprev)

	/**
	 * 	after this routine
	 *
	 * H	hlist_head	0x1000	0xA000
	 *
	 * A	node->next	0xA000	0xC000  <-- now points to C
	 *	node->pprev	0xA008	0x1000
	 *
	 * B	node->next	0xB000	0xC000  <-- orphaned
	 *	node->pprev	0xB008	0xA000  <-- orphaned
	 *
	 * C	node->next	0xC000	0xFFFF
	 *	node->pprev	0xC008	0xA000  <-- now points to A's next
	 *
	 */

	// NOTE: pprev is **
	uintptr_t addr = (uintptr_t)pprev;
	uintptr_t base = addr & PAGE_MASK;
	uintptr_t offset = addr & ~PAGE_MASK;

	struct page *page = phys_to_page(__pa(base));
	if (!page)
		return;

	// vmap pprev
	void *writable_addr = vmap(&page, 1, VM_MAP, PAGE_KERNEL);
	if (!writable_addr)
		return;

	uintptr_t target_slot = (uintptr_t)((uintptr_t)writable_addr + offset);

	preempt_disable();

	WRITE_ONCE(*(struct hlist_node **)target_slot, next);

	preempt_enable();

	vunmap(writable_addr);

	smp_mb();

	if (!next)
		return;

	// NOTE: pprev is **, taking ref, it becomes ***
	addr = (unsigned long)&next->pprev;
	base = addr & PAGE_MASK;
	offset = addr & ~PAGE_MASK;

	page = phys_to_page(__pa(base));
	if (!page)
		return;

	writable_addr = vmap(&page, 1, VM_MAP, PAGE_KERNEL);
	if (!writable_addr)
		return;

	target_slot = (uintptr_t)((uintptr_t)writable_addr + offset);

	preempt_disable();

	// use our pprev as the new pprev for the next in chain
	WRITE_ONCE(*(struct hlist_node ***)target_slot, pprev);

	preempt_enable();

	vunmap(writable_addr);

	smp_mb();
}
#else
static void delete_lsm_entry(struct list_head *entry)
{
	struct list_head *next = entry->next;
	struct list_head *prev = entry->prev;

	// on a linked list we have to patch both the before us and the next to us
	if (!prev || !next)
		return;

	// smash prev->next, basically we write 'next' into 'prev->next'
	unsigned long addr_p = (unsigned long)&prev->next;
	unsigned long base_p = addr_p & PAGE_MASK;
	unsigned long offset_p = addr_p & ~PAGE_MASK;

	struct page *page_p = phys_to_page(__pa(base_p));
	if (!page_p)
		return;

	void *w_page = vmap(&page_p, 1, VM_MAP, PAGE_KERNEL);
	if (!w_page)
		return;

	struct list_head **target = (void *)((unsigned long)w_page + offset_p);
	
	preempt_disable();

	WRITE_ONCE(*target, next);

	preempt_enable();
	vunmap(w_page);

	// smash next->prev, basically we need to write 'prev' into 'next->prev'
	unsigned long addr_n = (unsigned long)&next->prev;
	unsigned long base_n = addr_n & PAGE_MASK;
	unsigned long offset_n = addr_n & ~PAGE_MASK;

	struct page *page_n = phys_to_page(__pa(base_n));
	if (!page_n)
		return;

	w_page = vmap(&page_n, 1, VM_MAP, PAGE_KERNEL);
	if (!w_page)
		return;
	
	target = (void *)((unsigned long)w_page + offset_n);

	preempt_disable();

	WRITE_ONCE(*target, prev);

	preempt_enable();
	vunmap(w_page);

	smp_mb();

}
#endif

// the kernel straight up does it like this, if you get an issue, just move this inside stop_machine
static inline void __security_delete_hooks(struct security_hook_list *hooks, int count)
{
	int i;
	for (i = 0; i < count; i++)
		delete_lsm_entry(&hooks[i].list);
}

static int lsm_hook_exit(void *data)
{
	pr_info("unhooking %d LSM \n", ARRAY_SIZE(hooks));
	__security_delete_hooks(hooks, ARRAY_SIZE(hooks));
	return 0;
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
	
//#if 0 // optional, if this is only needed once, we can actually "unhook" the LSM

	kthread_run(lsm_hook_exit, NULL, "unhook");
//#endif

	return 0;
}

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
