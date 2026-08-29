// SPDX-License-Identifier: GPL-2.0
/* Educational serdev client for a fictional UART management MCU. */
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/serdev.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/workqueue.h>

#define DEMO_BAUD		115200
#define DEMO_PING_INTERVAL	(5 * HZ)
#define DEMO_LINE_MAX		64

struct demo_uart_mcu {
	struct serdev_device *serdev;
	struct delayed_work ping_work;
	spinlock_t rx_lock;
	char rx_line[DEMO_LINE_MAX];
	char last_line[DEMO_LINE_MAX];
	size_t rx_len;
	u64 lines;
	u64 overflows;
	bool discarding;
	bool stopping;
};

static size_t demo_uart_receive_buf(struct serdev_device *serdev,
				    const unsigned char *buf, size_t count)
{
	struct demo_uart_mcu *mcu = serdev_device_get_drvdata(serdev);
	unsigned long flags;
	u64 lines, overflows;
	size_t i;

	spin_lock_irqsave(&mcu->rx_lock, flags);
	for (i = 0; i < count; i++) {
		if (buf[i] == '\n') {
			if (mcu->discarding) {
				mcu->discarding = false;
				mcu->rx_len = 0;
				continue;
			}
			mcu->rx_line[mcu->rx_len] = '\0';
			strscpy(mcu->last_line, mcu->rx_line,
				sizeof(mcu->last_line));
			mcu->rx_len = 0;
			mcu->lines++;
			continue;
		}
		if (buf[i] == '\r')
			continue;
		if (mcu->discarding)
			continue;
		if (mcu->rx_len + 1 < sizeof(mcu->rx_line)) {
			mcu->rx_line[mcu->rx_len++] = buf[i];
		} else {
			mcu->rx_len = 0;
			mcu->discarding = true;
			mcu->overflows++;
		}
	}
	lines = mcu->lines;
	overflows = mcu->overflows;
	spin_unlock_irqrestore(&mcu->rx_lock, flags);

	dev_dbg_ratelimited(&serdev->dev, "rx bytes=%zu lines=%llu overflow=%llu\n",
			    count, (unsigned long long)lines,
			    (unsigned long long)overflows);
	return count;
}

static const struct serdev_device_ops demo_uart_ops = {
	.receive_buf = demo_uart_receive_buf,
	.write_wakeup = serdev_device_write_wakeup,
};

static void demo_uart_ping_work(struct work_struct *work)
{
	struct demo_uart_mcu *mcu =
		container_of(to_delayed_work(work), struct demo_uart_mcu,
			     ping_work);
	static const unsigned char ping[] = "PING\n";
	int written;

	/* Synchronous helper handles controller short writes via write_wakeup. */
	written = serdev_device_write(mcu->serdev, ping, sizeof(ping) - 1, HZ);
	if (written < 0)
		dev_warn_ratelimited(&mcu->serdev->dev,
				     "PING write failed: %d\n", written);
	else if (written != sizeof(ping) - 1)
		dev_warn_ratelimited(&mcu->serdev->dev,
				     "short PING write: %d\n", written);
	if (!READ_ONCE(mcu->stopping))
		schedule_delayed_work(&mcu->ping_work, DEMO_PING_INTERVAL);
}

static void demo_uart_cleanup(void *arg)
{
	struct demo_uart_mcu *mcu = arg;

	WRITE_ONCE(mcu->stopping, true);
	cancel_delayed_work_sync(&mcu->ping_work);
	serdev_device_close(mcu->serdev);
}

static int demo_uart_probe(struct serdev_device *serdev)
{
	struct demo_uart_mcu *mcu;
	unsigned int actual_baud;
	int ret;

	mcu = devm_kzalloc(&serdev->dev, sizeof(*mcu), GFP_KERNEL);
	if (!mcu)
		return -ENOMEM;
	mcu->serdev = serdev;
	spin_lock_init(&mcu->rx_lock);
	INIT_DELAYED_WORK(&mcu->ping_work, demo_uart_ping_work);
	serdev_device_set_drvdata(serdev, mcu);
	serdev_device_set_client_ops(serdev, &demo_uart_ops);

	ret = serdev_device_open(serdev);
	if (ret)
		return dev_err_probe(&serdev->dev, ret, "cannot open UART\n");
	ret = devm_add_action_or_reset(&serdev->dev, demo_uart_cleanup, mcu);
	if (ret)
		return ret;

	actual_baud = serdev_device_set_baudrate(serdev, DEMO_BAUD);
	if (!actual_baud)
		return dev_err_probe(&serdev->dev, -EINVAL,
				     "cannot configure baud rate\n");
	serdev_device_set_flow_control(serdev, false);
	dev_info(&serdev->dev, "management MCU at %u baud\n", actual_baud);
	schedule_delayed_work(&mcu->ping_work, DEMO_PING_INTERVAL);
	return 0;
}

static const struct of_device_id demo_uart_of_match[] = {
	{ .compatible = "openai,demo-uart-mcu" },
	{ }
};
MODULE_DEVICE_TABLE(of, demo_uart_of_match);

static struct serdev_device_driver demo_uart_driver = {
	.probe = demo_uart_probe,
	.driver = {
		.name = "demo_uart_mcu",
		.of_match_table = demo_uart_of_match,
	},
};
module_serdev_device_driver(demo_uart_driver);

MODULE_AUTHOR("OpenBMC Peripheral Driver Study");
MODULE_DESCRIPTION("Educational UART management MCU serdev client");
MODULE_LICENSE("GPL");
