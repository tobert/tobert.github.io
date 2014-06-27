---
id: "2014-06-26-cassandra-and-docker"
title: "Running Cassandra inside Docker"
abstract: "Introduction to running Cassandra inside Docker."
tags: ["docker", "cassandra"]
pubdate: 2014-06-26T20:30:00Z
---

### TL;DR: use volumes for /var/lib/cassandra

As a fan of Linux containers and evangelist for Apache Cassandra, I get a lot
of questions about running Cassandra in Docker containers. I've run production
Cassandra clusters with cgroups in the past and had good luck with it, but
normally don't have much use for namespaces. One place where full containers
really helps is running clusters on a single machine.

### A new image

I looked around at some of the community Docker images there are some
out there, but I wanted something with less moving pieces.
Ideally, users (you, hopefully!) can pull an image and be up and
running with Cassandra without having to see a git repo. I also
wanted to go through the process of building a new Docker image since I
haven't done it in a really long time. The new code can be found at
https://github.com/tobert/cassandra-docker

### Single-node Quick Start

If all you want to do is run a single-node Cassandra instance in docker,
this should get you going. This command will pull down the tobert/dsc208
image I've published in the public registry and then run it with data
stored in the host's /srv/cassandra directory.

```
docker pull tobert/dsc20
mkdir /srv/cassandra
docker run -v /srv/cassandra:/var/lib/cassandra dsc208
```

### Cluster Quick Start

The process starts out the same way for a cluster, but there's a little more work to
do to pass in the seeds. You will need to create a data directory for each node in
the cluster, then tell Docker to use that as a volume inside each container. To get
gossip working one node needs to be started first so it can be used as a seed for the others.

```
docker pull tobert/dsc208

mkdir /srv/{cass0,cass1,cass2}

# pass in heap settings with envvars
HEAP="-e MAX_HEAP_SIZE=1G -e HEAP_NEWSIZE=100M"

docker run -d --name cass0 $HEAP -v /srv/cass0:/var/lib/cassandra dsc208
IP=$(< /srv/cass0/etc/listen_address.txt)

# you only need to set SEEDS the first time, but it doesn't hurt
docker run -d --name cass1 $HEAP -e SEEDS=$IP -v /srv/cass1:/var/lib/cassandra dsc208
docker run -d --name cass2 $HEAP -e SEEDS=$IP -v /srv/cass2:/var/lib/cassandra dsc208

# wait a minute or two for the gossip to converge
nodetool -h $IP status
```

### Building the Image

This image uses a simple Dockerfile. It should be straightforward.

```
git clone https://github.com/tobert/cassandra-docker
cd cassandra-docker
docker pull ubuntu:raring
docker build -t dsc208 .
```

## Conclusion

Running Cassandra in Docker isn't ideal, but it can be quite handy for development and QA work.
For now, I still don't recommend running dockerizing production Cassandra clusters, but as
the configuration tooling changes, I may change my mind.

