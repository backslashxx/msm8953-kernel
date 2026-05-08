obj-y += small_lsm.o
ccflags-y += -g0
ccflags-y += -Wno-declaration-after-statement

# see security_delete_hooks
ifeq ($(shell grep -A5 "^struct security_hook_list" $(srctree)/include/linux/lsm_hooks.h 2>/dev/null | grep -q "struct hlist_node" 2>/dev/null; echo $$?),0)
$(info -- lsm_demo: security_hook_list - hlist_node found!)
CFLAGS_small_lsm.o += -DLSM_DEMO_COMPAT_SECURITY_DELETE_HOOKS_HLIST
endif
