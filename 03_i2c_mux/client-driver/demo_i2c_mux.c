// SPDX-License-Identifier: GPL-2.0-only
/*
 * Educational driver for a fictional four-channel I2C mux.
 *
 * Control byte: BIT(channel) selects one channel; 0 disconnects all.
 * Replace this model with the real device datasheet and upstream driver.
 */

#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/i2c-mux.h>
#include <linux/module.h>
#include <linux/property.h>
#include <linux/types.h>

#define DEMO_MUX_CHANNELS 4
#define DEMO_MUX_DISCONNECT 0x00
#define DEMO_MUX_CACHE_INVALID 0xff

struct demo_mux_data {
	struct i2c_client *client;
	u8 last_value;
	bool idle_disconnect;
};

/*
 * The demo uses parent-locked mux semantics (flags == 0). The mux core has
 * already locked the parent adapter when this callback runs. Therefore the
 * selector transaction must use the unlocked SMBus helper. Calling
 * i2c_smbus_write_byte() here would try to lock the same adapter again.
 */
static int demo_mux_write_unlocked(struct demo_mux_data *data, u8 value)
{
	union i2c_smbus_data smbus_data = { };
	s32 ret;

	ret = __i2c_smbus_xfer(data->client->adapter,
				data->client->addr,
				data->client->flags,
				I2C_SMBUS_WRITE,
				value,
				I2C_SMBUS_BYTE,
				&smbus_data);
	if (ret < 0)
		data->last_value = DEMO_MUX_CACHE_INVALID;

	return ret;
}

static int demo_mux_select(struct i2c_mux_core *muxc, u32 channel)
{
	struct demo_mux_data *data = i2c_mux_priv(muxc);
	u8 value;
	int ret;

	if (channel >= DEMO_MUX_CHANNELS)
		return -EINVAL;

	value = BIT(channel);
	if (data->last_value == value)
		return 0;

	ret = demo_mux_write_unlocked(data, value);
	if (!ret)
		data->last_value = value;

	return ret;
}

static int demo_mux_deselect(struct i2c_mux_core *muxc, u32 channel)
{
	struct demo_mux_data *data = i2c_mux_priv(muxc);
	int ret;

	(void)channel;
	if (!data->idle_disconnect ||
	    data->last_value == DEMO_MUX_DISCONNECT)
		return 0;

	ret = demo_mux_write_unlocked(data, DEMO_MUX_DISCONNECT);
	if (!ret)
		data->last_value = DEMO_MUX_DISCONNECT;

	return ret;
}

static int demo_mux_probe(struct i2c_client *client)
{
	struct i2c_mux_core *muxc;
	struct demo_mux_data *data;
	bool idle_disconnect;
	int channel;
	int ret;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE))
		return dev_err_probe(&client->dev, -EOPNOTSUPP,
				     "parent lacks SMBus byte support\n");

	idle_disconnect = device_property_read_bool(&client->dev,
						     "i2c-mux-idle-disconnect");

	/* flags == 0 selects parent-locked operation. */
	muxc = i2c_mux_alloc(client->adapter, &client->dev,
			     DEMO_MUX_CHANNELS, sizeof(*data), 0,
			     demo_mux_select,
			     idle_disconnect ? demo_mux_deselect : NULL);
	if (!muxc)
		return -ENOMEM;

	data = i2c_mux_priv(muxc);
	data->client = client;
	data->last_value = DEMO_MUX_CACHE_INVALID;
	data->idle_disconnect = idle_disconnect;
	i2c_set_clientdata(client, muxc);

	/* Probe is outside a mux callback, so the normal locking helper is used. */
	ret = i2c_smbus_write_byte(client, DEMO_MUX_DISCONNECT);
	if (ret < 0)
		return dev_err_probe(&client->dev, ret,
				     "failed to initialize disconnected state\n");
	data->last_value = DEMO_MUX_DISCONNECT;

	for (channel = 0; channel < DEMO_MUX_CHANNELS; channel++) {
		ret = i2c_mux_add_adapter(muxc, 0, channel);
		if (ret)
			goto err_del_adapters;
	}

	dev_info(&client->dev, "registered %u channels%s\n",
		 DEMO_MUX_CHANNELS,
		 idle_disconnect ? " with idle disconnect" : "");
	return 0;

err_del_adapters:
	i2c_mux_del_adapters(muxc);
	i2c_smbus_write_byte(client, DEMO_MUX_DISCONNECT);
	return dev_err_probe(&client->dev, ret,
			     "failed to register channel %d\n", channel);
}

static void demo_mux_remove(struct i2c_client *client)
{
	struct i2c_mux_core *muxc = i2c_get_clientdata(client);

	i2c_mux_del_adapters(muxc);
	if (i2c_smbus_write_byte(client, DEMO_MUX_DISCONNECT) < 0)
		dev_warn(&client->dev, "failed to disconnect channels\n");
}

static const struct of_device_id demo_mux_of_match[] = {
	{ .compatible = "demo,quad-i2c-mux" },
	{ }
};
MODULE_DEVICE_TABLE(of, demo_mux_of_match);

static const struct i2c_device_id demo_mux_ids[] = {
	{ "demo_i2c_mux", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, demo_mux_ids);

static struct i2c_driver demo_mux_driver = {
	.driver = {
		.name = "demo_i2c_mux",
		.of_match_table = demo_mux_of_match,
	},
	.probe = demo_mux_probe,
	.remove = demo_mux_remove,
	.id_table = demo_mux_ids,
};
module_i2c_driver(demo_mux_driver);

MODULE_AUTHOR("OpenBMC Linux Driver Study Project");
MODULE_DESCRIPTION("Educational four-channel I2C mux driver");
MODULE_LICENSE("GPL");

