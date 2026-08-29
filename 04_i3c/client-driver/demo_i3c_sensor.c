// SPDX-License-Identifier: GPL-2.0-only
/*
 * Educational Linux 6.18 I3C hwmon driver for a fictional temperature sensor.
 * Replace the PID, register map, IBI payload, and limits with a real datasheet.
 */

#include <linux/atomic.h>
#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/hwmon.h>
#include <linux/i3c/device.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/string.h>

#define DEMO_REG_DEVICE_ID       0x00
#define DEMO_REG_TEMP_MSB        0x01
#define DEMO_REG_TEMP_LSB        0x02
#define DEMO_REG_STATUS          0x03
#define DEMO_REG_CONFIG          0x04
#define DEMO_REG_TEMP_HIGH       0x05

#define DEMO_DEVICE_ID           0xa5
#define DEMO_STATUS_HIGH_ALARM   BIT(0)
#define DEMO_IBI_SLOTS           4
#define DEMO_IBI_MAX_PAYLOAD     2

struct demo_i3c_data {
	struct i3c_device *i3cdev;
	struct mutex lock;
	u8 tx_buf[2];
	u8 rx_buf[2];
	atomic64_t ibi_count;
	atomic_t last_ibi_status;
	bool ibi_requested;
};

static int demo_i3c_read_locked(struct demo_i3c_data *data, u8 reg,
				u8 *value, u16 length)
{
	struct i3c_priv_xfer xfers[2] = {
		{
			.rnw = false,
			.len = 1,
			.data.out = data->tx_buf,
		},
		{
			.rnw = true,
			.len = length,
			.data.in = data->rx_buf,
		},
	};
	int ret;

	if (!length || length > sizeof(data->rx_buf))
		return -EINVAL;

	data->tx_buf[0] = reg;
	ret = i3c_device_do_priv_xfers(data->i3cdev, xfers, ARRAY_SIZE(xfers));
	if (ret)
		return ret;
	if (xfers[1].actual_len != length)
		return -EIO;

	memcpy(value, data->rx_buf, length);
	return 0;
}

static int demo_i3c_write_locked(struct demo_i3c_data *data, u8 reg, u8 value)
{
	struct i3c_priv_xfer xfer = {
		.rnw = false,
		.len = 2,
		.data.out = data->tx_buf,
	};
	int ret;

	data->tx_buf[0] = reg;
	data->tx_buf[1] = value;
	ret = i3c_device_do_priv_xfers(data->i3cdev, &xfer, 1);
	if (ret)
		return ret;

	return xfer.actual_len == 2 ? 0 : -EIO;
}

static int demo_i3c_read_temp_locked(struct demo_i3c_data *data, long *value)
{
	u8 bytes[2];
	s16 raw;
	int ret;

	ret = demo_i3c_read_locked(data, DEMO_REG_TEMP_MSB,
				   bytes, sizeof(bytes));
	if (ret)
		return ret;

	raw = (s16)((bytes[0] << 8) | bytes[1]);
	raw >>= 4;
	*value = DIV_ROUND_CLOSEST((long)raw * 625, 10);
	return 0;
}

