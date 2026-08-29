// SPDX-License-Identifier: GPL-2.0-only
/* Educational host-side bulk loopback driver. Lab VID/PID only. */
#include <linux/fs.h>
#include <linux/kref.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/usb.h>

#define DEMO_USB_VENDOR_ID       0xffff
#define DEMO_USB_PRODUCT_ID      0xffff
#define DEMO_USB_MINOR_BASE      192
#define DEMO_USB_MAX_XFER        4096
#define DEMO_USB_TIMEOUT_MS      2000

struct demo_usb {
	struct usb_device *udev;
	struct usb_interface *interface;
	struct kref kref;
	struct mutex io_lock;
	u8 bulk_in_ep;
	u8 bulk_out_ep;
	bool disconnected;
};

static struct usb_driver demo_usb_driver;

static void demo_usb_delete(struct kref *kref)
{
	struct demo_usb *dev = container_of(kref, struct demo_usb, kref);

	usb_put_dev(dev->udev);
	kfree(dev);
}

static int demo_usb_open(struct inode *inode, struct file *file)
{
	struct usb_interface *interface;
	struct demo_usb *dev;

	interface = usb_find_interface(&demo_usb_driver, iminor(inode));
	if (!interface)
		return -ENODEV;

	dev = usb_get_intfdata(interface);
	if (!dev)
		return -ENODEV;

	kref_get(&dev->kref);
	file->private_data = dev;
	return 0;
}

static int demo_usb_release(struct inode *inode, struct file *file)
{
	struct demo_usb *dev = file->private_data;

	(void)inode;
	if (dev)
		kref_put(&dev->kref, demo_usb_delete);
	return 0;
}

static ssize_t demo_usb_read(struct file *file, char __user *user_buffer,
			     size_t count, loff_t *ppos)
{
	struct demo_usb *dev = file->private_data;
	unsigned char *buffer;
	int actual = 0;
	int ret;

	(void)ppos;
	if (!count)
		return 0;
	count = min_t(size_t, count, DEMO_USB_MAX_XFER);
	buffer = kmalloc(count, GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	mutex_lock(&dev->io_lock);
	if (dev->disconnected) {
		ret = -ENODEV;
		goto out_unlock;
	}

	ret = usb_bulk_msg(dev->udev,
			   usb_rcvbulkpipe(dev->udev, dev->bulk_in_ep),
			   buffer, count, &actual, DEMO_USB_TIMEOUT_MS);
	if (!ret && copy_to_user(user_buffer, buffer, actual))
		ret = -EFAULT;
	if (!ret)
		ret = actual;

out_unlock:
	mutex_unlock(&dev->io_lock);
	kfree(buffer);
	return ret;
}

static ssize_t demo_usb_write(struct file *file, const char __user *user_buffer,
			      size_t count, loff_t *ppos)
{
	struct demo_usb *dev = file->private_data;
	unsigned char *buffer;
	int actual = 0;
	int ret;

	(void)ppos;
	if (!count)
		return 0;
	count = min_t(size_t, count, DEMO_USB_MAX_XFER);
	buffer = memdup_user(user_buffer, count);
	if (IS_ERR(buffer))
		return PTR_ERR(buffer);

	mutex_lock(&dev->io_lock);
	if (dev->disconnected) {
		ret = -ENODEV;
		goto out_unlock;
	}

	ret = usb_bulk_msg(dev->udev,
			   usb_sndbulkpipe(dev->udev, dev->bulk_out_ep),
			   buffer, count, &actual, DEMO_USB_TIMEOUT_MS);
	if (!ret)
		ret = actual;

out_unlock:
	mutex_unlock(&dev->io_lock);
	kfree(buffer);
	return ret;
}

static const struct file_operations demo_usb_fops = {
	.owner = THIS_MODULE,
	.open = demo_usb_open,
	.release = demo_usb_release,
	.read = demo_usb_read,
	.write = demo_usb_write,
	.llseek = no_llseek,
};

static struct usb_class_driver demo_usb_class = {
	.name = "demo_usb%d",
	.fops = &demo_usb_fops,
	.minor_base = DEMO_USB_MINOR_BASE,
};

static int demo_usb_probe(struct usb_interface *interface,
			  const struct usb_device_id *id)
{
	struct usb_host_interface *alts = interface->cur_altsetting;
	struct demo_usb *dev;
	int i;
	int ret;

	(void)id;
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	dev->udev = usb_get_dev(interface_to_usbdev(interface));
	dev->interface = interface;
	kref_init(&dev->kref);
	mutex_init(&dev->io_lock);

	for (i = 0; i < alts->desc.bNumEndpoints; i++) {
		const struct usb_endpoint_descriptor *ep = &alts->endpoint[i].desc;

		if (!dev->bulk_in_ep && usb_endpoint_is_bulk_in(ep))
			dev->bulk_in_ep = ep->bEndpointAddress;
		if (!dev->bulk_out_ep && usb_endpoint_is_bulk_out(ep))
			dev->bulk_out_ep = ep->bEndpointAddress;
	}

	if (!dev->bulk_in_ep || !dev->bulk_out_ep) {
		ret = -ENODEV;
		goto error;
	}

	usb_set_intfdata(interface, dev);
	ret = usb_register_dev(interface, &demo_usb_class);
	if (ret) {
		usb_set_intfdata(interface, NULL);
		goto error;
	}

	dev_info(&interface->dev, "bulk loopback attached as /dev/demo_usb%d\n",
		 interface->minor - DEMO_USB_MINOR_BASE);
	return 0;

error:
	kref_put(&dev->kref, demo_usb_delete);
	return ret;
}

static void demo_usb_disconnect(struct usb_interface *interface)
{
	struct demo_usb *dev = usb_get_intfdata(interface);

	usb_set_intfdata(interface, NULL);
	usb_deregister_dev(interface, &demo_usb_class);

	mutex_lock(&dev->io_lock);
	dev->disconnected = true;
	mutex_unlock(&dev->io_lock);
	kref_put(&dev->kref, demo_usb_delete);
	dev_info(&interface->dev, "bulk loopback disconnected\n");
}

static const struct usb_device_id demo_usb_ids[] = {
	{ USB_DEVICE(DEMO_USB_VENDOR_ID, DEMO_USB_PRODUCT_ID) },
	{ }
};
MODULE_DEVICE_TABLE(usb, demo_usb_ids);

static struct usb_driver demo_usb_driver = {
	.name = "demo_usb_bulk",
	.probe = demo_usb_probe,
	.disconnect = demo_usb_disconnect,
	.id_table = demo_usb_ids,
};
module_usb_driver(demo_usb_driver);

MODULE_AUTHOR("OpenBMC peripheral study project");
MODULE_DESCRIPTION("Educational USB bulk loopback host driver");
MODULE_LICENSE("GPL");
