// SPDX-License-Identifier: GPL-2.0
/*
 * MTK CCMNI bring-up scaffold (stub).
 *
 * Real CCMNI creates netdevs (ccmni0..N) backed by the ECCCI modem stack.
 * This stub exists so Kconfig wiring can be enabled during early bring-up
 * without pulling in the full vendor netdev implementation yet.
 */

#include <linux/init.h>
#include <linux/module.h>

static int __init mtk_ccmni_stub_init(void)
{
	pr_warn("mtk_ccmni_stub: loaded (scaffold only, not functional)\n");
	return 0;
}

static void __exit mtk_ccmni_stub_exit(void)
{
	pr_info("mtk_ccmni_stub: unloaded\n");
}

module_init(mtk_ccmni_stub_init);
module_exit(mtk_ccmni_stub_exit);

MODULE_DESCRIPTION("MediaTek CCMNI netdev scaffold (Cosmo bring-up)");
MODULE_AUTHOR("Cosmo Communicator bring-up");
MODULE_LICENSE("GPL");

