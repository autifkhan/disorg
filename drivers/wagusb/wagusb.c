#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/usb.h>
#include <linux/mutex.h>
#include <linux/slab.h>

#define WAGUSB_MIN(x, y) (((x) < (y)) ? (x) : (y))

MODULE_LICENSE("Dual BSD/GPL");

#define EPBUFF_SIZE 512

#define WAGUSB_PID_LXI 0x0450
#define WAGUSB_PID_SENSOR 0x1040
#define WAGUSB_PID_MTYMP 0x1035
#define WAGUSB_PID_PVSM 0x1100
#define WAGUSB_PID_CP20 0x1110

static struct usb_device_id id_table[] =
{
  {USB_DEVICE(0x0770, WAGUSB_PID_LXI)},
  {USB_DEVICE(0x0770, WAGUSB_PID_SENSOR)},
  {USB_DEVICE(0x0770, WAGUSB_PID_MTYMP)},
  {USB_DEVICE(0x0770, WAGUSB_PID_PVSM)},
  {USB_DEVICE(0x0770, WAGUSB_PID_CP20)},
  {},
};

struct usb_wagusb
{
  struct usb_device *udev;
  __u8 bulk_in_endpointAddr;
  __u8 bulk_out_endpointAddr;
  unsigned char *bulk_in_buffer;
  unsigned char *bulk_out_buffer;
  int read_pos;
  int bytes_read;
  struct mutex lock;
};

ssize_t wagusb_read (struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
  int result = 0, cache_count;/*, bs;*/
  struct usb_wagusb *dev = (struct usb_wagusb*) filp->private_data;

  if (count > EPBUFF_SIZE) return -EFAULT;
  if (NULL == dev) return -ENODEV;

  mutex_lock(&dev->lock);
  cache_count = dev->bytes_read - dev->read_pos;
  if (cache_count > 0 && count <= cache_count) //we have the bytes the user needs
  {
    if (copy_to_user(buf, &dev->bulk_in_buffer[dev->read_pos], count))
      result = -EFAULT;
    else
    {
      dev->read_pos += count;
      result = count;
    }
  }
  else
  {
    if (cache_count > 0) //we have fewer bytes than user needs
    {
      if (copy_to_user(buf, &dev->bulk_in_buffer[dev->read_pos], cache_count))
      {
        result = -EFAULT;
      }
      else
      {
        dev->read_pos += cache_count;
        result = cache_count;
      }
    }
    else //we have no bytes; request from device
    {
    //*
      result = usb_bulk_msg(dev->udev, usb_rcvbulkpipe(dev->udev, dev->bulk_in_endpointAddr),
                            dev->bulk_in_buffer, EPBUFF_SIZE, &dev->bytes_read, 10);
      if (result)
      {
        dev->read_pos = 0;
        if (result != -110)
          printk(KERN_ALERT "wagusb: wagusb_read() usb_blk_msg() result = %d\n", result);
      }
      else
      {
        if (copy_to_user(buf, dev->bulk_in_buffer, WAGUSB_MIN(count, dev->bytes_read)))
	  result = -EFAULT;
	else
	{
	  dev->read_pos = WAGUSB_MIN(count, dev->bytes_read);
	  result = WAGUSB_MIN(count, dev->bytes_read);
	}
      }
    //*/
      //result = -EIO;
    }
  }
  /*
  if (dev->read_pos == dev->bytes_read)
  {
    bs = usb_bulk_msg(dev->udev, usb_rcvbulkpipe(dev->udev, dev->bulk_in_endpointAddr),
                          dev->bulk_in_buffer, EPBUFF_SIZE, &dev->bytes_read, 10);
    if (bs)
    {
      if (bs != -110)
        printk(KERN_ALERT "wagusb: wagusb_read() usb_blk_msg() result = %d\n", bs);
    }
    dev->read_pos = 0;
  }
  //*/
  mutex_unlock(&dev->lock);
  return result;
}

ssize_t wagusb_write (struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
  int result = 0;
  struct usb_wagusb *dev = (struct usb_wagusb*) filp->private_data;

  if (count > EPBUFF_SIZE) return -EFAULT;
  if (NULL == dev) return -ENODEV;

  mutex_lock(&dev->lock);
  if (copy_from_user(dev->bulk_out_buffer, buf, count))
  {
    mutex_unlock(&dev->lock);
    return -EFAULT;
  }
  result = usb_bulk_msg(dev->udev, usb_sndbulkpipe(dev->udev, dev->bulk_out_endpointAddr),
                        dev->bulk_out_buffer, count, &count, HZ*10);
  if (result)
    printk(KERN_ALERT "wagusb: wagusb_write() ERROR: cant write, result = %d\n", result);
  else
    result = count;
  mutex_unlock(&dev->lock);
  return result;
}

