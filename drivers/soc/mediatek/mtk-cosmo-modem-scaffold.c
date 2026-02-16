// SPDX-License-Identifier: GPL-2.0-only
/*
 * Minimal DT resource scaffold for Cosmo Communicator MT6771 modem path.
 *
 * This driver is intentionally non-functional as a data-path modem driver.
 * It provides a safe probe surface while staging DT and integration work.
 */

#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

struct mtk_cosmo_modem_desc {
	const char *block_name;
};

static int mtk_cosmo_modem_probe(struct platform_device *pdev)
{
	const struct mtk_cosmo_modem_desc *desc;
	struct device *dev = &pdev->dev;
	int irq_count;
	int mem_count = 0;
	int i;

	desc = device_get_match_data(dev);
	if (!desc)
		return -EINVAL;

	while (platform_get_resource(pdev, IORESOURCE_MEM, mem_count))
		mem_count++;

	irq_count = platform_irq_count(pdev);
	if (irq_count < 0)
		irq_count = 0;

	for (i = 0; i < irq_count; i++) {
		int irq = platform_get_irq_optional(pdev, i);

		if (irq < 0)
			continue;
		dev_dbg(dev, "%s: irq[%d]=%d\n", desc->block_name, i, irq);
	}

	dev_info(dev, "%s scaffold attached (mem=%d irq=%d)\n",
		 desc->block_name, mem_count, irq_count);

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
