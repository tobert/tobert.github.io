---
id: "2014-07-15-installing-cassandra-spark-stack"
title: "Installing the Cassandra / Spark OSS Stack"
abstract: "An overview of the steps required to install the Apache Cassandra + Spark stack on Linux."
tags: ["cassandra", "spark"]
pubdate: 2014-07-15T23:00:00Z
---

## Init

As mentioned in my [portacluster system imaging](/post/2014-07-14-portacluster-system-imaging.html) post,
I am performing this install on 1 admin node (node0) and 6 worker nodes (node\[1-6]) running 64-bit Arch Linux.
If you're attempting to do the same on a Redhat or Debian variant, you will need to
change package names and commands for the target distro. The
rest of the process should port over without much modification.

## Overview

When assembling an analytics stack, there are usually myriad choices to make. For this build, I decided to
build the smallest stack possible that lets me run Spark queries on Cassandra data. As configured it is
not highly available since the Spark master is standalone. It's a decent tradeoff for portacluster, since
I can run the master on the admin node which doesn't get rebooted/reimaged constantly. I'm also going to
skip HDFS or some kind of HDFS replacement for now. Options I plan to look at later are GlusterFS's HDFS
adapter and [Pithos](http://pithos.io) as an S3 adapter. In the end, the stack is simply Cassandra and
Spark with the [spark-cassandra-connector](https://github.com/datastax/spark-cassandra-connector).

## Install Cassandra

My first pass at this section involved setting up a package repo, but since I don't have time to package
Spark properly right now, I'm going to use the tarball distros of Cassandra and Spark to keep it simple.
[joschi](https://github.com/joschi) maintains a package on the [AUR](https://aur.archlinux.org/packages/cassandra/)
but I have chosen not to use it for this install.
I'm also using the Arch packages of OpenJDK, which isn't supported by Datastax, but works fine for hacking.
The JDK is pre-installed on my Arch image, it's as simple as `sudo pacman -S extra/jdk7-openjdk`.

First, I downloaded the Cassandra tarball from [apache.org](http://cassandra.apache.org/download/) to
node0 in /srv/public/tgz.

```
sudo curl -o /srv/public/tgz/apache-cassandra-2.0.9-bin.tar.gz \
  http://mirrors.gigenet.com/apache/cassandra/2.0.9/apache-cassandra-2.0.9-bin.tar.gz
```

Then on the worker nodes, it gets downloaded and expanded in /opt.
[cl-run.pl](https://github.com/tobert/perl-ssh-tools/blob/master/cl-run.pl) is part
of my custom parallel ssh tools. pssh, etc. will work similarly.

```
pkg="apache-cassandra-2.0.9-bin.tar.gz"
cl-run.pl -c "curl http://node0/tgz/$pkg |sudo tar -C /opt -xzf -"
cl-run.pl -c "sudo ln -s /opt/apache-cassandra-2.0.9 /opt/cassandra"
```

To make it easier to do upgrades without regenerating the configuration, I
relocate the conf dir to /etc/cassandra to match what packages do. This assumes there
is no existing /etc/cassandra.

```
cl-run.pl -c "sudo mv /opt/cassandra/conf /etc/cassandra"
cl-run.pl -c "sudo ln -s /etc/cassandra /opt/cassandra/conf"
```

I will start Cassandra with a systemd unit, so I push that out as well. This unit
file runs Cassandra out of the tarball as the cassandra user with the stdout/stderr going
to the systemd journal (which you can view with `journalctl -f`). I also included some
ulimit settings and bump the OOM score downwards to make it less likely that the kernel
will kill Cassandra when out of memory. Finally, since the target machines have 16GB of
RAM, the heap needs to be set to 8GB (cassandra-env.sh calculates 3995M which is way too low).

```
cat > cassandra.service <<EOF
[Unit]
Description=Cassandra Tarball
After=network.target

[Service]
User=cassandra
Group=cassandra
RuntimeDirectory=cassandra
PIDFile=/run/cassandra/cassandra.pid
ExecStart=/opt/cassandra/bin/cassandra -f -p /run/cassandra/cassandra.pid
StandardOutput=journal
StandardError=journal
OOMScoreAdjust=-500
LimitNOFILE=infinity
LimitMEMLOCK=infinity
LimitNPROC=infinity
LimitAS=infinity
Environment=MAX_HEAP_SIZE=8G HEAP_NEWSIZE=1G CASSANDRA_HEAPDUMP_DIR=/srv/cassandra/log

[Install]
WantedBy=multi-user.target
EOF

cl-sendfile.pl -x -l cassandra.service -r /etc/systemd/system/multi-user.target.wants/cassandra.service
cl-run.pl -c "sudo systemctl daemon-reload"
```

Since all Cassandra data is being redirected to /srv/cassandra and it's going to run as the
cassandra user, those need to be created.

```
cat > user.sh <<EOF
mkdir -p /srv/cassandra/{log,data,commitlogs,saved_caches}
(grep -q '^cassandra:' /etc/group)  || groupadd -g 1234 cassandra
(grep -q '^cassandra:' /etc/passwd) || useradd -u 1234 -c "Apache Cassandra" -g cassandra -s /bin/bash -d /srv/cassandra cassandra
chown -R cassandra:cassandra /srv/cassandra
EOF

cl-run.pl -x -s user.sh
```

## Configure Cassandra

Before starting Cassandra I want to make a few changes to the standard configurations. I'm not a big
fan of LSB so I redirect all of the /var files to /srv/cassandra so they're all in one place. There's
only one SSD in the target systems so the commit log goes on the same drive.

Note that my systems have a bridge in front of the default interface. You will need to adjust the
ip=$() line to work on your systems.

```
cat config.sh
ip=$(ip addr show br0 |perl -ne 'if ($_ =~ /inet (\d+\.\d+\.\d+\.\d+)/) { print $1 }')

perl -ibak -pe "
  s/^(cluster_name:).*/\$1 'Portable Cluster'/;
  s/^(listen|rpc)_address:.*/\${1}_address: $ip/;
  s|/var/lib|/srv|;
  s/(\s+-\s+seeds:).*/\$1 '192.168.10.11,192.168.10.12,192.168.10.13,192.168.10.14,192.168.10.15,192.168.10.16'/
" /opt/cassandra/conf/cassandra.yaml
# EOF

cl-run.pl -x -s config.sh
```

The default log4-server.propterties has log4j printing to stdout. This is not desirable in a background
service configuration, so I remove it. The logs are also now written to /srv/cassandra/log.

```
cat > log4j-server.properties <<EOF
log4j.rootLogger=INFO,R
log4j.appender.R=org.apache.log4j.RollingFileAppender
log4j.appender.R.maxFileSize=20MB
log4j.appender.R.maxBackupIndex=20
log4j.appender.R.layout=org.apache.log4j.PatternLayout
log4j.appender.R.layout.ConversionPattern=%5p [%t] %d{ISO8601} %F (line %L) %m%n
log4j.appender.R.File=/srv/cassandra/log/system.log
log4j.logger.org.apache.thrift.server.TNonblockingServer=ERROR
EOF

cl-sendfile.pl -x -l log4j-server.properties -r /opt/cassandra/conf/log4j-server.properties
```

And with that, Cassandra is ready to start.

```
cl-run.pl -c "sudo systemctl start cassandra.service"
ssh node3 tail -f /srv/cassandra/log/system.log
```

