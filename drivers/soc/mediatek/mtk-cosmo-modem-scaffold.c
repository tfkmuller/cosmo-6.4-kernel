// SPDX-License-Identifier: GPL-2.0-only
/*
 * Minimal DT resource scaffold for Cosmo Communicator MT6771 modem path.
 *
 * This driver is intentionally non-functional as a data-path modem driver.
 * It provides a safe probe surface while staging DT and integration work.
 */

#include <linux/debugfs.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>

struct mtk_cosmo_modem_desc {
	const char *block_name;
};

struct mtk_cosmo_modem {
	struct device *dev;
	const struct mtk_cosmo_modem_desc *desc;
	void __iomem *base;
	int mem_count;
	int irq_count;
	int irq;
	bool irq_test_enabled;
	u32 ccif_ack_offset;
	u32 ccif_ack_mask;
	atomic_t irq_hits;
#ifdef CONFIG_DEBUG_FS
	struct dentry *dbg_dir;
#endif
};

#ifdef CONFIG_DEBUG_FS
static struct dentry *mtk_cosmo_dbg_root;
static DEFINE_MUTEX(mtk_cosmo_dbg_lock);

static int mtk_cosmo_modem_status_show(struct seq_file *s, void *unused)
{
	struct mtk_cosmo_modem *md = s->private;

	seq_printf(s, "block=%s\n", md->desc->block_name);
	seq_printf(s, "device=%s\n", dev_name(md->dev));
	seq_printf(s, "mem_count=%d\n", md->mem_count);
	seq_printf(s, "irq_count=%d\n", md->irq_count);
	seq_printf(s, "irq_test_enabled=%u\n", md->irq_test_enabled ? 1 : 0);
	seq_printf(s, "irq=%d\n", md->irq);
	seq_printf(s, "irq_hits=%d\n", atomic_read(&md->irq_hits));
	seq_printf(s, "ack_offset=0x%x\n", md->ccif_ack_offset);
	seq_printf(s, "ack_mask=0x%x\n", md->ccif_ack_mask);

	return 0;
}

static int mtk_cosmo_modem_status_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtk_cosmo_modem_status_show, inode->i_private);
}

