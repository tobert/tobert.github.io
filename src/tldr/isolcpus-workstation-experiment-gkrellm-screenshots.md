Try booting one of your Linux machines with isolcpus=0 or isolcpus=0,1 if you have
a hyperthreading (Intel) CPU. On Debian variants you'll want to put it in
`/etc/defaults/grub`. I'm running Arch so I have the following in my
`/boot/syslinux/syslinux.cfg`

```
LABEL arch
    MENU LABEL Arch Linux
    LINUX ../vmlinuz-linux
    APPEND root=LABEL=NEWROOT rw isolcpus=0,1
    INITRD ../initramfs-linux.img
```

I've been running with this enabled on my daily driver workstation and I feel like
system responsiveness is considerably better. This is all seat-of-your-pants performance
measurement at the moment. I suspect this is something that will be useful in many
more situations than it's used currently.

Here's what my system load looks like in gkrellm at the moment:

![gkrellm](/images/gkrellm-2015-01-02.png)

This is cool because most of the kernel work is happening on the first core. The
isolcpus=0,1 parameter has instructed the kernel to not schedule *userland* processes
on it. It can still schedule kernel threads on it, and lookie there, it's always idle
so there's rarely any wait for CPU time and the caches aren't constantly bombarded.

And here it is with Team Fortress 2 running, because science! You can also the bouncing
thread problem in the CPU graphs and my ZFS disks heating up while TF2 loads up its resources.

![tf2](/images/tf2-gkrellm-2015-01-02.png)

More to come!
