---
id: "2014-11-07-docker-entrypoints"
title: "Docker Entrypoints and the UX of Containers"
abstract: "Docker entrypoints provide an opportunity to give your users a better UX than they would have without Docker"
tags: ["docker", "cassandra", "entrypoint", "sprok"]
pubdate: 2014-11-07T20:30:00Z
---

### Entrypoints

When building containers using Dockerfiles, you can specify an entrypoint for the container
that Docker will use as process 1 inside the container. This is the default command that
is executed on `docker run`.

It seems that most people are going with a process supervisor these days. That's a fine
approach, but it's not what I'm here to talk about.

### Precision

One of my goals for the [cassandra-docker](https://github.com/tobert/cassandra-docker) images
is to have very precise control over every aspect of starting up the process. The problem with
that is that Cassandra, like most other JVM projects, uses a load of shell scripts to set up
all the command-line arguments to the JVM.

<script src="https://gist.github.com/tobert/43000a3a7da7ba389d31.js"></script>