static umode_t demo_i3c_is_visible(const void *drvdata,
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

static int demo_i3c_hwmon_read(struct device *dev,
			       enum hwmon_sensor_types type,
			       u32 attr, int channel, long *value)
{
	struct demo_i3c_data *data = dev_get_drvdata(dev);
	u8 regval;
	int ret;

	if (type != hwmon_temp || channel != 0)
		return -EOPNOTSUPP;

	mutex_lock(&data->lock);
	switch (attr) {
	case hwmon_temp_input:
		ret = demo_i3c_read_temp_locked(data, value);
		break;
	case hwmon_temp_max:
		ret = demo_i3c_read_locked(data, DEMO_REG_TEMP_HIGH, &regval, 1);
		if (!ret)
			*value = (long)(s8)regval * 1000;
		break;
	case hwmon_temp_alarm:
		ret = demo_i3c_read_locked(data, DEMO_REG_STATUS, &regval, 1);
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

static int demo_i3c_hwmon_write(struct device *dev,
				enum hwmon_sensor_types type,
				u32 attr, int channel, long value)
{
	struct demo_i3c_data *data = dev_get_drvdata(dev);
	long degrees;
	int ret;

	if (type != hwmon_temp || channel != 0 || attr != hwmon_temp_max)
		return -EOPNOTSUPP;

	degrees = DIV_ROUND_CLOSEST(value, 1000);
	if (degrees < -128 || degrees > 127)
		return -ERANGE;

	mutex_lock(&data->lock);
	ret = demo_i3c_write_locked(data, DEMO_REG_TEMP_HIGH,
				    (u8)(s8)degrees);
	mutex_unlock(&data->lock);
	return ret;
}

static const struct hwmon_ops demo_i3c_hwmon_ops = {
	.is_visible = demo_i3c_is_visible,
	.read = demo_i3c_hwmon_read,
	.write = demo_i3c_hwmon_write,
};

static const struct hwmon_channel_info * const demo_i3c_info[] = {
	HWMON_CHANNEL_INFO(temp,
			   HWMON_T_INPUT | HWMON_T_MAX | HWMON_T_ALARM),
	NULL
};

static const struct hwmon_chip_info demo_i3c_chip_info = {
	.ops = &demo_i3c_hwmon_ops,
	.info = demo_i3c_info,
};

static void demo_i3c_ibi_handler(struct i3c_device *i3cdev,
				 const struct i3c_ibi_payload *payload)
{
	struct demo_i3c_data *data = i3cdev_get_drvdata(i3cdev);
	u8 status = 0;

	if (payload && payload->len)
		status = ((const u8 *)payload->data)[0];

	atomic_set(&data->last_ibi_status, status);
	atomic64_inc(&data->ibi_count);
	if (status & DEMO_STATUS_HIGH_ALARM)
		dev_warn_ratelimited(i3cdev_to_dev(i3cdev),
				     "high-temperature IBI received\n");
}

static ssize_t ibi_count_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct demo_i3c_data *data = dev_get_drvdata(dev);

	(void)attr;
	return sysfs_emit(buf, "%lld\n",
			  (long long)atomic64_read(&data->ibi_count));
}
static DEVICE_ATTR_RO(ibi_count);

static ssize_t last_ibi_status_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct demo_i3c_data *data = dev_get_drvdata(dev);

	(void)attr;
	return sysfs_emit(buf, "0x%02x\n",
			  atomic_read(&data->last_ibi_status) & 0xff);
}
static DEVICE_ATTR_RO(last_ibi_status);

static struct attribute *demo_i3c_attrs[] = {
	&dev_attr_ibi_count.attr,
	&dev_attr_last_ibi_status.attr,
	NULL,
};

static const struct attribute_group demo_i3c_attr_group = {
	.attrs = demo_i3c_attrs,
};

static int demo_i3c_probe(struct i3c_device *i3cdev)
{
	struct i3c_ibi_setup ibi_setup = {
		.max_payload_len = DEMO_IBI_MAX_PAYLOAD,
		.num_slots = DEMO_IBI_SLOTS,
		.handler = demo_i3c_ibi_handler,
	};
	struct i3c_device_info devinfo;
	struct demo_i3c_data *data;
	struct device *dev = i3cdev_to_dev(i3cdev);
	struct device *hwmon_dev;
	u8 device_id;
	int ret;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->i3cdev = i3cdev;
	mutex_init(&data->lock);
	atomic64_set(&data->ibi_count, 0);
	atomic_set(&data->last_ibi_status, 0);
	i3cdev_set_drvdata(i3cdev, data);

	mutex_lock(&data->lock);
	ret = demo_i3c_read_locked(data, DEMO_REG_DEVICE_ID, &device_id, 1);
	mutex_unlock(&data->lock);
	if (ret)
		return dev_err_probe(dev, ret, "failed to read device ID\n");
	if (device_id != DEMO_DEVICE_ID)
		return dev_err_probe(dev, -ENODEV,
				     "unexpected device ID 0x%02x\n", device_id);

	ret = devm_device_add_group(dev, &demo_i3c_attr_group);
	if (ret)
		return dev_err_probe(dev, ret, "failed to create IBI attributes\n");

	hwmon_dev = devm_hwmon_device_register_with_info(dev,
			"demo_i3c_sensor", data, &demo_i3c_chip_info, NULL);
	if (IS_ERR(hwmon_dev))
		return dev_err_probe(dev, PTR_ERR(hwmon_dev),
				     "failed to register hwmon device\n");

	ret = i3c_device_request_ibi(i3cdev, &ibi_setup);
	if (!ret) {
		data->ibi_requested = true;
		ret = i3c_device_enable_ibi(i3cdev);
		if (ret) {
			i3c_device_free_ibi(i3cdev);
			data->ibi_requested = false;
			dev_warn(dev, "IBI enable failed: %d\n", ret);
		}
	} else {
		dev_info(dev, "IBI unavailable: %d; continuing in polling mode\n",
			 ret);
	}

	i3c_device_get_info(i3cdev, &devinfo);
	dev_info(dev, "demo sensor PID %012llx at dynamic address 0x%02x%s\n",
		 (unsigned long long)devinfo.pid, devinfo.dyn_addr,
		 data->ibi_requested ? " with IBI" : "");
	return 0;
}

static void demo_i3c_remove(struct i3c_device *i3cdev)
{
	struct demo_i3c_data *data = i3cdev_get_drvdata(i3cdev);

	if (data->ibi_requested) {
		i3c_device_disable_ibi(i3cdev);
		i3c_device_free_ibi(i3cdev);
		data->ibi_requested = false;
	}
}

static const struct i3c_device_id demo_i3c_ids[] = {
	I3C_DEVICE(0x0123, 0x0456, NULL),
	{ }
};
MODULE_DEVICE_TABLE(i3c, demo_i3c_ids);

static struct i3c_driver demo_i3c_driver = {
	.driver = {
		.name = "demo_i3c_sensor",
	},
	.probe = demo_i3c_probe,
	.remove = demo_i3c_remove,
	.id_table = demo_i3c_ids,
};
module_i3c_driver(demo_i3c_driver);

MODULE_AUTHOR("OpenBMC Linux Driver Study Project");
MODULE_DESCRIPTION("Educational Linux 6.18 I3C temperature sensor driver");
MODULE_LICENSE("GPL");
