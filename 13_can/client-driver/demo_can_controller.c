// SPDX-License-Identifier: GPL-2.0-only
/* Educational driver for a fictional MMIO Classical CAN controller. */
#include <linux/bitfield.h>
#include <linux/can/dev.h>
#include <linux/can/error.h>
#include <linux/can/skb.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/string.h>

#define DEMO_CAN_CTRL             0x00
#define  DEMO_CAN_CTRL_ENABLE     BIT(0)
#define  DEMO_CAN_CTRL_LOOPBACK   BIT(1)
#define  DEMO_CAN_CTRL_LISTEN     BIT(2)
#define DEMO_CAN_BITTIMING        0x04
#define DEMO_CAN_INT_STATUS       0x08
#define DEMO_CAN_INT_ENABLE       0x0c
#define  DEMO_CAN_INT_RX          BIT(0)
#define  DEMO_CAN_INT_TX          BIT(1)
#define  DEMO_CAN_INT_BUS_OFF     BIT(2)
#define  DEMO_CAN_INT_ALL         (DEMO_CAN_INT_RX | DEMO_CAN_INT_TX | \
				    DEMO_CAN_INT_BUS_OFF)
#define DEMO_CAN_TX_ID            0x10
#define DEMO_CAN_TX_DLC           0x14
#define DEMO_CAN_TX_DATA0         0x18
#define DEMO_CAN_TX_COMMAND       0x20
#define DEMO_CAN_RX_ID            0x30
#define DEMO_CAN_RX_DLC           0x34
#define DEMO_CAN_RX_DATA0         0x38
#define DEMO_CAN_RX_RELEASE       0x40

struct demo_can_priv {
	struct can_priv can;
	void __iomem *base;
	struct clk *clk;
	struct net_device *ndev;
};

static const struct can_bittiming_const demo_can_bittiming_const = {
	.name = "demo_can",
	.tseg1_min = 2,
	.tseg1_max = 16,
	.tseg2_min = 1,
	.tseg2_max = 8,
	.sjw_max = 4,
	.brp_min = 1,
	.brp_max = 1024,
	.brp_inc = 1,
};

static void demo_can_write_data(void __iomem *base, unsigned int offset,
				const u8 *data, unsigned int len)
{
	unsigned int i;

	for (i = 0; i < len; i += 4) {
		u32 value = 0;
		unsigned int chunk = min(4U, len - i);

		memcpy(&value, data + i, chunk);
		writel(value, base + offset + i);
	}
}

static void demo_can_read_data(void __iomem *base, unsigned int offset,
			       u8 *data, unsigned int len)
{
	unsigned int i;

	for (i = 0; i < len; i += 4) {
		u32 value = readl(base + offset + i);
		unsigned int chunk = min(4U, len - i);

		memcpy(data + i, &value, chunk);
	}
}

static int demo_can_set_bittiming(struct net_device *ndev)
{
	struct demo_can_priv *priv = netdev_priv(ndev);
	const struct can_bittiming *bt = &priv->can.bittiming;
	u32 value;

	/* Fictional layout; real hardware often uses separate timing fields. */
	value = FIELD_PREP(GENMASK(9, 0), bt->brp - 1) |
		FIELD_PREP(GENMASK(15, 12), bt->prop_seg + bt->phase_seg1 - 1) |
		FIELD_PREP(GENMASK(19, 16), bt->phase_seg2 - 1) |
		FIELD_PREP(GENMASK(23, 20), bt->sjw - 1);
	writel(value, priv->base + DEMO_CAN_BITTIMING);
	return 0;
}

