---
id: "2014-11-12-docker-entrypoints"
title: "Docker Entrypoints and the UX of Containers"
abstract: "Docker entrypoints provide an opportunity to give your users a better UX than they would have without Docker"
tags: ["docker", "cassandra", "entrypoint", "sprok"]
pubdate: 2014-11-12T00:15:00Z
---

### Entrypoints

When building containers using Dockerfiles, you can specify an entrypoint for the container
that Docker will use as process 1 inside the container. This is the default command that
is executed on `docker run`. It looks like this: `ENTRYPOINT /bin/cassandra-docker`. When this is in place,
`docker run` will execute that program with remaining arguments passed to it unless the user overrides
it with `docker run --entrypoint=command`. This has some handy side-effects.

It seems that most people are going with a process supervisor these days. That's a fine
approach, but it doesn't take full advantage of shipping your software inside a Docker image.

### Precision

One of my goals for the [cassandra-docker](https://github.com/tobert/cassandra-docker) images
is to have very precise control over how Cassandra and its tools start. It is common to have to edit
shell scripts to tune projects that use the JVM and this has always bothered me. What started out as a
clever way to build a classpath has grown over time to be a sprawling set of shell scripts with lots
of undefined behavior. I also don't like the way '../' shows up in so many paths. It could be cleaned up
but it works, so there it is. It makes me itchy every time I see it.

```
cassandra 9359 9358 19 19:34 pts/0 00:00:06 java -ea -javaagent:./../lib/jamm-0.2.6.jar -XX:+CMSClassUnloadingEnabled -XX:+UseThreadPriorities -XX:ThreadPriorityPolicy=42 -Xms3941M -Xmx3941M -Xmn800M -XX:+HeapDumpOnOutOfMemoryError -Xss256k -XX:StringTableSize=1000003 -XX:+UseParNewGC -XX:+UseConcMarkSweepGC -XX:+CMSParallelRemarkEnabled -XX:SurvivorRatio=8 -XX:MaxTenuringThreshold=1 -XX:CMSInitiatingOccupancyFraction=75 -XX:+UseCMSInitiatingOccupancyOnly -XX:+UseTLAB -XX:CompileCommandFile=./../conf/hotspot_compiler -XX:CMSWaitDuration=10000 -XX:+UseCondCardMark -Djava.net.preferIPv4Stack=true -Dcom.sun.management.jmxremote.port=7199 -Dcom.sun.management.jmxremote.rmi.port=7199 -Dcom.sun.management.jmxremote.ssl=false -Dcom.sun.management.jmxremote.authenticate=false -Dlogback.configurationFile=logback.xml -Dcassandra.logdir=./../logs -Dcassandra.storagedir=./../data -Dcassandra-foreground=yes -cp ./../conf:./../build/classes/main:./../build/classes/thrift:./../lib/airline-0.6.jar:./../lib/antlr-runtime-3.5.2.jar:./../lib/apache-cassandra-2.1.1.jar:./../lib/apache-cassandra-clientutil-2.1.1.jar:./../lib/apache-cassandra-thrift-2.1.1.jar:./../lib/commons-cli-1.1.jar:./../lib/commons-codec-1.2.jar:./../lib/commons-lang3-3.1.jar:./../lib/commons-math3-3.2.jar:./../lib/compress-lzf-0.8.4.jar:./../lib/concurrentlinkedhashmap-lru-1.4.jar:./../lib/disruptor-3.0.1.jar:./../lib/guava-16.0.jar:./../lib/high-scale-lib-1.0.6.jar:./../lib/jackson-core-asl-1.9.2.jar:./../lib/jackson-mapper-asl-1.9.2.jar:./../lib/jamm-0.2.6.jar:./../lib/javax.inject.jar:./../lib/jbcrypt-0.3m.jar:./../lib/jline-1.0.jar:./../lib/jna-4.0.0.jar:./../lib/json-simple-1.1.jar:./../lib/libthrift-0.9.1.jar:./../lib/logback-classic-1.1.2.jar:./../lib/logback-core-1.1.2.jar:./../lib/lz4-1.2.0.jar:./../lib/metrics-core-2.2.0.jar:./../lib/netty-all-4.0.23.Final.jar:./../lib/reporter-config-2.1.0.jar:./../lib/slf4j-api-1.7.2.jar:./../lib/snakeyaml-1.11.jar:./../lib/snappy-java-1.0.5.2.jar:./../lib/stream-2.5.2.jar:./../lib/stringtemplate-4.0.2.jar:./../lib/super-csv-2.1.0.jar:./../lib/thrift-server-0.3.7.jar org.apache.cassandra.service.CassandraDaemon
```

So what about precision?

While the most common options - heap sizes - can be overridden in `/etc/{default,sysconfig}/cassandra`, there are plenty
of other settings I'm interested in benchmarking that require editing a shell script. This could be automated using
templates, but the number of input variables grows to include most of them quickly if you want to avoid using conflicting
settings (such as different GC strategies).

```
# GC tuning options
JVM_OPTS="$JVM_OPTS -XX:+UseParNewGC"
JVM_OPTS="$JVM_OPTS -XX:+UseConcMarkSweepGC"
JVM_OPTS="$JVM_OPTS -XX:+CMSParallelRemarkEnabled"
JVM_OPTS="$JVM_OPTS -XX:SurvivorRatio=8"
JVM_OPTS="$JVM_OPTS -XX:MaxTenuringThreshold=1"
JVM_OPTS="$JVM_OPTS -XX:CMSInitiatingOccupancyFraction=75"
JVM_OPTS="$JVM_OPTS -XX:+UseCMSInitiatingOccupancyOnly"
JVM_OPTS="$JVM_OPTS -XX:+UseTLAB"
JVM_OPTS="$JVM_OPTS -XX:CompileCommandFile=$CASSANDRA_CONF/hotspot_compiler"
JVM_OPTS="$JVM_OPTS -XX:CMSWaitDuration=10000"

```

Here we see procedural code being used to build a string of options to pass to the JVM. This is what almost everybody does, but it is not how the kernel does things. The execve(2p) system call looks like this: `int execve(const char *path, char *const argv[], char *const envp[]);`. Path is usually something like `/usr/bin/java`, argv[] is an _array_. Envp, surprisingly, is another array of `KEY=VALUE` pairs.

So what I'm saying is, the way processes are started is nowhere near precise. We're passing an array of parameters to a shell script that turns those params into variables that get concetenated into a huge string so it can be parsed again and turned back into a couple arrays. In the process, a bunch of environment variables get leaked, perhaps a [shellshock exploit](https://github.com/tobert/sh-c-shock/blob/master/test.sh), not to mention all the opportunities for strings to get mangled.

Ever since the shellshock fiasco, I've been thinking about how to get the shell out of the process of launching services. We've come a long ways in recent years with things like [systemd units](http://www.freedesktop.org/software/systemd/man/systemd.unit.html), but even that doesn't go far enough in my opinion. Why use a command strings instead of having a syntax for argv [arrays](https://github.com/toml-lang/toml#array). When my search for a precise launcher kept coming up empty, I decided to hack something up and now [sprok](https://github.com/tobert/sprok) exists. I wired up a few things and felt like it is worthwhile.

### Entrypoints

The new entrypoint for cassandra-docker, [uses sprok to achieve precision](https://github.com/tobert/cassandra-docker/blob/master/conf/sproks/cassandra.yaml). It also uses it to enable some UX improvements I've wanted for a while, such as wrapping all the subcommands, moving all mutable state (config, logs, data) onto a single root path, and precision.

### User Experience

Projects like Cassandra tend to have tools that are shipped with the core product - you know, nodetool, cqlsh, cassandra-stress. Mysql has `mysql`, `mysqld`, and a host of supporting utilities. The special thing about JVM projects is that they ship consistently awful shell code in reliably inconsistent filesystem layouts. This leads to
a poor user experience across projects. A comprehensive entrypoint provides an opportunity to work
around this problem by providing a consistent experience.

For Cassandra, this is what it looks like in [cassandra-docker](https://github.com/tobert/cassandra-docker):

```sh
docker run --name cass -d -v /srv/cassandra:/data tobert/cassandra:2.1.1 cassandra -name "My Cluster"
docker exec cass nodetool status
docker exec cass cassandra-stress
docker exec cass cqlsh
```

This intentionally looks just like the commands are being executed as-is, but that is not the case! While
I could have set the PATH to include `/opt/cassandra/bin:/opt/cassandra/tools/bin`, that does not provide
any way to hook these tools and provide settings automatically, such as ports and IP addresses. This is
a little tricky, but can be quite effective with a little pre-parsing of command-line arguments passed
to the entrypoint.

For example, `docker exec cass cqlsh` is automatically passed the IP of the default interface as its
first argument *if no arguments are specified*. This is the expected default behavior for cqlsh, but
it didn't work in my older containers because Cassandra is bound on the default IP rather than localhost.

Another example is running nodetool. The nodetool shell script appends the JMX port with `-p $JMX_PORT`
if the -p option is not used. The entrypoint takes care of this same job but can make more informed
decisions about what parameters to pass since it's programmed to be "docker aware".

### Externalizing Mutable State

One other goal I had for cassandra-docker is to move all mutable state onto a single volume. Since
a volume in Docker is a simple bind mount -- something a little like a symlink for directories --
it can be mapped anywhere
on the host system. The recommendation for Cassandra in Docker is to always use a volume. With the
configuration for Cassandra also residing on the volume, you get the additional advantage of being
able to modify all of Cassandra's config from the host rather than having to enter the container.

```sh
docker run --name cass -d -v /srv/cassandra:/data tobert/cassandra:2.1.1 cassandra -name "My Cluster"
vim /srv/cassandra/conf/cassandra.yml
```

Where the entrypoint really helps Cassandra is with performance tuning. Many tuning options for Cassandra
require editing shell scripts such as `/etc/cassandra/cassandra-env.sh`. While this could be easily
put on the volume just like the sprok configs, it provides too much opportunity for strange behavior
when related scripts search the filesystem for various configs (take a look at what bin/cassandra does).
Instead, the cassandra-docker entrypoint uses sprok configuration files to set up the JVM parameters and
all commands' parameters are now in `$VOLUME/conf/sproks/<command>.yaml`. Changing the heap or any
other parameter outside of the usual config files is now declarative and precise.

### Supervisors, Part 2

Many of my ops coleagues will have noticed that sprok and the cassandra-docker entrypoint call exec()
and do not supervise processes. This is because I believe that process supervision is something the
user or the user's framework should handle. Having the user handle supervision means they get full
visibility into failures and get better control over containerized processes. This way, users who
like runit or systemd or whatever can specify their own restart policies and target `docker run` rather
than the cassandra process directly. Users of systems like Kubernetes, Fig, Mesos, and the others
can use the same image without worrying about complex interactions between competing process supervisors.

### Conclusion

A great entrypoint should be boring for your users. It can take care of all the details of running
a given application inside Docker without requiring the user to learn project layouts or other minutae
of the containerized application. For simple entrypoints with simple configurations,
[sprok](https://github.com/tobert/sprok) might
be applicable, but I suspect you will find interesting ways to build entrypoints on your own.

Please let me know if you see an entrypoint you like but tweeting at [@AlTobey](https://twitter.com/AlTobey).

