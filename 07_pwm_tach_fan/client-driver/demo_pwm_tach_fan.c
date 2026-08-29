// SPDX-License-Identifier: GPL-2.0
/* Educational PWM consumer + GPIO tachometer hwmon driver. */
#include <linux/gpio/consumer.h>
#include <linux/hwmon.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/math64.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/pwm.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>

struct demo_fan {
	struct pwm_device *pwm;
	struct gpio_desc *tach;
	struct mutex pwm_lock;
	spinlock_t pulse_lock;
	struct delayed_work sample_work;
	unsigned long pulse_count;
	unsigned long last_sample;
	u32 pulses_per_rev;
	u32 rpm;
	u32 min_rpm;
	u8 pwm_value;
};

static int demo_fan_apply_pwm(struct demo_fan *fan, u8 value)
{
	struct pwm_args args;
	struct pwm_state state;

	pwm_get_args(fan->pwm, &args);
	pwm_get_state(fan->pwm, &state);
	if (!state.period)
		state.period = args.period;
	if (!state.period)
		return -EINVAL;
	state.duty_cycle = DIV_ROUND_CLOSEST_ULL(state.period * value, 255);
	state.enabled = true;
	return pwm_apply_might_sleep(fan->pwm, &state);
}

static irqreturn_t demo_fan_tach_irq(int irq, void *arg)
{
	struct demo_fan *fan = arg;

	(void)irq;
	spin_lock(&fan->pulse_lock);
	fan->pulse_count++;
	spin_unlock(&fan->pulse_lock);
	return IRQ_HANDLED;
}

static void demo_fan_sample_work(struct work_struct *work)
{
	struct demo_fan *fan =
		container_of(to_delayed_work(work), struct demo_fan, sample_work);
	unsigned long flags, now = jiffies;
	unsigned long pulses;
	u32 elapsed_ms;
	u64 divisor;

	spin_lock_irqsave(&fan->pulse_lock, flags);
	pulses = fan->pulse_count;
	fan->pulse_count = 0;
	spin_unlock_irqrestore(&fan->pulse_lock, flags);

	elapsed_ms = jiffies_to_msecs(now - fan->last_sample);
	fan->last_sample = now;
	divisor = (u64)fan->pulses_per_rev * elapsed_ms;
	if (divisor)
		WRITE_ONCE(fan->rpm,
			   (u32)div_u64((u64)pulses * 60000, divisor));
	schedule_delayed_work(&fan->sample_work, HZ);
}

static umode_t demo_fan_is_visible(const void *drvdata,
				   enum hwmon_sensor_types type,
				   u32 attr, int channel)
{
	(void)drvdata;
	if (channel != 0)
		return 0;
	if (type == hwmon_fan) {
		if (attr == hwmon_fan_input)
			return 0444;
		if (attr == hwmon_fan_min)
			return 0644;
	}
	if (type == hwmon_pwm && attr == hwmon_pwm_input)
		return 0644;
	return 0;
}

static int demo_fan_read(struct device *dev, enum hwmon_sensor_types type,
			 u32 attr, int channel, long *val)
{
	struct demo_fan *fan = dev_get_drvdata(dev);

	if (channel != 0)
		return -EOPNOTSUPP;
	if (type == hwmon_fan && attr == hwmon_fan_input)
		*val = READ_ONCE(fan->rpm);
	else if (type == hwmon_fan && attr == hwmon_fan_min)
		*val = fan->min_rpm;
	else if (type == hwmon_pwm && attr == hwmon_pwm_input)
		*val = fan->pwm_value;
	else
		return -EOPNOTSUPP;
	return 0;
}

static int demo_fan_write(struct device *dev, enum hwmon_sensor_types type,
			  u32 attr, int channel, long val)
{
	struct demo_fan *fan = dev_get_drvdata(dev);
	int ret = 0;

	if (channel != 0)
		return -EOPNOTSUPP;
	if (type == hwmon_fan && attr == hwmon_fan_min) {
		fan->min_rpm = clamp_val(val, 0, 100000);
		return 0;
	}
	if (type != hwmon_pwm || attr != hwmon_pwm_input)
		return -EOPNOTSUPP;

	val = clamp_val(val, 0, 255);
	mutex_lock(&fan->pwm_lock);
	ret = demo_fan_apply_pwm(fan, val);
	if (!ret)
		fan->pwm_value = val;
	mutex_unlock(&fan->pwm_lock);
	return ret;
}

