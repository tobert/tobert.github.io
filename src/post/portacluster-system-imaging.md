---
id: "2014-07-14-portacluster-system-imaging"
title: "Portacluster: System Imaging"
abstract: "A simple PXE imaging setup for installing Arch Linux on my portable cluster"
tags: ["portacluster", "pxe", "dnsmasq", "nginx", "systemrescuecd", "linux", "sysadmin", "devops"]
pubdate: 2014-07-14T23:10:00Z
---

I reimaged [portacluster](/post/2014-02-07-portacluster.html) back to stock
Arch Linux yesterday and need to install and configure Cassandra and Spark
on them. I had recently surveyed many of the existing bare metal provisioning systems,
and decided to assemble a simple setup. The basics of how it works are outlined
below.

I run Arch Linux everywhere I can, so that's what I'm using on portacluster. If
one of the provisioning systems out there were easy enough to configure, I might
switch distros, but this turned out to be a lot less work. I'm still happy with
running Arch on these systems and hope this is helpful to others trying to
deploy distros that don't provide sophisticated bare metal installation tools.

The descriptions of files come after the code blocks below. node0 is the admin
node of portacluster and runs all the miscellaneous services including PXE and HTTP.

```
atobey@node0 ~ $ cat /etc/nginx/nginx.conf
worker_processes  1;

events {
    worker_connections  1024;
}

http {
    include            mime.types;
    default_type       application/octet-stream;
    sendfile           on;
    keepalive_timeout  0;

    server {
        listen      80 default_server;
        server_name node0.pc.datastax.com node0 localhost 192.168.10.10;

        location / {
            root   /srv/public;
            autoindex on;
        }
    }
}
```

The nginx configuration is about as simple as it gets. Pretty much all I had to
do to get started is change the web root to /srv/public and set
keepalive\_timeout to 0 (fixes some downloads hanging).

```
atobey@node0 ~ $ cat /etc/dnsmasq.conf
interface=eno1
enable-tftp
tftp-root=/srv/public/pxe
read-ethers
dhcp-range=192.168.10.130,192.168.10.180,1h
dhcp-boot=/pxelinux.0

dhcp-host=c4:04:15:90:bf:b4,switch.pc.datastax.com
dhcp-host=00:c0:b7:b6:6a:4c,pdu.pc.datastax.com
dhcp-host=c0:3f:d5:60:e5:a1,node1.pc.datastax.com
dhcp-host=c0:3f:d5:60:cd:8b,node2.pc.datastax.com
dhcp-host=c0:3f:d5:60:e5:b9,node3.pc.datastax.com
dhcp-host=c0:3f:d5:60:e4:ac,node4.pc.datastax.com
dhcp-host=c0:3f:d5:60:6f:18,node5.pc.datastax.com
dhcp-host=c0:3f:d5:60:d0:9e,node6.pc.datastax.com
```

The simplicity of configuration is the main reason I like using dnsmasq. It
provides DHCP, tftp, and DNS proxying in one daemon and I don't have to
remember the rules of various tools' configuration syntax. This configuration
boots pxelinux which will pull down a config (below) and boot the system
in short order. I use the interface= line to bind it to the portacluster
network interface so it's unlikely it will accidentally stomp on a foreign
network's DHCP services even when I have another USB interface attached.

```
atobey@node0 ~ $ cd /srv/public/pxe/pxelinux.cfg/
atobey@node0 /srv/public/pxe/pxelinux.cfg $ ls -l
total 40
lrwxrwxrwx 1 root root  10 Jul 13 19:13 01-c0-3f-d5-60-6f-18 -> arch-linux
lrwxrwxrwx 1 root root  10 Jul 13 19:13 01-c0-3f-d5-60-cd-8b -> arch-linux
lrwxrwxrwx 1 root root  10 Jul 13 19:13 01-c0-3f-d5-60-d0-9e -> arch-linux
lrwxrwxrwx 1 root root  10 Jul 13 19:13 01-c0-3f-d5-60-e4-ac -> arch-linux
lrwxrwxrwx 1 root root  10 Jul 13 19:13 01-c0-3f-d5-60-e5-a1 -> arch-linux
lrwxrwxrwx 1 root root  10 Jul 13 19:13 01-c0-3f-d5-60-e5-b9 -> arch-linux
-rw-r--r-- 1 root root 260 Jun 27 22:48 arch-linux
lrwxrwxrwx 1 root root   9 Jun 28 00:19 default -> installer
-rw-r--r-- 1 root root 237 Jun 28 00:10 installer
-rw-r--r-- 1 root root 234 Jun 28 00:19 rescue
```

