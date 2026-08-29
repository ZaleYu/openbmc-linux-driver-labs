// SPDX-License-Identifier: GPL-2.0-only
/*
 * Educational GPIO consumer and threaded-IRQ platform driver.
 * Replace the fictional binding and signal policy with the real board design.
 */

#include <linux/atomic.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/kstrtox.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/timekeeping.h>

struct demo_gpio_data {
	struct device *dev;
	struct gpio_desc *presence;
	struct gpio_desc *fault;
	struct gpio_desc *reset;
	atomic64_t event_count;
	atomic64_t last_event_ns;
};

static ssize_t present_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	struct demo_gpio_data *data = dev_get_drvdata(dev);
	int value;

	value = gpiod_get_value_cansleep(data->presence);
	if (value < 0)
		return value;

	return sysfs_emit(buf, "%d\n", value);
}
static DEVICE_ATTR_RO(present);

static ssize_t fault_show(struct device *dev,
			  struct device_attribute *attr, char *buf)
{
	struct demo_gpio_data *data = dev_get_drvdata(dev);
	int value;

	if (!data->fault)
		return -ENODEV;

	value = gpiod_get_value_cansleep(data->fault);
	if (value < 0)
		return value;

	return sysfs_emit(buf, "%d\n", value);
}
static DEVICE_ATTR_RO(fault);

static ssize_t event_count_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct demo_gpio_data *data = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%lld\n", atomic64_read(&data->event_count));
}
static DEVICE_ATTR_RO(event_count);

static ssize_t last_event_ns_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct demo_gpio_data *data = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%lld\n", atomic64_read(&data->last_event_ns));
}
static DEVICE_ATTR_RO(last_event_ns);

static ssize_t reset_show(struct device *dev,
			  struct device_attribute *attr, char *buf)
{
	struct demo_gpio_data *data = dev_get_drvdata(dev);
	int value;

	if (!data->reset)
		return -ENODEV;

	value = gpiod_get_value_cansleep(data->reset);
	if (value < 0)
		return value;

	return sysfs_emit(buf, "%d\n", value);
}

static ssize_t reset_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct demo_gpio_data *data = dev_get_drvdata(dev);
	bool asserted;
	int ret;

	if (!data->reset)
		return -ENODEV;

	ret = kstrtobool(buf, &asserted);
	if (ret)
		return ret;

	/* Logical 1 means asserted; Device Tree handles active-low polarity. */
	gpiod_set_value_cansleep(data->reset, asserted);
	dev_info(dev, "reset is now %s\n", asserted ? "asserted" : "deasserted");

	return count;
}
static DEVICE_ATTR_RW(reset);

static struct attribute *demo_gpio_attrs[] = {
	&dev_attr_present.attr,
	&dev_attr_fault.attr,
	&dev_attr_event_count.attr,
	&dev_attr_last_event_ns.attr,
	&dev_attr_reset.attr,
	NULL,
};

static umode_t demo_gpio_attr_is_visible(struct kobject *kobj,
					struct attribute *attr, int index)
{
	struct device *dev = kobj_to_dev(kobj);
	struct demo_gpio_data *data = dev_get_drvdata(dev);

	if (attr == &dev_attr_fault.attr && !data->fault)
		return 0;
	if (attr == &dev_attr_reset.attr && !data->reset)
		return 0;

	return attr->mode;
}

static const struct attribute_group demo_gpio_group = {
	.attrs = demo_gpio_attrs,
	.is_visible = demo_gpio_attr_is_visible,
};

static void demo_gpio_record_event(struct demo_gpio_data *data,
				   const char *name,
				   struct gpio_desc *desc,
				   const char *attribute)
{
	int value;

	value = gpiod_get_value_cansleep(desc);
	atomic64_inc(&data->event_count);
	atomic64_set(&data->last_event_ns, ktime_get_ns());

	if (value < 0)
		dev_err_ratelimited(data->dev, "%s read failed: %d\n", name, value);
	else
		dev_info_ratelimited(data->dev, "%s changed: logical=%d\n",
				     name, value);

	sysfs_notify(&data->dev->kobj, NULL, attribute);
	sysfs_notify(&data->dev->kobj, NULL, "event_count");
	sysfs_notify(&data->dev->kobj, NULL, "last_event_ns");
}

