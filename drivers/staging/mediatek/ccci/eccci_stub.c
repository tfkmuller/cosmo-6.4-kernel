// SPDX-License-Identifier: GPL-2.0
/*
 * MTK ECCCI bring-up scaffold (stub).
 *
 * This exists to establish a clean integration point in a 6.4 kernel tree
 * before importing the vendor ECCCI/CLDMA modem stack from the Cosmo 4.4 tree.
 * The actual modem stack is large and depends on DT/IRQ/clock wiring; keeping
 * this stub buildable allows incremental porting without destabilizing the
 * main kernel bring-up work.
 */

#include <linux/debugfs.h>
#include <linux/init.h>
#include <linux/module.h>

static struct dentry *mtk_eccci_debugfs;

static int __init mtk_eccci_stub_init(void)
{
	mtk_eccci_debugfs = debugfs_create_dir("mtk_eccci_stub", NULL);
	pr_warn("mtk_eccci_stub: loaded (scaffold only, not functional)\n");
	return 0;
}

static void __exit mtk_eccci_stub_exit(void)
{
	debugfs_remove_recursive(mtk_eccci_debugfs);
	mtk_eccci_debugfs = NULL;
	pr_info("mtk_eccci_stub: unloaded\n");
}

module_init(mtk_eccci_stub_init);
module_exit(mtk_eccci_stub_exit);

MODULE_DESCRIPTION("MediaTek ECCCI modem stack scaffold (Cosmo bring-up)");
MODULE_AUTHOR("Cosmo Communicator bring-up");
MODULE_LICENSE("GPL");

