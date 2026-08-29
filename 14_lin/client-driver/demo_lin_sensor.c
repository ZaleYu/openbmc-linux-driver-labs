// SPDX-License-Identifier: GPL-2.0-only
/* Educational UART/serdev LIN master for a fictional temperature node. */
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/hwmon.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/property.h>
#include <linux/serdev.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <asm/unaligned.h>

#define DEMO_LIN_BAUD             19200
#define DEMO_LIN_DEFAULT_ID       0x12
#define DEMO_LIN_DEFAULT_POLL_MS  1000
#define DEMO_LIN_RESPONSE_LEN     3

struct demo_lin {
	struct serdev_device *serdev;
	struct delayed_work poll_work;
	struct mutex lock;
	struct gpio_desc *enable_gpio;
	u8 frame_id;
	u8 protected_id;
	u8 rx[DEMO_LIN_RESPONSE_LEN];
	size_t rx_len;
	u32 poll_interval_ms;
	long temperature_mc;
	unsigned long last_valid;
	bool valid;
	bool stopping;
};

static u8 demo_lin_make_pid(u8 id)
{
	u8 p0 = ((id >> 0) ^ (id >> 1) ^ (id >> 2) ^ (id >> 4)) & 1;
	u8 p1 = (~((id >> 1) ^ (id >> 3) ^ (id >> 4) ^ (id >> 5))) & 1;

	return (id & 0x3f) | (p0 << 6) | (p1 << 7);
}

static u8 demo_lin_checksum(u8 pid, const u8 *data, size_t len)
{
	u16 sum = pid;
	size_t i;

	for (i = 0; i < len; i++) {
		sum += data[i];
		if (sum > 0xff)
			sum = (sum & 0xff) + 1;
	}
	return (u8)~sum;
}

static size_t demo_lin_receive_buf(struct serdev_device *serdev,
				    const u8 *data, size_t count)
{
	struct demo_lin *lin = serdev_device_get_drvdata(serdev);
	size_t i;

	mutex_lock(&lin->lock);
	for (i = 0; i < count; i++) {
		if (lin->rx_len < sizeof(lin->rx))
			lin->rx[lin->rx_len++] = data[i];
		if (lin->rx_len == sizeof(lin->rx)) {
			u8 expected = demo_lin_checksum(lin->protected_id,
							lin->rx, 2);

			if (expected == lin->rx[2]) {
				s16 raw = (s16)get_unaligned_le16(lin->rx);

				lin->temperature_mc = (long)raw * 100;
				lin->last_valid = jiffies;
				lin->valid = true;
			} else {
				dev_warn_ratelimited(&serdev->dev,
					"checksum mismatch: received 0x%02x expected 0x%02x\n",
					lin->rx[2], expected);
			}
			lin->rx_len = 0;
		}
	}
	mutex_unlock(&lin->lock);
	return count;
}

static const struct serdev_device_ops demo_lin_serdev_ops = {
	.receive_buf = demo_lin_receive_buf,
};

static void demo_lin_poll_work(struct work_struct *work)
{
	struct demo_lin *lin = container_of(to_delayed_work(work),
					  struct demo_lin, poll_work);
	u8 header[2] = { 0x55, lin->protected_id };
	ssize_t written;
	int ret;

	mutex_lock(&lin->lock);
	if (lin->stopping) {
		mutex_unlock(&lin->lock);
		return;
	}
	lin->rx_len = 0;
	mutex_unlock(&lin->lock);

	/* 800 us exceeds 13 nominal bits at 19200 baud; measure actual hardware. */
	ret = serdev_device_break_ctl(lin->serdev, 1);
	if (ret) {
		dev_err_ratelimited(&lin->serdev->dev,
				    "UART cannot assert LIN break: %d\n", ret);
		goto reschedule;
	}
	usleep_range(800, 900);
	ret = serdev_device_break_ctl(lin->serdev, 0);
	if (ret) {
		dev_err_ratelimited(&lin->serdev->dev,
				    "UART cannot release LIN break: %d\n", ret);
		goto reschedule;
	}
	usleep_range(100, 150);
	written = serdev_device_write(lin->serdev, header, sizeof(header), HZ);
	if (written != (ssize_t)sizeof(header))
		dev_warn_ratelimited(&lin->serdev->dev,
				     "header write returned %zd\n", written);

reschedule:
	mutex_lock(&lin->lock);
	if (!lin->stopping)
		schedule_delayed_work(&lin->poll_work,
				      msecs_to_jiffies(lin->poll_interval_ms));
	mutex_unlock(&lin->lock);
}

static umode_t demo_lin_hwmon_is_visible(const void *data,
					 enum hwmon_sensor_types type,
					 u32 attr, int channel)
{
	(void)data;
	(void)channel;
	if (type == hwmon_temp && attr == hwmon_temp_input)
		return 0444;
	return 0;
}