static const struct file_operations mtk_cosmo_modem_status_fops = {
	.owner = THIS_MODULE,
	.open = mtk_cosmo_modem_status_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static void mtk_cosmo_modem_debugfs_remove(void *data)
{
	struct mtk_cosmo_modem *md = data;

	debugfs_remove_recursive(md->dbg_dir);
}

static void mtk_cosmo_modem_debugfs_init(struct mtk_cosmo_modem *md)
{
	struct dentry *dir;
	int ret;

	mutex_lock(&mtk_cosmo_dbg_lock);
	if (!mtk_cosmo_dbg_root)
		mtk_cosmo_dbg_root = debugfs_create_dir("mtk-cosmo-modem", NULL);
	mutex_unlock(&mtk_cosmo_dbg_lock);

	if (!mtk_cosmo_dbg_root || IS_ERR(mtk_cosmo_dbg_root))
		return;

	dir = debugfs_create_dir(dev_name(md->dev), mtk_cosmo_dbg_root);
	if (IS_ERR_OR_NULL(dir))
		return;

	md->dbg_dir = dir;
	debugfs_create_file("status", 0444, md->dbg_dir, md,
			    &mtk_cosmo_modem_status_fops);
	ret = devm_add_action_or_reset(md->dev, mtk_cosmo_modem_debugfs_remove,
				       md);
	if (ret)
		md->dbg_dir = NULL;
}
#else
static void mtk_cosmo_modem_debugfs_init(struct mtk_cosmo_modem *md)
{
}
#endif

static irqreturn_t mtk_cosmo_modem_irq_handler(int irq, void *data)
{
	struct mtk_cosmo_modem *md = data;

	if (md->irq_test_enabled && md->base && md->ccif_ack_mask)
		writel(md->ccif_ack_mask, md->base + md->ccif_ack_offset);

	atomic_inc(&md->irq_hits);
	dev_dbg(md->dev, "%s: irq handled (hits=%d)\n",
		md->desc->block_name, atomic_read(&md->irq_hits));

	return IRQ_HANDLED;
}

static int mtk_cosmo_modem_probe(struct platform_device *pdev)
{
	const struct mtk_cosmo_modem_desc *desc;
	struct mtk_cosmo_modem *md;
	struct device_node *np = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	int ret;
	int i;

	desc = device_get_match_data(dev);
	if (!desc)
		return -EINVAL;

	md = devm_kzalloc(dev, sizeof(*md), GFP_KERNEL);
	if (!md)
		return -ENOMEM;

	md->dev = dev;
	md->desc = desc;
	md->irq = -ENXIO;
	md->ccif_ack_offset = 0x14;
	atomic_set(&md->irq_hits, 0);
	platform_set_drvdata(pdev, md);

	while (platform_get_resource(pdev, IORESOURCE_MEM, md->mem_count))
		md->mem_count++;

	if (md->mem_count > 0) {
		md->base = devm_platform_ioremap_resource(pdev, 0);
		if (IS_ERR(md->base))
			return dev_err_probe(dev, PTR_ERR(md->base),
					     "%s: failed to map registers\n",
					     md->desc->block_name);
	}

	md->irq_count = platform_irq_count(pdev);
	if (md->irq_count < 0)
		md->irq_count = 0;

	for (i = 0; i < md->irq_count; i++) {
		int irq = platform_get_irq_optional(pdev, i);

		if (irq < 0)
			continue;
		dev_dbg(dev, "%s: irq[%d]=%d\n", desc->block_name, i, irq);
	}

	if (of_device_is_compatible(np, "mediatek,ap_ccif0") &&
	    device_property_read_bool(dev, "mediatek,cosmo-enable-irq-test")) {
		md->irq_test_enabled = true;
		device_property_read_u32(dev, "mediatek,cosmo-ccif-ack-offset",
					 &md->ccif_ack_offset);
		device_property_read_u32(dev, "mediatek,cosmo-ccif-ack-mask",
					 &md->ccif_ack_mask);

		md->irq = platform_get_irq(pdev, 0);
		if (md->irq < 0)
			return dev_err_probe(dev, md->irq,
					     "%s: missing irq for test mode\n",
					     md->desc->block_name);

		if (!md->base)
			return dev_err_probe(dev, -EINVAL,
					     "%s: missing registers for irq test\n",
					     md->desc->block_name);

		ret = devm_request_irq(dev, md->irq, mtk_cosmo_modem_irq_handler,
				       0, dev_name(dev), md);
		if (ret)
			return dev_err_probe(dev, ret,
					     "%s: failed to request irq %d\n",
					     md->desc->block_name, md->irq);
	}

	mtk_cosmo_modem_debugfs_init(md);

	dev_info(dev, "%s scaffold attached (mem=%d irq=%d)\n",
		 desc->block_name, md->mem_count, md->irq_count);

	return 0;
}

static const struct mtk_cosmo_modem_desc mtk_cosmo_mdcldma = {
	.block_name = "mdcldma",
};

static const struct mtk_cosmo_modem_desc mtk_cosmo_ap_ccif0 = {
	.block_name = "ap_ccif0",
};

static const struct mtk_cosmo_modem_desc mtk_cosmo_md_ccif0 = {
	.block_name = "md_ccif0",
};

static const struct mtk_cosmo_modem_desc mtk_cosmo_ap_ccif1 = {
	.block_name = "ap_ccif1",
};

static const struct mtk_cosmo_modem_desc mtk_cosmo_md_ccif1 = {
	.block_name = "md_ccif1",
};

static const struct mtk_cosmo_modem_desc mtk_cosmo_apcldmain = {
	.block_name = "apcldmain",
};

static const struct mtk_cosmo_modem_desc mtk_cosmo_apcldmaout = {
	.block_name = "apcldmaout",
};

static const struct mtk_cosmo_modem_desc mtk_cosmo_apcldmamisc = {
	.block_name = "apcldmamisc",
};

static const struct mtk_cosmo_modem_desc mtk_cosmo_apcldmaout_ao = {
	.block_name = "apcldmaout_ao",
};

static const struct mtk_cosmo_modem_desc mtk_cosmo_apcldmamisc_ao = {
	.block_name = "apcldmamisc_ao",
};

static const struct mtk_cosmo_modem_desc mtk_cosmo_mdcldmain = {
	.block_name = "mdcldmain",
};

static const struct mtk_cosmo_modem_desc mtk_cosmo_mdcldmaout = {
	.block_name = "mdcldmaout",
};

static const struct mtk_cosmo_modem_desc mtk_cosmo_mdcldmamisc = {
	.block_name = "mdcldmamisc",
};

static const struct mtk_cosmo_modem_desc mtk_cosmo_infra_md_cfg = {
	.block_name = "infra_md_cfg",
};

static const struct of_device_id mtk_cosmo_modem_of_match[] = {
	{ .compatible = "mediatek,mdcldma", .data = &mtk_cosmo_mdcldma },
	{ .compatible = "mediatek,ap_ccif0", .data = &mtk_cosmo_ap_ccif0 },
	{ .compatible = "mediatek,md_ccif0", .data = &mtk_cosmo_md_ccif0 },
	{ .compatible = "mediatek,ap_ccif1", .data = &mtk_cosmo_ap_ccif1 },
	{ .compatible = "mediatek,md_ccif1", .data = &mtk_cosmo_md_ccif1 },
	{ .compatible = "mediatek,apcldmain", .data = &mtk_cosmo_apcldmain },
	{ .compatible = "mediatek,apcldmaout", .data = &mtk_cosmo_apcldmaout },
	{ .compatible = "mediatek,apcldmamisc", .data = &mtk_cosmo_apcldmamisc },
	{ .compatible = "mediatek,apcldmaout_ao", .data = &mtk_cosmo_apcldmaout_ao },
	{ .compatible = "mediatek,apcldmamisc_ao", .data = &mtk_cosmo_apcldmamisc_ao },
	{ .compatible = "mediatek,mdcldmain", .data = &mtk_cosmo_mdcldmain },
	{ .compatible = "mediatek,mdcldmaout", .data = &mtk_cosmo_mdcldmaout },
	{ .compatible = "mediatek,mdcldmamisc", .data = &mtk_cosmo_mdcldmamisc },
	{ .compatible = "mediatek,infra_md_cfg", .data = &mtk_cosmo_infra_md_cfg },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, mtk_cosmo_modem_of_match);

static struct platform_driver mtk_cosmo_modem_driver = {
	.probe = mtk_cosmo_modem_probe,
	.driver = {
		.name = "mtk-cosmo-modem-scaffold",
		.of_match_table = mtk_cosmo_modem_of_match,
	},
};
module_platform_driver(mtk_cosmo_modem_driver);

MODULE_DESCRIPTION("Cosmo Communicator MT6771 modem scaffold");
MODULE_LICENSE("GPL");