This is a simple PXE bootstrapping setup that I've used a few times in the past. The only
infrastructure required is dnsmasq and nginx. When I want to reimage a system, I symlink
its MAC to the 'installer' and reboot.  The MAC and default file names follow the convention of
[pxelinux](http://www.syslinux.org/wiki/index.php/PXELINUX).

```
atobey@node0 /srv/public/pxe/pxelinux.cfg $ cat arch-linux
PROMPT 0
TIMEOUT 10
DEFAULT arch

LABEL arch
        LINUX /arch-kernel
        INITRD /arch-initramfs
        APPEND root=/dev/sda2 rw
```

The `arch-linux` config boots the system normally using a kernel and initramfs stored on
node0. This isn't entirely practical since it's easy to get out-of-sync kernel/modules.
I put this in place because I eventually want to get rid of the root partition entirely
and run the OS directly out of initramfs, but that's a topic for a different post.
Nodes 1-6 currently do not have boot sectors installed, since this works fine and I also
haven't settled whether I will continue to boot them in legacy BIOS mode or do the work
to get them PXE booting into native UEFI mode.

I typically run the following shell loop to enable the installer:

```
for mac in 01-c0-*
do
  ln -nfs installer $mac
done
```

After that I will reboot the systems with cluster ssh or power cycle them by pulling
the plug. Enabling normal boot is the same procedure but links to `arch-linux` instead
of `installer`.

```
atobey@node0 /srv/public/pxe/pxelinux.cfg $ cat installer
PROMPT 0
TIMEOUT 10
DEFAULT installer

LABEL installer
        LINUX /sysrcd-kernel
        INITRD /sysrcd-initram.igz
        APPEND docache dodhcp setkmap=us netboot=http://192.168.10.10/sysrcd/sysrcd.dat ar_source=http://192.168.10.10/installer ar_nowait
```

The installer config uses [System Rescue CD](http://www.sysresccd.org/Sysresccd-manual-en_PXE_network_booting)'s
autorun functionality to execute a
[system installer](https://github.com/tobert/portacluster-scripts/blob/master/overlay/admin/srv/public/installer/autorun)
written in bash. It's fast in my experience and easy to hack on. The golden image is
a simple tarball of an [Arch Chroot](https://wiki.archlinux.org/index.php/DeveloperWiki:Building_in_a_Clean_Chroot)
with the a few extra packages pre-installed. There is no additional configuration
in the tarball. Any extra config is pushed later using
[portacluster-scripts](https://github.com/tobert/portacluster-scripts).

I usually watch for systems to finish installing by tailing `/var/log/nginx/access.log`. The installer script
does a wget to a currently non-existent URL with the system's hostname and MAC. Eventually I may write
a small program to disable the installer, but for now it's good enough.

```
PROMPT 0
TIMEOUT 10
DEFAULT installer

LABEL installer
        LINUX /sysrcd-kernel
        INITRD /sysrcd-initram.igz
        APPEND docache dodhcp setkmap=us netboot=http://192.168.10.10/sysrcd/sysrcd.dat ar_source=http://192.168.10.10/rescue ar_nowait
```

The rescue config is the same as intaller but with ar\_source pointed at http://node0/rescue
instead of installer. The rescue autorun installs my atobey user and ssh key so I can log in
over ssh.

In previous setups like this I've generated custom initramfs images for the
installer, but System Rescue CD is working great and is now a component I don't
have to maintain. I copy the essential files out of the ISO into /srv/public/sysrcd
and symlink the kernel + initramfs into /srv/public/pxe so they can be served
over tftp by dnsmasq. I haven't found many machines that aren't supported by SysRCD
which has helped it to become one of my favorite power tools for bare metal systems work.

## Coming Soon

I originally started this post to describe how to install an OSS Cassandra/Spark cluster
but decided to publish this part separately. The Cassandra/Spark post will likely go out
tomorrow (2014-07-15).


