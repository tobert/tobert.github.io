Many applications seem to be having trouble with a Linux feature called
"Transparent Hugepages". When enabled, the Linux kernel will try to allocate
memory in bigger chunks - typically 2MB - rather than 4K at a time. This can
impact performance by reducing the load on CPU caches (TLB) used for managing
ranges of addresses. As you can imagine, managing memory on a 128GB machine in
4KB pages requires a fair bit of work. Using 2MB pages reduces the number of
pages being tracked by a factor of 512!

So, with that said, it sounds like huge pages are a Good Thing, but they can
cause surprises when enabled in the background. Many applications assume the
page size on Linux is 4096 and allocate memory based on that assumption. That's
not such a bad thing since the OS still reports a page size of 4096. Where it
gets ugly is when Linux decides to defrag 4K pages into 2MB pages. In order
to do so, it has to lock the memory being moved which almost always ends up
being visible in the applications whose pages are being migrated. On most
Redhat variants including RHEL and CentOS, THP defrag is enabled by default
and you probably want to disable it via a setting in sysfs.

```
# require apps to opt into defrag
echo madvise |sudo tee /sys/kernel/mm/transparent_hugepage/defrag

# disable defrag entirely
echo never |sudo tee /sys/kernel/mm/transparent_hugepage/defrag
```

I'm going to continue experimenting with defrag enabled in my
lab. There are a few knobs in syfs that resemble the ksm (kernel samepage merging -
a similar feature for deduplicating memory) settings that can be used to
tune khugepaged.

```
[atobey@moltar ~]$ ls /sys/kernel/mm/hugepages/hugepages-2048kB/
free_hugepages  nr_hugepages  nr_hugepages_mempolicy  nr_overcommit_hugepages  resv_hugepages  surplus_hugepages
```

## -XX:+AlwaysPreTouch

I ran some quick tests and it appears that THP and Cassandra get along well
once defrag is out of the picture. The big problem with Cassandra and THP
is that while the JVM will happily malloc all of your heap at startup when
-Xmx == -Xms, the operating system is pessimistic about it and does not
actually map real memory pages until those pages are accessed. Fortunately,
there's an easy way to get the JVM to access all of its memory immediately
at startup. It's easy to see this works with THP. I suspect it also helps
in virtualized environments to make sure the heap is allocated in as few
fragments as possible.

```
[atobey@moltar ~]$ grep Huge /proc/meminfo
AnonHugePages:    641024 kB
HugePages_Total:       0
HugePages_Free:        0
HugePages_Rsvd:        0
HugePages_Surp:        0
Hugepagesize:       2048 kB
[atobey@moltar ~]$ sudo docker start cass01
cass01
[atobey@moltar ~]$ grep AnonHuge /proc/meminfo
AnonHugePages:   1042432 kB
[atobey@moltar ~]$ grep AnonHuge /proc/meminfo
AnonHugePages:   1130496 kB
[atobey@moltar ~]$ grep AnonHuge /proc/meminfo
AnonHugePages:   1185792 kB
```

As you can see, the kernel is allocating huge pages to Cassandra when I start
it up in the docker start command. It isn't allocating all 4GB of heap I configured
because that memory has not been used by the JVM. As you can imagine, running in this
way may cause some fragmentation over time.

Now here's a similar exercise with -XX:+AlwaysPreTouch added to the JVM parameters.

```
[atobey@moltar ~]$ grep AnonHuge /proc/meminfo
AnonHugePages:    665600 kB
[atobey@moltar ~]$ sudo docker start cass01
cass01
[atobey@moltar ~]$ grep AnonHuge /proc/meminfo
AnonHugePages:   4868096 kB
[atobey@moltar ~]$ ps -eo args |grep java
/usr/bin/java ... -Xms3941M -Xmx3941M -Xmn800M -XX:+AlwaysPreTouch ...
```

In this case, all of the pages are getting zeroed immediately after the JVM starts,
forcing the kernel to map them all immediately.  My suspicion is that most JVM
applications with statically-sized heaps (-Xmx == -Xms) should be enabling
-XX:+AlwaysPreTouch rather than letting the OS map memory pages on use.

## -XX+UseLargePages -XX:LargePageSizeInBytes=2m

There is a way to tell the JVM to explicitly use huge pages at startup
using the -XX:+UseLargePages option. This requires more changes to get
working but may be preferable for your environment.

First, the kernel has to be instructed to reserve memory for huge pages. Then
a group ID has to be provided so the kernel can allow non-root processes
to allocate huge pages.

