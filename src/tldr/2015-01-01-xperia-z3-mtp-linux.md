There are lots of posts on the web about how to make various phones
work by setting up udev and either mtpfs or go-mtpfs but none of those
worked on my [Sony Xperia Z3](http://www.sonymobile.com/us/products/phones/xperia-z3-t-mobile/)
and Arch Linux workstation. It turns out
that [go-mtpfs](https://github.com/hanwen/go-mtpfs) (and, presumably,
mtpfs) use 64-bit extensions to the MTP protocol by default that are
not supported by the Z3.

```sh
mkdir ~/phone
$GOPATH/bin/go-mtpfs -android=false ~/phone
```

If you don't want to mess with the udev rules, you can manually set
the permissions on the USB device while messing around.

```sh
# danger! This works on my machines. Might be bad for yours. But probably not.
sudo chown $USER /dev/bus/usb/*/*
```

I also noticed that the udev rules on most sites are changing the USB device
file to mode 0666. Awful idea even on a personal machine. Just set the group
to a group your user in (type `groups` on the shell to see).

```
ACTION=="add" SYBSYSTEM=="usb", SYSFS{idVendor}=="0fce", SYSFS{idProduct}=="01bf", GROUP="storage", MODE="0660"
```
