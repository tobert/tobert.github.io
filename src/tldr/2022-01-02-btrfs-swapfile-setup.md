A sidequest of setting up some of my electronic music lab at home is setting up
the laptop I'm typing this on to run Arch Linux to be dedicated to music production.

A sidequest of the sidequest is that suspend wasn't working right. When I partitioned
the drive I thought, well, I have 32GB of RAM and an NVMe drive, I probably shouldn't
be swapping, ever. So I didn't make a swap partition. I also don't feel like repartitioning
right now instead of hooking up more MIDI devices, so let's try a swapfile. I found
the existing docs kinda incomplete so here's my take on the process, showing each command
without further adieu:

Here's my `/etc/fstab`. Of special note are the mount flags for the btrfs `root/swap` subvolume. The first two mounts
are boot and operating system disks. The second two are for the swapfile.

```
# gadget's fstab, by Amy Tobey
#
# EFI: /dev/nvme0n1p1, plaintext
#
# this system is configured to boot via UEFI, which will directly load the
# linux kernel from this partition and start it
UUID=AAAA-AAAA /boot vfat rw,relatime,fmask=0022,dmask=0022,codepage=437,iocharset=ascii,shortname=mixed,utf8,errors=remount-ro	0 2

# root: /dev/nvme0n1p2, but encrypted
#
# luks full-system encryption, one big btrfs filesystem
# created with plain old cryptsetup, using encrypt hook in initramfs
/dev/mappper/root / btrfs rw,relatime,ssd,space_cache=v2,subvolid=5,subvol=/ 0 0

# swap
#
# btrfs subvolume for the swapfile, which is /swap/file below
# the nodatacow option is mandatory for swapfile usage
/dev/mapper/root /swap btrfs rw,nodatacow,nodatasum,subvol=root/swap 0 1

# a plain swapfile, on the btrfs subvolume ^^
/swap/file none swap defaults 0 0
```

Create the subvolume, create the swapfile, turn it all on:

```sh
btrfs subvolume create /root/swap # create the subvolume
mkdir /swap                       # create the mountpoint directory
mount /swap                       # mount the subvolume on /swap, reads settings from /etc/fstab
chmod 0700 /swap                  # tighten up permissions to -rwx------
touch /swap/file                  # create an empty file
chmod 0600 /swap/file             # tighten up permissions to -rw-------
chattr +C /swap/file              # mark the file as ineligible for copy-on-write
dd if=/dev/zero of=/swap/file bs=4096 count=16777216 # write out 64GB of zeroed pages
mkswap /swap/file                 # write swap signature to the file
swapon -a                         # turn it on!
free                              # display available memory and swap
```

Astute readers will notice I did `chattr` and the `nodatacow` mount option. I think the `chattr`
probably isn't necessary in this configuration. I tried it without the mount option and had no
luck, and left it here for completeness since it can't really hurt.

