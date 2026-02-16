// SPDX-License-Identifier: GPL-2.0-only
/*
 * Solomon SSD20xx touchscreen driver (minimal upstream-style bring-up port)
 *
 * This is a clean Linux input/i2c implementation for the legacy
 * "mediatek,solomon_touch" compatible used by the Cosmo Communicator.
 */

#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>

#define SSD20XX_NAME			"solomon_ssd20xx"

#define SSD20XX_MAX_SLOTS		10
#define SSD20XX_EVENT_SIZE		6
#define SSD20XX_MAX_EVENT_BYTES		(SSD20XX_MAX_SLOTS * SSD20XX_EVENT_SIZE)

#define SSD20XX_DEFAULT_IC_MAX_X	1080
#define SSD20XX_DEFAULT_IC_MAX_Y	2160

#define SSD20XX_REG_INT_CLEAR		0x0043
#define SSD20XX_REG_X_RESOLUTION	0x0056
#define SSD20XX_REG_Y_RESOLUTION	0x0057
#define SSD20XX_REG_STATUS_LENGTH	0x0AF0
#define SSD20XX_REG_POINT_DATA		0x0AF1

struct ssd20xx {
	struct i2c_client *client;
	struct input_dev *input;
	struct gpio_desc *reset_gpio;

	unsigned long active_slots[BITS_TO_LONGS(SSD20XX_MAX_SLOTS)];
	u16 ic_max_x;
	u16 ic_max_y;
	bool no_legacy_rotation;
};

static int ssd20xx_read_block(struct ssd20xx *ts, u16 reg, void *buf, size_t len)
{
	struct i2c_msg msgs[2];
	u8 addr[2] = { reg & 0xff, reg >> 8 };
	int ret;

	msgs[0].addr = ts->client->addr;
	msgs[0].flags = 0;
	msgs[0].len = sizeof(addr);
	msgs[0].buf = addr;

	msgs[1].addr = ts->client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = len;
	msgs[1].buf = buf;

	ret = i2c_transfer(ts->client->adapter, msgs, ARRAY_SIZE(msgs));
	if (ret < 0)
		return ret;
	if (ret != ARRAY_SIZE(msgs))
		return -EIO;

	return 0;
}

static int ssd20xx_read_u16(struct ssd20xx *ts, u16 reg, u16 *val)
{
	u8 buf[2];
	int ret;

	ret = ssd20xx_read_block(ts, reg, buf, sizeof(buf));
	if (ret)
		return ret;

	*val = buf[0] | (buf[1] << 8);
	return 0;
}

static int ssd20xx_write_u16(struct ssd20xx *ts, u16 reg, u16 val)
{
	u8 tx[4] = {
		reg & 0xff,
		reg >> 8,
		val & 0xff,
		val >> 8,
	};
	int ret;

	ret = i2c_master_send(ts->client, tx, sizeof(tx));
	if (ret < 0)
		return ret;
	if (ret != sizeof(tx))
		return -EIO;

	return 0;
}

static void ssd20xx_release_slots(struct ssd20xx *ts)
{
	int slot;

	for (slot = 0; slot < SSD20XX_MAX_SLOTS; slot++) {
		if (!test_bit(slot, ts->active_slots))
			continue;

		input_mt_slot(ts->input, slot);
		input_mt_report_slot_state(ts->input, MT_TOOL_FINGER, false);
	}

	bitmap_zero(ts->active_slots, SSD20XX_MAX_SLOTS);
	input_mt_sync_frame(ts->input);
	input_sync(ts->input);
}

static irqreturn_t ssd20xx_irq_thread(int irq, void *dev_id)
{
	struct ssd20xx *ts = dev_id;
	unsigned long current_slots[BITS_TO_LONGS(SSD20XX_MAX_SLOTS)];
	u8 raw[SSD20XX_MAX_EVENT_BYTES];
	u16 point_info;
	u16 raw_x, raw_y;
	u8 id, len, weight;
	int points, i;
	int ret;

	ret = ssd20xx_read_u16(ts, SSD20XX_REG_STATUS_LENGTH, &point_info);
	if (ret) {
		dev_dbg(&ts->client->dev, "status read failed: %d\n", ret);
		return IRQ_HANDLED;
	}

	len = point_info & 0xff;
	if (!len) {
		ssd20xx_release_slots(ts);
		goto clear_irq;
	}

	if (len > SSD20XX_MAX_EVENT_BYTES) {
		dev_dbg(&ts->client->dev, "clamping oversized report len=%u\n", len);
		len = SSD20XX_MAX_EVENT_BYTES;
	}

	len -= len % SSD20XX_EVENT_SIZE;
	if (!len)
		goto clear_irq;

	ret = ssd20xx_read_block(ts, SSD20XX_REG_POINT_DATA, raw, len);
	if (ret) {
		dev_dbg(&ts->client->dev, "point read failed: %d\n", ret);
		return IRQ_HANDLED;
	}

	bitmap_zero(current_slots, SSD20XX_MAX_SLOTS);
	points = len / SSD20XX_EVENT_SIZE;

	for (i = 0; i < points; i++) {
		const u8 *evt = &raw[i * SSD20XX_EVENT_SIZE];
		int x, y;

		id = evt[0];
		raw_x = ((evt[3] & 0xf0) << 4) | evt[1];
		raw_y = ((evt[3] & 0x0f) << 8) | evt[2];
		weight = evt[4];

		if (id >= SSD20XX_MAX_SLOTS)
			continue;

		if (!ts->no_legacy_rotation) {
			x = raw_y;
			y = ts->ic_max_x > raw_x ? ts->ic_max_x - raw_x : 0;
		} else {
			x = raw_x;
			y = raw_y;
		}

		input_mt_slot(ts->input, id);
		input_mt_report_slot_state(ts->input, MT_TOOL_FINGER, weight > 0);
		if (weight > 0) {
			input_report_abs(ts->input, ABS_MT_POSITION_X, x);
			input_report_abs(ts->input, ABS_MT_POSITION_Y, y);
			input_report_abs(ts->input, ABS_MT_PRESSURE, weight);
			__set_bit(id, current_slots);
		}
	}

	for (i = 0; i < SSD20XX_MAX_SLOTS; i++) {
		if (test_bit(i, ts->active_slots) && !test_bit(i, current_slots)) {
			input_mt_slot(ts->input, i);
			input_mt_report_slot_state(ts->input, MT_TOOL_FINGER, false);
		}
	}

	bitmap_copy(ts->active_slots, current_slots, SSD20XX_MAX_SLOTS);
	input_mt_sync_frame(ts->input);
	input_sync(ts->input);

clear_irq:
	ssd20xx_write_u16(ts, SSD20XX_REG_INT_CLEAR, 0x0001);
	return IRQ_HANDLED;
}