static struct usb_driver wagusb_driver;
int wagusb_open(struct inode *inode, struct file *filep)
{
  struct usb_wagusb *dev;
  struct usb_interface *interface;
  int subminor;

  subminor = iminor(inode);
  interface = usb_find_interface(&wagusb_driver, subminor);
  if (!interface)
  {
    printk(KERN_ALERT "wagusb: wagusb_open() ERROR: cant find interface\n");
    return -ENODEV;
  }

  dev = usb_get_intfdata(interface);
  if (!dev)
  {
    printk(KERN_ALERT "wagusb: wagusb_open() ERROR: cant find usb_wagusb\n");
    return -ENODEV;
  }

  mutex_lock(&dev->lock);
  dev->read_pos = 0;
  dev->bytes_read = 0;
  filep->private_data = dev;
  mutex_unlock(&dev->lock);
  return 0;
}

static const struct file_operations wagusb_fops =
{
  .owner = THIS_MODULE,
  .read = wagusb_read,
  .write = wagusb_write,
  .open = wagusb_open,
};

static struct usb_class_driver wagusb_class =
{
  .name = "wagusb%3.3d",
  .fops = &wagusb_fops,
};

static int wagusb_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
  struct usb_device *udev = interface_to_usbdev(interface);
  struct usb_wagusb *dev = NULL;
  struct usb_host_interface *iface_desc;
  struct usb_endpoint_descriptor *endpoint;
  int result = -ENOMEM, i;

  dev = kzalloc(sizeof(struct usb_wagusb), GFP_KERNEL);
  if (NULL == dev)
  {
    printk (KERN_ALERT "wagusb: wagusb_probe() - Out of memory\n"); 
    goto error_mem;
  }

  mutex_init(&dev->lock);
  dev->udev = usb_get_dev(udev);

  /* set up endpoint info */
  iface_desc = interface->cur_altsetting;
  for (i = 0; i < iface_desc->desc.bNumEndpoints; i++)
  {
    endpoint = &iface_desc->endpoint[i].desc;
    if (usb_endpoint_is_bulk_in(endpoint))
    {
      dev->bulk_in_endpointAddr = endpoint->bEndpointAddress;
      dev->bulk_in_buffer = kzalloc(EPBUFF_SIZE, GFP_KERNEL);
      if (NULL == dev->bulk_in_buffer)
      {
        printk (KERN_ALERT "wagusb: wagusb_probe() - Out of memory\n"); 
        goto error_mem;
      }
    }
    if (usb_endpoint_is_bulk_out(endpoint))
    {
      dev->bulk_out_endpointAddr = endpoint->bEndpointAddress;
      dev->bulk_out_buffer = kzalloc(EPBUFF_SIZE, GFP_KERNEL);
      if (NULL == dev->bulk_out_buffer)
      {
        printk (KERN_ALERT "wagusb: wagusb_probe() - Out of memory\n"); 
        goto error_mem;
      }
    }
  }

  if (!dev->bulk_in_endpointAddr && !dev->bulk_out_endpointAddr)
  {
    printk (KERN_ALERT "wagusb: wagusb_probe() ERROR: BULK IN and OUT not found\n"); 
    result = -1;
    goto error_noendpoints;
  }

  dev->read_pos = 0;
  dev->bytes_read = 0;
  usb_set_intfdata(interface, dev);

  result = usb_register_dev(interface, &wagusb_class);
  if (result)
  {
    printk (KERN_ALERT "wagusb: wagusb_probe() ERROR: Cant register device\n"); 
    usb_set_intfdata(interface, NULL);
    goto error_cant_register_dev;
  }

  return 0;
error_cant_register_dev:
error_noendpoints:
error_mem:
  if (dev && dev->bulk_in_buffer) kfree(dev->bulk_in_buffer);
  if (dev && dev->bulk_out_buffer) kfree(dev->bulk_out_buffer);
  if (dev) kfree(dev);
  return result;
}

static void wagusb_disconnect(struct usb_interface *interface)
{
  struct usb_wagusb *dev = NULL;

  dev = usb_get_intfdata(interface);
  if (dev)
  {
    mutex_lock(&dev->lock);
    usb_deregister_dev(interface, &wagusb_class);
    usb_set_intfdata(interface, NULL);
    if (dev->bulk_in_buffer) kfree(dev->bulk_in_buffer);
    if (dev->bulk_out_buffer) kfree(dev->bulk_out_buffer);
    usb_put_dev(dev->udev);
    mutex_unlock(&dev->lock);
    kfree(dev);
  }
  else
    printk(KERN_ALERT "wagusb: wagusb_disconnect() Very bad things happened\n");
}

struct cdev *p_wagusb_cdev;

static struct usb_driver wagusb_driver = 
{
  .name = "wagusb",
  .probe = wagusb_probe,
  .disconnect = wagusb_disconnect,
  .id_table = id_table,
};

static int wagusb_init(void)
{
  int result = 0;
  result = usb_register(&wagusb_driver);
  if (result < 0)
    printk (KERN_ALERT "wagusb: usb_register() failed, error = %d\n", result);
  return result;
}

static void wagusb_exit(void)
{
  usb_deregister(&wagusb_driver);
}

module_init(wagusb_init);
module_exit(wagusb_exit);