static const struct hwmon_ops demo_fan_hwmon_ops = {
	.is_visible = demo_fan_is_visible,
	.read = demo_fan_read,
	.write = demo_fan_write,
};

static const struct hwmon_channel_info * const demo_fan_info[] = {
	HWMON_CHANNEL_INFO(fan, HWMON_F_INPUT | HWMON_F_MIN),
	HWMON_CHANNEL_INFO(pwm, HWMON_PWM_INPUT),
	NULL
};

static const struct hwmon_chip_info demo_fan_chip_info = {
	.ops = &demo_fan_hwmon_ops,
	.info = demo_fan_info,
};

static void demo_fan_cleanup(void *arg)
{
	struct demo_fan *fan = arg;

	cancel_delayed_work_sync(&fan->sample_work);
	mutex_lock(&fan->pwm_lock);
	if (!demo_fan_apply_pwm(fan, 255))
		fan->pwm_value = 255;
	mutex_unlock(&fan->pwm_lock);
}

static int demo_fan_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct demo_fan *fan;
	struct device *hwmon;
	u32 value;
	int irq, ret;

	fan = devm_kzalloc(dev, sizeof(*fan), GFP_KERNEL);
	if (!fan)
		return -ENOMEM;
	fan->pwm = devm_pwm_get(dev, NULL);
	if (IS_ERR(fan->pwm))
		return dev_err_probe(dev, PTR_ERR(fan->pwm), "cannot get PWM\n");
	fan->tach = devm_gpiod_get(dev, "tach", GPIOD_IN);
	if (IS_ERR(fan->tach))
		return dev_err_probe(dev, PTR_ERR(fan->tach),
				     "cannot get tach GPIO\n");
	irq = gpiod_to_irq(fan->tach);
	if (irq < 0)
		return dev_err_probe(dev, irq, "cannot map tach IRQ\n");

	fan->pulses_per_rev = 2;
	if (!device_property_read_u32(dev, "pulses-per-revolution", &value)) {
		if (value < 1 || value > 16)
			return dev_err_probe(dev, -EINVAL, "invalid pulses per revolution\n");
		fan->pulses_per_rev = value;
	}
	fan->min_rpm = 1000;
	device_property_read_u32(dev, "fan-min-rpm", &fan->min_rpm);
	mutex_init(&fan->pwm_lock);
	spin_lock_init(&fan->pulse_lock);
	INIT_DELAYED_WORK(&fan->sample_work, demo_fan_sample_work);

	ret = demo_fan_apply_pwm(fan, 255);
	if (ret)
		return dev_err_probe(dev, ret, "cannot set fail-safe PWM\n");
	fan->pwm_value = 255;
	ret = devm_add_action_or_reset(dev, demo_fan_cleanup, fan);
	if (ret)
		return ret;
	ret = devm_request_irq(dev, irq, demo_fan_tach_irq, IRQF_TRIGGER_RISING,
			       dev_name(dev), fan);
	if (ret)
		return dev_err_probe(dev, ret, "cannot request tach IRQ\n");

	hwmon = devm_hwmon_device_register_with_info(dev, "demo_pwm_tach_fan",
						    fan, &demo_fan_chip_info,
						    NULL);
	if (IS_ERR(hwmon))
		return PTR_ERR(hwmon);
	platform_set_drvdata(pdev, fan);
	fan->last_sample = jiffies;
	schedule_delayed_work(&fan->sample_work, HZ);
	return 0;
}

static const struct of_device_id demo_fan_of_match[] = {
	{ .compatible = "openai,demo-pwm-tach-fan" },
	{ }
};
MODULE_DEVICE_TABLE(of, demo_fan_of_match);

static struct platform_driver demo_fan_driver = {
	.probe = demo_fan_probe,
	.driver = {
		.name = "demo_pwm_tach_fan",
		.of_match_table = demo_fan_of_match,
	},
};
module_platform_driver(demo_fan_driver);

MODULE_AUTHOR("OpenBMC Peripheral Driver Study");
MODULE_DESCRIPTION("Educational PWM/tach hwmon fan driver");
MODULE_LICENSE("GPL");