static int ssd20xx_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct ssd20xx *ts;
	struct input_dev *input;
	u16 reg_val;
	int irqflags;
	int ret;

	if (!client->irq)
		return dev_err_probe(dev, -EINVAL, "missing IRQ\n");

	ts = devm_kzalloc(dev, sizeof(*ts), GFP_KERNEL);
	if (!ts)
		return -ENOMEM;

	ts->client = client;
	ts->ic_max_x = SSD20XX_DEFAULT_IC_MAX_X;
	ts->ic_max_y = SSD20XX_DEFAULT_IC_MAX_Y;
	ts->no_legacy_rotation = device_property_read_bool(dev,
							   "solomon,no-legacy-rotation");

	ts->reset_gpio = devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ts->reset_gpio))
		return dev_err_probe(dev, PTR_ERR(ts->reset_gpio),
				     "failed to get reset gpio\n");

	if (ts->reset_gpio) {
		gpiod_set_value_cansleep(ts->reset_gpio, 0);
		usleep_range(5000, 7000);
		gpiod_set_value_cansleep(ts->reset_gpio, 1);
		msleep(50);
	}

	ret = ssd20xx_read_u16(ts, SSD20XX_REG_X_RESOLUTION, &reg_val);
	if (!ret && reg_val)
		ts->ic_max_x = reg_val;

	ret = ssd20xx_read_u16(ts, SSD20XX_REG_Y_RESOLUTION, &reg_val);
	if (!ret && reg_val)
		ts->ic_max_y = reg_val;

	input = devm_input_allocate_device(dev);
	if (!input)
		return -ENOMEM;

	ts->input = input;
	input->name = "Solomon SSD20xx Touchscreen";
	input->id.bustype = BUS_I2C;

	input_set_capability(input, EV_KEY, BTN_TOUCH);
	input_set_abs_params(input, ABS_MT_POSITION_X, 0,
			     ts->no_legacy_rotation ? ts->ic_max_x : ts->ic_max_y,
			     0, 0);
	input_set_abs_params(input, ABS_MT_POSITION_Y, 0,
			     ts->no_legacy_rotation ? ts->ic_max_y : ts->ic_max_x,
			     0, 0);
	input_set_abs_params(input, ABS_MT_PRESSURE, 0, 255, 0, 0);

	ret = input_mt_init_slots(input, SSD20XX_MAX_SLOTS, INPUT_MT_DIRECT);
	if (ret)
		return dev_err_probe(dev, ret, "failed to init mt slots\n");

	i2c_set_clientdata(client, ts);

	irqflags = irq_get_trigger_type(client->irq);
	if (!irqflags)
		irqflags = IRQF_TRIGGER_LOW;

	ret = devm_request_threaded_irq(dev, client->irq, NULL, ssd20xx_irq_thread,
					irqflags | IRQF_ONESHOT,
					SSD20XX_NAME, ts);
	if (ret)
		return dev_err_probe(dev, ret, "failed to request irq\n");

	ret = input_register_device(input);
	if (ret)
		return dev_err_probe(dev, ret, "failed to register input device\n");

	dev_info(dev, "SSD20xx touch online (%ux%u, legacy_rotation=%s)\n",
		 ts->ic_max_x, ts->ic_max_y,
		 ts->no_legacy_rotation ? "off" : "on");
	return 0;
}

static const struct i2c_device_id ssd20xx_id[] = {
	{ "ssd20xx" },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ssd20xx_id);

static const struct of_device_id ssd20xx_of_match[] = {
	{ .compatible = "mediatek,solomon_touch" },
	{ }
};
MODULE_DEVICE_TABLE(of, ssd20xx_of_match);

static struct i2c_driver ssd20xx_driver = {
	.driver = {
		.name = SSD20XX_NAME,
		.of_match_table = ssd20xx_of_match,
	},
	.probe = ssd20xx_probe,
	.id_table = ssd20xx_id,
};
module_i2c_driver(ssd20xx_driver);

MODULE_AUTHOR("Cosmo 6.4 bring-up");
MODULE_DESCRIPTION("Solomon SSD20xx touchscreen driver");
MODULE_LICENSE("GPL");
