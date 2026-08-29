// SPDX-License-Identifier: GPL-2.0-only
/*
 * Educational I2C hwmon driver for a fictional temperature sensor.
 * Replace the register map and binding with the real device specification.
 */

#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/hwmon.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/regmap.h>

#define DEMO_REG_DEVICE_ID       0x00
#define DEMO_REG_TEMP_MSB        0x01
#define DEMO_REG_TEMP_LSB        0x02
#define DEMO_REG_STATUS          0x03
#define DEMO_REG_CONFIG          0x04
#define DEMO_REG_TEMP_HIGH       0x05

#define DEMO_DEVICE_ID           0xa5
#define DEMO_STATUS_HIGH_ALARM   BIT(0)

struct demo_i2c_data {
	struct device *dev;
	struct regmap *regmap;
	struct mutex lock;
};

static const struct regmap_config demo_i2c_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = DEMO_REG_TEMP_HIGH,
	.cache_type = REGCACHE_NONE,
};

static int demo_i2c_read_temp(struct demo_i2c_data *data, long *value)
{
	u8 bytes[2];
	s16 raw;
	int ret;

	ret = regmap_bulk_read(data->regmap, DEMO_REG_TEMP_MSB,
			       bytes, sizeof(bytes));
	if (ret)
		return ret;

	/* Signed 12-bit two's-complement value in bits [15:4]. */
	raw = (s16)((bytes[0] << 8) | bytes[1]);
	raw >>= 4;
	*value = DIV_ROUND_CLOSEST((long)raw * 625, 10);

	return 0;
}

static umode_t demo_i2c_is_visible(const void *drvdata,
				   enum hwmon_sensor_types type,
				   u32 attr, int channel)
{
	if (type != hwmon_temp || channel != 0)
		return 0;

	switch (attr) {
	case hwmon_temp_input:
	case hwmon_temp_alarm:
		return 0444;
	case hwmon_temp_max:
		return 0644;
	default:
		return 0;
	}
}

static int demo_i2c_read(struct device *dev,
			 enum hwmon_sensor_types type,
			 u32 attr, int channel, long *value)
{
	struct demo_i2c_data *data = dev_get_drvdata(dev);
	unsigned int regval;
	int ret;

	if (type != hwmon_temp || channel != 0)
		return -EOPNOTSUPP;

	mutex_lock(&data->lock);

	switch (attr) {
	case hwmon_temp_input:
		ret = demo_i2c_read_temp(data, value);
		break;
	case hwmon_temp_max:
		ret = regmap_read(data->regmap, DEMO_REG_TEMP_HIGH, &regval);
		if (!ret)
			*value = (long)(s8)regval * 1000;
		break;
	case hwmon_temp_alarm:
		ret = regmap_read(data->regmap, DEMO_REG_STATUS, &regval);
		if (!ret)
			*value = !!(regval & DEMO_STATUS_HIGH_ALARM);
		break;
	default:
		ret = -EOPNOTSUPP;
		break;
	}

	mutex_unlock(&data->lock);
	return ret;
}

static int demo_i2c_write(struct device *dev,
			  enum hwmon_sensor_types type,
			  u32 attr, int channel, long value)
{
	struct demo_i2c_data *data = dev_get_drvdata(dev);
	long degrees;
	int ret;

	if (type != hwmon_temp || channel != 0 || attr != hwmon_temp_max)
		return -EOPNOTSUPP;

	degrees = DIV_ROUND_CLOSEST(value, 1000);
	if (degrees < -128 || degrees > 127)
		return -ERANGE;

	mutex_lock(&data->lock);
	ret = regmap_write(data->regmap, DEMO_REG_TEMP_HIGH, (u8)(s8)degrees);
	mutex_unlock(&data->lock);

	return ret;
}

static const struct hwmon_ops demo_i2c_hwmon_ops = {
	.is_visible = demo_i2c_is_visible,
	.read = demo_i2c_read,
	.write = demo_i2c_write,
};

static const struct hwmon_channel_info * const demo_i2c_info[] = {
	HWMON_CHANNEL_INFO(temp,
			   HWMON_T_INPUT | HWMON_T_MAX | HWMON_T_ALARM),
	NULL
};

static const struct hwmon_chip_info demo_i2c_chip_info = {
	.ops = &demo_i2c_hwmon_ops,
	.info = demo_i2c_info,
};

static irqreturn_t demo_i2c_irq_thread(int irq, void *arg)
{
	struct demo_i2c_data *data = arg;
	unsigned int status;
	int ret;

	ret = regmap_read(data->regmap, DEMO_REG_STATUS, &status);
	if (ret) {
		dev_err_ratelimited(data->dev, "failed to read alert status: %d\n", ret);
		return IRQ_HANDLED;
	}

	if (status & DEMO_STATUS_HIGH_ALARM)
		dev_warn_ratelimited(data->dev, "high-temperature alert asserted\n");

	return IRQ_HANDLED;
}

static int demo_i2c_probe(struct i2c_client *client)
{
	struct device *hwmon_dev;
	struct demo_i2c_data *data;
	unsigned int device_id;
	int ret;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
		return dev_err_probe(&client->dev, -EOPNOTSUPP,
				     "adapter does not support raw I2C transfers\n");

	data = devm_kzalloc(&client->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->dev = &client->dev;
	mutex_init(&data->lock);

	data->regmap = devm_regmap_init_i2c(client, &demo_i2c_regmap_config);
	if (IS_ERR(data->regmap))
		return dev_err_probe(&client->dev, PTR_ERR(data->regmap),
				     "failed to initialize regmap\n");

	ret = regmap_read(data->regmap, DEMO_REG_DEVICE_ID, &device_id);
	if (ret)
		return dev_err_probe(&client->dev, ret, "failed to read device ID\n");

	if (device_id != DEMO_DEVICE_ID)
		return dev_err_probe(&client->dev, -ENODEV,
				     "unexpected device ID 0x%02x\n", device_id);

	i2c_set_clientdata(client, data);

	if (client->irq > 0) {
		ret = devm_request_threaded_irq(&client->dev, client->irq,
					NULL, demo_i2c_irq_thread,
					IRQF_ONESHOT,
					dev_name(&client->dev), data);
		if (ret)
			return dev_err_probe(&client->dev, ret,
					     "failed to request alert IRQ\n");
	}

	hwmon_dev = devm_hwmon_device_register_with_info(&client->dev,
				"demo_i2c_sensor", data,
				&demo_i2c_chip_info, NULL);
	if (IS_ERR(hwmon_dev))
		return dev_err_probe(&client->dev, PTR_ERR(hwmon_dev),
				     "failed to register hwmon device\n");

	dev_info(&client->dev, "demo sensor registered%s\n",
		 client->irq > 0 ? " with alert IRQ" : "");
	return 0;
}

static const struct of_device_id demo_i2c_of_match[] = {
	{ .compatible = "demo,temp-sensor" },
	{ }
};
MODULE_DEVICE_TABLE(of, demo_i2c_of_match);

static const struct i2c_device_id demo_i2c_id[] = {
	{ "demo_i2c_sensor", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, demo_i2c_id);

static struct i2c_driver demo_i2c_driver = {
	.driver = {
		.name = "demo_i2c_sensor",
		.of_match_table = demo_i2c_of_match,
	},
	.probe = demo_i2c_probe,
	.id_table = demo_i2c_id,
};
module_i2c_driver(demo_i2c_driver);

MODULE_AUTHOR("OpenBMC Linux Driver Study Project");
MODULE_DESCRIPTION("Educational I2C temperature sensor hwmon driver");
MODULE_LICENSE("GPL");