static int demo_can_set_mode(struct net_device *ndev, enum can_mode mode)
{
	struct demo_can_priv *priv = netdev_priv(ndev);
	u32 ctrl = DEMO_CAN_CTRL_ENABLE;

	if (mode != CAN_MODE_START)
		return -EOPNOTSUPP;

	if (priv->can.ctrlmode & CAN_CTRLMODE_LOOPBACK)
		ctrl |= DEMO_CAN_CTRL_LOOPBACK;
	if (priv->can.ctrlmode & CAN_CTRLMODE_LISTENONLY)
		ctrl |= DEMO_CAN_CTRL_LISTEN;
	writel(DEMO_CAN_INT_ALL, priv->base + DEMO_CAN_INT_ENABLE);
	writel(ctrl, priv->base + DEMO_CAN_CTRL);
	priv->can.state = CAN_STATE_ERROR_ACTIVE;
	netif_wake_queue(ndev);
	return 0;
}

static netdev_tx_t demo_can_start_xmit(struct sk_buff *skb,
				       struct net_device *ndev)
{
	struct demo_can_priv *priv = netdev_priv(ndev);
	struct can_frame *cf = (struct can_frame *)skb->data;

	if (can_dropped_invalid_skb(ndev, skb))
		return NETDEV_TX_OK;

	netif_stop_queue(ndev);
	can_put_echo_skb(skb, ndev, 0, 0);
	writel(cf->can_id, priv->base + DEMO_CAN_TX_ID);
	writel(cf->len, priv->base + DEMO_CAN_TX_DLC);
	demo_can_write_data(priv->base, DEMO_CAN_TX_DATA0, cf->data, cf->len);
	writel(1, priv->base + DEMO_CAN_TX_COMMAND);
	return NETDEV_TX_OK;
}

static void demo_can_receive(struct net_device *ndev)
{
	struct demo_can_priv *priv = netdev_priv(ndev);
	struct can_frame *cf;
	struct sk_buff *skb;
	u32 dlc;

	skb = alloc_can_skb(ndev, &cf);
	if (!skb) {
		ndev->stats.rx_dropped++;
		writel(1, priv->base + DEMO_CAN_RX_RELEASE);
		return;
	}

	cf->can_id = readl(priv->base + DEMO_CAN_RX_ID);
	dlc = readl(priv->base + DEMO_CAN_RX_DLC) & 0xf;
	cf->len = can_cc_dlc2len(dlc);
	demo_can_read_data(priv->base, DEMO_CAN_RX_DATA0, cf->data, cf->len);
	writel(1, priv->base + DEMO_CAN_RX_RELEASE);

	ndev->stats.rx_packets++;
	ndev->stats.rx_bytes += cf->len;
	netif_rx(skb);
}

static irqreturn_t demo_can_irq(int irq, void *data)
{
	struct net_device *ndev = data;
	struct demo_can_priv *priv = netdev_priv(ndev);
	u32 status = readl(priv->base + DEMO_CAN_INT_STATUS);

	(void)irq;
	if (!(status & DEMO_CAN_INT_ALL))
		return IRQ_NONE;
	writel(status, priv->base + DEMO_CAN_INT_STATUS);

	if (status & DEMO_CAN_INT_RX)
		demo_can_receive(ndev);
	if (status & DEMO_CAN_INT_TX) {
		unsigned int bytes = can_get_echo_skb(ndev, 0, NULL);

		ndev->stats.tx_packets++;
		ndev->stats.tx_bytes += bytes;
		netif_wake_queue(ndev);
	}
	if (status & DEMO_CAN_INT_BUS_OFF) {
		priv->can.state = CAN_STATE_BUS_OFF;
		can_bus_off(ndev);
	}

	return IRQ_HANDLED;
}

static int demo_can_open(struct net_device *ndev)
{
	struct demo_can_priv *priv = netdev_priv(ndev);
	u32 ctrl = DEMO_CAN_CTRL_ENABLE;
	int ret;

	ret = open_candev(ndev);
	if (ret)
		return ret;
	ret = demo_can_set_bittiming(ndev);
	if (ret) {
		close_candev(ndev);
		return ret;
	}
	if (priv->can.ctrlmode & CAN_CTRLMODE_LOOPBACK)
		ctrl |= DEMO_CAN_CTRL_LOOPBACK;
	if (priv->can.ctrlmode & CAN_CTRLMODE_LISTENONLY)
		ctrl |= DEMO_CAN_CTRL_LISTEN;
	writel(DEMO_CAN_INT_ALL, priv->base + DEMO_CAN_INT_ENABLE);
	writel(ctrl, priv->base + DEMO_CAN_CTRL);
	priv->can.state = CAN_STATE_ERROR_ACTIVE;
	netif_start_queue(ndev);
	return 0;
}