static int demo_lin_hwmon_read(struct device *dev,
			       enum hwmon_sensor_types type,
			       u32 attr, int channel, long *value)
{
	struct demo_lin *lin = dev_get_drvdata(dev);
	unsigned long stale_after;
	int ret = 0;

	(void)channel;
	if (type != hwmon_temp || attr != hwmon_temp_input)
		return -EOPNOTSUPP;

	stale_after = msecs_to_jiffies(lin->poll_interval_ms * 3U);
	mutex_lock(&lin->lock);
	if (!lin->valid || time_after(jiffies, lin->last_valid + stale_after))
		ret = -ENODATA;
	else
		*value = lin->temperature_mc;
	mutex_unlock(&lin->lock);
	return ret;
}

static const struct hwmon_ops demo_lin_hwmon_ops = {
	.is_visible = demo_lin_hwmon_is_visible,
	.read = demo_lin_hwmon_read,
};

static const struct hwmon_channel_info * const demo_lin_hwmon_info[] = {
	HWMON_CHANNEL_INFO(temp, HWMON_T_INPUT),
	NULL,
};

static const struct hwmon_chip_info demo_lin_chip_info = {
	.ops = &demo_lin_hwmon_ops,
	.info = demo_lin_hwmon_info,
};

static int demo_lin_probe(struct serdev_device *serdev)
{
	struct device *hwmon;
	struct demo_lin *lin;
	u32 value;
	unsigned int actual_baud;
	int ret;

	lin = devm_kzalloc(&serdev->dev, sizeof(*lin), GFP_KERNEL);
	if (!lin)
		return -ENOMEM;
	lin->serdev = serdev;
	lin->frame_id = DEMO_LIN_DEFAULT_ID;
	lin->poll_interval_ms = DEMO_LIN_DEFAULT_POLL_MS;
	if (!device_property_read_u32(&serdev->dev, "demo,lin-frame-id", &value)) {
		if (value > 0x3b)
			return dev_err_probe(&serdev->dev, -EINVAL,
					     "lin-frame-id must be 0x00..0x3b\n");
		lin->frame_id = value;
	}
	if (!device_property_read_u32(&serdev->dev, "demo,poll-interval-ms", &value)) {
		if (value < 10 || value > 60000)
			return dev_err_probe(&serdev->dev, -EINVAL,
					     "poll interval must be 10..60000 ms\n");
		lin->poll_interval_ms = value;
	}
	lin->protected_id = demo_lin_make_pid(lin->frame_id);
	mutex_init(&lin->lock);
	INIT_DELAYED_WORK(&lin->poll_work, demo_lin_poll_work);
	serdev_device_set_drvdata(serdev, lin);
	serdev_device_set_client_ops(serdev, &demo_lin_serdev_ops);

	lin->enable_gpio = devm_gpiod_get_optional(&serdev->dev, "enable",
						    GPIOD_OUT_HIGH);
	if (IS_ERR(lin->enable_gpio))
		return dev_err_probe(&serdev->dev, PTR_ERR(lin->enable_gpio),
				     "transceiver enable GPIO\n");
	ret = devm_serdev_device_open(&serdev->dev, serdev);
	if (ret)
		return dev_err_probe(&serdev->dev, ret, "open UART\n");
	serdev_device_set_flow_control(serdev, false);
	ret = serdev_device_set_parity(serdev, SERDEV_PARITY_NONE);
	if (ret)
		return dev_err_probe(&serdev->dev, ret, "set parity\n");
	actual_baud = serdev_device_set_baudrate(serdev, DEMO_LIN_BAUD);
	if (!actual_baud)
		return dev_err_probe(&serdev->dev, -EINVAL, "set baud rate\n");
	if (actual_baud != DEMO_LIN_BAUD)
		dev_warn(&serdev->dev, "requested %u baud, got %u\n",
			 DEMO_LIN_BAUD, actual_baud);

	hwmon = devm_hwmon_device_register_with_info(&serdev->dev,
				"demo_lin_temp", lin, &demo_lin_chip_info, NULL);
	if (IS_ERR(hwmon))
		return dev_err_probe(&serdev->dev, PTR_ERR(hwmon),
				     "register hwmon\n");

	schedule_delayed_work(&lin->poll_work, 0);
	dev_info(&serdev->dev, "LIN demo sensor ID 0x%02x PID 0x%02x\n",
		 lin->frame_id, lin->protected_id);
	return 0;
}

static void demo_lin_remove(struct serdev_device *serdev)
{
	struct demo_lin *lin = serdev_device_get_drvdata(serdev);

	mutex_lock(&lin->lock);
	lin->stopping = true;
	mutex_unlock(&lin->lock);
	cancel_delayed_work_sync(&lin->poll_work);
	serdev_device_write_flush(serdev);
	if (lin->enable_gpio)
		gpiod_set_value_cansleep(lin->enable_gpio, 0);
}

static const struct of_device_id demo_lin_of_match[] = {
	{ .compatible = "demo,lin-temp-node" },
	{ }
};
MODULE_DEVICE_TABLE(of, demo_lin_of_match);

static struct serdev_device_driver demo_lin_driver = {
	.probe = demo_lin_probe,
	.remove = demo_lin_remove,
	.driver = {
		.name = "demo-lin-sensor",
		.of_match_table = demo_lin_of_match,
	},
};
module_serdev_device_driver(demo_lin_driver);

MODULE_AUTHOR("OpenBMC peripheral study project");
MODULE_DESCRIPTION("Educational UART/serdev LIN temperature driver");
MODULE_LICENSE("GPL");
