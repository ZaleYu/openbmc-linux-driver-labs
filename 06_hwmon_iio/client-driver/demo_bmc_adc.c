// SPDX-License-Identifier: GPL-2.0
/* Educational IIO provider for a fictional MMIO BMC ADC. */
#include <linux/bitfield.h>
#include <linux/bits.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/iio/iio.h>

#define DEMO_ADC_VREF_MV	1800
#define DEMO_ADC_BITS		12
#define DEMO_ADC_MASK		GENMASK(DEMO_ADC_BITS - 1, 0)
#define DEMO_REG_VCORE		0x00
#define DEMO_REG_VDDIO		0x04
#define DEMO_REG_TEMP		0x08
#define DEMO_REG_AUX		0x0c

struct demo_adc {
	void __iomem *base;
};

#define DEMO_VOLTAGE_CHAN(_index, _reg) {			\
	.type = IIO_VOLTAGE, .indexed = 1, .channel = (_index),	\
	.address = (_reg),					\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),		\
	.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE),	\
}

static const struct iio_chan_spec demo_adc_channels[] = {
	DEMO_VOLTAGE_CHAN(0, DEMO_REG_VCORE),
	DEMO_VOLTAGE_CHAN(1, DEMO_REG_VDDIO),
	{
		.type = IIO_TEMP,
		.indexed = 1,
		.channel = 0,
		.address = DEMO_REG_TEMP,
		.info_mask_separate = BIT(IIO_CHAN_INFO_PROCESSED),
	},
	DEMO_VOLTAGE_CHAN(2, DEMO_REG_AUX),
};

static int demo_adc_read_raw(struct iio_dev *indio_dev,
			     const struct iio_chan_spec *chan,
			     int *val, int *val2, long mask)
{
	struct demo_adc *adc = iio_priv(indio_dev);
	u32 reg;

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		if (chan->type != IIO_VOLTAGE)
			return -EINVAL;
		reg = readl(adc->base + chan->address);
		*val = FIELD_GET(DEMO_ADC_MASK, reg);
		return IIO_VAL_INT;
	case IIO_CHAN_INFO_SCALE:
		if (chan->type != IIO_VOLTAGE)
			return -EINVAL;
		*val = DEMO_ADC_VREF_MV;
		*val2 = DEMO_ADC_BITS;
		return IIO_VAL_FRACTIONAL_LOG2;
	case IIO_CHAN_INFO_PROCESSED:
		if (chan->type != IIO_TEMP)
			return -EINVAL;
		*val = (s16)readl(adc->base + chan->address) * 10;
		return IIO_VAL_INT;
	default:
		return -EINVAL;
	}
}

static const struct iio_info demo_adc_info = {
	.read_raw = demo_adc_read_raw,
};

static int demo_adc_probe(struct platform_device *pdev)
{
	struct iio_dev *indio_dev;
	struct demo_adc *adc;
	int ret;

	indio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(*adc));
	if (!indio_dev)
		return -ENOMEM;
	adc = iio_priv(indio_dev);
	adc->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(adc->base))
		return PTR_ERR(adc->base);

	indio_dev->name = "demo_bmc_adc";
	indio_dev->info = &demo_adc_info;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = demo_adc_channels;
	indio_dev->num_channels = ARRAY_SIZE(demo_adc_channels);

	ret = devm_iio_device_register(&pdev->dev, indio_dev);
	if (ret)
		return dev_err_probe(&pdev->dev, ret,
				     "failed to register IIO device\\n");
	platform_set_drvdata(pdev, indio_dev);
	return 0;
}

static const struct of_device_id demo_adc_of_match[] = {
	{ .compatible = "openai,demo-bmc-adc" },
	{ }
};
MODULE_DEVICE_TABLE(of, demo_adc_of_match);

static struct platform_driver demo_adc_driver = {
	.probe = demo_adc_probe,
	.driver = {
		.name = "demo_bmc_adc",
		.of_match_table = demo_adc_of_match,
	},
};
module_platform_driver(demo_adc_driver);

MODULE_AUTHOR("OpenBMC Peripheral Driver Study");
MODULE_DESCRIPTION("Educational BMC MMIO ADC IIO provider");
MODULE_LICENSE("GPL");

