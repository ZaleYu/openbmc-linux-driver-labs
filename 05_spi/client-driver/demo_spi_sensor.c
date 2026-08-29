// SPDX-License-Identifier: GPL-2.0
/* Educational driver for a fictional SPI temperature/status device. */
#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/hwmon.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/spi/spi.h>

#define DEMO_REG_ID		0x00
#define DEMO_REG_TEMP_MSB	0x01
#define DEMO_REG_STATUS		0x03
#define DEMO_REG_LIMIT_MSB	0x04
#define DEMO_ID			0x5a
#define DEMO_READ		BIT(7)
#define DEMO_STATUS_ALARM	BIT(0)

struct demo_spi_sensor {
	struct spi_device *spi;
	struct mutex lock;
};

static int demo_read(struct demo_spi_sensor *data, u8 reg, void *buf, size_t len)
{
	u8 cmd = DEMO_READ | reg;

	return spi_write_then_read(data->spi, &cmd, sizeof(cmd), buf, len);
}

static int demo_write16(struct demo_spi_sensor *data, u8 reg, s16 value)
{
	u8 tx[] = { reg, (u8)(value >> 8), (u8)value };

	return spi_write(data->spi, tx, sizeof(tx));
}

static int demo_read16(struct demo_spi_sensor *data, u8 reg, s16 *value)
{
	u8 rx[2];
	int ret;

	ret = demo_read(data, reg, rx, sizeof(rx));
	if (!ret)
		*value = (s16)((rx[0] << 8) | rx[1]);
	return ret;
}

static umode_t demo_is_visible(const void *drvdata,
			       enum hwmon_sensor_types type, u32 attr, int channel)
{
	if (type != hwmon_temp || channel != 0)
		return 0;
	if (attr == hwmon_temp_input || attr == hwmon_temp_max_alarm)
		return 0444;
	if (attr == hwmon_temp_max)
		return 0644;
	return 0;
}

static int demo_hwmon_read(struct device *dev, enum hwmon_sensor_types type,
			   u32 attr, int channel, long *val)
{
	struct demo_spi_sensor *data = dev_get_drvdata(dev);
	s16 raw;
	u8 status;
	int ret;

	if (type != hwmon_temp || channel != 0)
		return -EOPNOTSUPP;
	mutex_lock(&data->lock);
	if (attr == hwmon_temp_input)
		ret = demo_read16(data, DEMO_REG_TEMP_MSB, &raw);
	else if (attr == hwmon_temp_max)
		ret = demo_read16(data, DEMO_REG_LIMIT_MSB, &raw);
	else if (attr == hwmon_temp_max_alarm) {
		ret = demo_read(data, DEMO_REG_STATUS, &status, 1);
		if (!ret)
			*val = !!(status & DEMO_STATUS_ALARM);
		goto out;
	} else {
		ret = -EOPNOTSUPP;
		goto out;
	}
	if (!ret)
		*val = (long)raw * 10; /* centi-C to milli-C */
out:
	mutex_unlock(&data->lock);
	return ret;
}

static int demo_hwmon_write(struct device *dev, enum hwmon_sensor_types type,
			    u32 attr, int channel, long val)
{
	struct demo_spi_sensor *data = dev_get_drvdata(dev);
	int ret;

	if (type != hwmon_temp || attr != hwmon_temp_max || channel != 0)
		return -EOPNOTSUPP;
	if (val < -327680 || val > 327670)
		return -ERANGE;
	mutex_lock(&data->lock);
	ret = demo_write16(data, DEMO_REG_LIMIT_MSB, val / 10);
	mutex_unlock(&data->lock);
	return ret;
}

static const struct hwmon_ops demo_hwmon_ops = {
	.is_visible = demo_is_visible,
	.read = demo_hwmon_read,
	.write = demo_hwmon_write,
};

static const struct hwmon_channel_info * const demo_info[] = {
	HWMON_CHANNEL_INFO(temp, HWMON_T_INPUT | HWMON_T_MAX |
			   HWMON_T_MAX_ALARM),
	NULL
};

static const struct hwmon_chip_info demo_chip_info = {
	.ops = &demo_hwmon_ops,
	.info = demo_info,
};

static int demo_probe(struct spi_device *spi)
{
	struct demo_spi_sensor *data;
	struct device *hwmon;
	u8 id;
	int ret;

	if (spi->mode != SPI_MODE_0)
		return dev_err_probe(&spi->dev, -EINVAL, "SPI mode 0 required\n");
	data = devm_kzalloc(&spi->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;
	data->spi = spi;
	mutex_init(&data->lock);
	spi_set_drvdata(spi, data);
	ret = demo_read(data, DEMO_REG_ID, &id, 1);
	if (ret)
		return dev_err_probe(&spi->dev, ret, "cannot read device ID\n");
	if (id != DEMO_ID)
		return dev_err_probe(&spi->dev, -ENODEV, "unexpected ID 0x%02x\n", id);
	hwmon = devm_hwmon_device_register_with_info(&spi->dev, "demo_spi_sensor",
			data, &demo_chip_info, NULL);
	return PTR_ERR_OR_ZERO(hwmon);
}

static const struct of_device_id demo_of_match[] = {
	{ .compatible = "openai,demo-spi-sensor" },
	{ }
};
MODULE_DEVICE_TABLE(of, demo_of_match);

static struct spi_driver demo_driver = {
	.driver = {
		.name = "demo_spi_sensor",
		.of_match_table = demo_of_match,
	},
	.probe = demo_probe,
};
module_spi_driver(demo_driver);

MODULE_AUTHOR("OpenBMC Peripheral Driver Study");
MODULE_DESCRIPTION("Educational SPI hwmon sensor driver");
MODULE_LICENSE("GPL");