static irqreturn_t demo_presence_irq_thread(int irq, void *arg)
{
	struct demo_gpio_data *data = arg;

	demo_gpio_record_event(data, "presence", data->presence, "present");
	return IRQ_HANDLED;
}

static irqreturn_t demo_fault_irq_thread(int irq, void *arg)
{
	struct demo_gpio_data *data = arg;

	demo_gpio_record_event(data, "fault", data->fault, "fault");
	return IRQ_HANDLED;
}

static int demo_gpio_apply_debounce(struct demo_gpio_data *data,
				    struct gpio_desc *desc,
				    const char *name, u32 debounce_us)
{
	int ret;

	if (!desc || !debounce_us)
		return 0;

	ret = gpiod_set_debounce(desc, debounce_us);
	if (ret == -ENOTSUPP)
		dev_warn(data->dev,
			 "%s GPIO does not support hardware debounce; apply policy elsewhere\n",
			 name);
	else if (ret)
		return dev_err_probe(data->dev, ret,
				     "failed to configure %s debounce\n", name);

	return 0;
}

static int demo_gpio_request_irq(struct demo_gpio_data *data,
				 struct gpio_desc *desc,
				 irq_handler_t thread_fn,
				 const char *name)
{
	int irq;

	irq = gpiod_to_irq(desc);
	if (irq < 0)
		return dev_err_probe(data->dev, irq,
				     "failed to map %s GPIO to IRQ\n", name);

	return devm_request_threaded_irq(data->dev, irq, NULL, thread_fn,
					 IRQF_ONESHOT | IRQF_TRIGGER_RISING |
					 IRQF_TRIGGER_FALLING,
					 name, data);
}

static int demo_gpio_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct demo_gpio_data *data;
	u32 debounce_us = 0;
	int ret;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->dev = dev;
	atomic64_set(&data->event_count, 0);
	atomic64_set(&data->last_event_ns, 0);
	platform_set_drvdata(pdev, data);

	data->presence = devm_gpiod_get(dev, "presence", GPIOD_IN);
	if (IS_ERR(data->presence))
		return dev_err_probe(dev, PTR_ERR(data->presence),
				     "failed to request presence GPIO\n");

	data->fault = devm_gpiod_get_optional(dev, "fault", GPIOD_IN);
	if (IS_ERR(data->fault))
		return dev_err_probe(dev, PTR_ERR(data->fault),
				     "failed to request fault GPIO\n");

	/* GPIOD_OUT_LOW means logical inactive, including for active-low lines. */
	data->reset = devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(data->reset))
		return dev_err_probe(dev, PTR_ERR(data->reset),
				     "failed to request reset GPIO\n");

	device_property_read_u32(dev, "debounce-interval-us", &debounce_us);

	ret = demo_gpio_apply_debounce(data, data->presence, "presence", debounce_us);
	if (ret)
		return ret;

	ret = demo_gpio_apply_debounce(data, data->fault, "fault", debounce_us);
	if (ret)
		return ret;

	ret = demo_gpio_request_irq(data, data->presence,
				    demo_presence_irq_thread, "demo-presence");
	if (ret)
		return dev_err_probe(dev, ret, "failed to request presence IRQ\n");

	if (data->fault) {
		ret = demo_gpio_request_irq(data, data->fault,
					    demo_fault_irq_thread, "demo-fault");
		if (ret)
			return dev_err_probe(dev, ret, "failed to request fault IRQ\n");
	}

	ret = devm_device_add_group(dev, &demo_gpio_group);
	if (ret)
		return dev_err_probe(dev, ret, "failed to create sysfs attributes\n");

	dev_info(dev, "GPIO monitor registered%s%s\n",
		 data->fault ? " with fault input" : "",
		 data->reset ? " and reset output" : "");
	return 0;
}

static const struct of_device_id demo_gpio_of_match[] = {
	{ .compatible = "demo,gpio-irq-monitor" },
	{ }
};
MODULE_DEVICE_TABLE(of, demo_gpio_of_match);

static struct platform_driver demo_gpio_driver = {
	.probe = demo_gpio_probe,
	.driver = {
		.name = "demo_gpio_irq",
		.of_match_table = demo_gpio_of_match,
	},
};
module_platform_driver(demo_gpio_driver);

MODULE_AUTHOR("OpenBMC Linux Driver Study Project");
MODULE_DESCRIPTION("Educational GPIO consumer and threaded IRQ driver");
MODULE_LICENSE("GPL");