static int demo_can_stop(struct net_device *ndev)
{
	struct demo_can_priv *priv = netdev_priv(ndev);

	netif_stop_queue(ndev);
	writel(0, priv->base + DEMO_CAN_INT_ENABLE);
	writel(0, priv->base + DEMO_CAN_CTRL);
	priv->can.state = CAN_STATE_STOPPED;
	close_candev(ndev);
	return 0;
}

static const struct net_device_ops demo_can_netdev_ops = {
	.ndo_open = demo_can_open,
	.ndo_stop = demo_can_stop,
	.ndo_start_xmit = demo_can_start_xmit,
	.ndo_change_mtu = can_change_mtu,
};

static int demo_can_probe(struct platform_device *pdev)
{
	struct demo_can_priv *priv;
	struct net_device *ndev;
	int irq;
	int ret;

	ndev = alloc_candev(sizeof(*priv), 1);
	if (!ndev)
		return -ENOMEM;
	priv = netdev_priv(ndev);
	priv->ndev = ndev;
	priv->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(priv->base)) {
		ret = PTR_ERR(priv->base);
		goto free_netdev;
	}
	priv->clk = devm_clk_get_enabled(&pdev->dev, NULL);
	if (IS_ERR(priv->clk)) {
		ret = dev_err_probe(&pdev->dev, PTR_ERR(priv->clk), "clock\n");
		goto free_netdev;
	}
	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		ret = irq;
		goto free_netdev;
	}

	priv->can.clock.freq = clk_get_rate(priv->clk);
	priv->can.bittiming_const = &demo_can_bittiming_const;
	priv->can.do_set_bittiming = demo_can_set_bittiming;
	priv->can.do_set_mode = demo_can_set_mode;
	priv->can.ctrlmode_supported = CAN_CTRLMODE_LOOPBACK |
					 CAN_CTRLMODE_LISTENONLY |
					 CAN_CTRLMODE_BERR_REPORTING;
	priv->can.state = CAN_STATE_STOPPED;
	ndev->netdev_ops = &demo_can_netdev_ops;
	ndev->flags |= IFF_ECHO;
	SET_NETDEV_DEV(ndev, &pdev->dev);
	platform_set_drvdata(pdev, ndev);

	ret = devm_request_irq(&pdev->dev, irq, demo_can_irq, 0,
			       dev_name(&pdev->dev), ndev);
	if (ret)
		goto free_netdev;
	ret = register_candev(ndev);
	if (ret)
		goto free_netdev;

	dev_info(&pdev->dev, "fictional educational CAN controller registered\n");
	return 0;

free_netdev:
	free_candev(ndev);
	return ret;
}

static void demo_can_remove(struct platform_device *pdev)
{
	struct net_device *ndev = platform_get_drvdata(pdev);

	unregister_candev(ndev);
	free_candev(ndev);
}

static const struct of_device_id demo_can_of_match[] = {
	{ .compatible = "demo,mmio-can" },
	{ }
};
MODULE_DEVICE_TABLE(of, demo_can_of_match);

static struct platform_driver demo_can_driver = {
	.probe = demo_can_probe,
	.remove = demo_can_remove,
	.driver = {
		.name = "demo-mmio-can",
		.of_match_table = demo_can_of_match,
	},
};
module_platform_driver(demo_can_driver);

MODULE_AUTHOR("OpenBMC peripheral study project");
MODULE_DESCRIPTION("Educational fictional MMIO CAN controller");
MODULE_LICENSE("GPL");