My Docker image runs cassandra as uid/gid 1337 so I've set hugetlb\_shm\_group to
1337. You will need ot set it to a gid that your cassandra system user is in.

The number of pages to reserve can be calculated by dividing your total heap size
in megabytes by 2. I usually round up for the first try. Once you see how many
pages Cassandra uses you can reduce the setting on the fly.

```
cat > /etc/sysctl.d/hugepages.conf <<EOF
vm.nr_hugepages=3000
vm.hugetlb_shm_group=1337
EOF
sysctl -p /etc/sysctl.d/hugepages.conf
grep HugePages_Total /proc/meminfo
```

That last grep is important! If Linux can't find space to allocate all of the huge
pages, the value of HugePages\_Total will be lower than you asked for (see below). The safe bet
at this point is to reboot. If nothing else is running on the system, the memory
can sometimes be freed up by dropping the VFS caches with `echo 1 |sudo tee /proc/sys/vm/drop_caches`.

Once hugepages are enabled in the kernel, the JVM has to be instructed to use them. This
requires adding the two flags to [cassandra-env.sh](https://gist.github.com/tobert/24f835809ed3ff3b05c7/revisions).

```
@@ -175,6 +175,9 @@ JVM_OPTS="$JVM_OPTS -ea"
 # add the jamm javaagent
 JVM_OPTS="$JVM_OPTS -javaagent:$CASSANDRA_HOME/lib/jamm-0.2.8.jar"

+# enable hugepages - requires some settings in /proc/sys/vm to work, fails gracefully
+JVM_OPTS="$JVM_OPTS -XX+UseLargePages -XX:LargePageSizeInBytes=2m"
+
 # some JVMs will fill up their heap when accessed via JMX, see CASSANDRA-6541
 JVM_OPTS="$JVM_OPTS -XX:+CMSClassUnloadingEnabled"
```

After making this change, start Cassandra again and then verify it by examining
/proc/meminfo. I picked 3000 at random. In this case it will fail to allocate
all 3000 requested but that's fine since my instance of Cassandra is configured
with a 4GB heap.

```
[atobey@moltar ~]$ echo 3000 |sudo tee /proc/sys/vm/nr_hugepages
3000
[atobey@moltar ~]$ grep Huge /proc/meminfo
AnonHugePages:    743424 kB
HugePages_Total:    2604
HugePages_Free:     2604
HugePages_Rsvd:        0
HugePages_Surp:        0
Hugepagesize:       2048 kB
[atobey@moltar ~]$ sudo docker start cass01
cass01
[atobey@moltar ~]$ grep Huge /proc/meminfo
AnonHugePages:    741376 kB
HugePages_Total:    2604
HugePages_Free:     2450
HugePages_Rsvd:     1970
HugePages_Surp:        0
Hugepagesize:       2048 kB
```

After getting it working, you'll want to make your nr\_hugepages number more precise. This
should usually be your heap size in megabytes / 2, in this case, 1970. I set it higher
at first because after Cassandra gets up and running it may still allocate memory beyond the
heap for off-heap, which ideally will be in hugepages. I checked my system again after
letting Cassandra run for a few minutes and the Rsvd pages number did increase slightly.

```
[atobey@moltar tobert.github.io]$ grep Huge /proc/meminfo
AnonHugePages:    712704 kB
HugePages_Total:    2604
HugePages_Free:     2357
HugePages_Rsvd:     1877
HugePages_Surp:        0
Hugepagesize:       2048 kB
```

So for these settings, I would set nr\_hugepages to 1900 and call it a day.

## Conclusion

I did some light performance testing on a quad-core Xeon machine and noticed *maybe* a 1%
improvement in performance. My suspicion is that it doesn't help a lot unless the machine
has lots of memory or a really large heap.

Most modern Linux distros ship with transparent hugepages enabled by default, so my recommendation
is to add -XX:+AlwaysPreTouch since it's a noop after startup and can provide some small performance
benefit. The -XX:+UseLargePages option is more work to configure and doesn't work as reliably.

As usual with modifying JVM options, test on one machine for a while then roll out slowly to be safe.

## Further Reading

http://lwn.net/Articles/517465/

http://structureddata.org/2012/06/18/linux-6-transparent-huge-pages-and-hadoop-workloads/

https://www.kernel.org/doc/Documentation/vm/hugetlbpage.txt

https://www.kernel.org/doc/Documentation/vm/transhuge.txt
