---
id: "2014-04-21-packer-hacking"
title: "Packer Hacking"
abstract: "Rebuilding the PlanetCassandra VM image with Packer"
tags: ["packer", "cassandra", "virtual machine", "golden image", "arch linux"]
pubdate: 2014-04-21T20:00:00Z
draft: false
---

My first technical project for Datastax was to build a virtual machine that runs
Cassandra. This is pretty straightforward most of the time, but we came up with some
additional goals to make things more accessible.

* total VM size less than 1GB
* boots to a single-node Cassandra instance
* make IP/port information easy to find and use
* compatible with VirtualBox and VMware Player/Workation
* bonus: work with KVM and/or Hyper-V

I added a couple extra things to keep it interesting enough to get it done.

* runs in under 1GB RAM
* full systemd integration
* tty.js console for browser access to cqlsh

In the first release in October 2013, only the first three goals were met. It worked with VirtualBox
but it was a pain in the butt. In the last update
the image was built in VirtualBox and exported as an [OVA]() that works with both Vbox and VMware.

In preparation for [Cassandra Week](), [Rebecca Mills]() and [Patrick McFaddin]() came up with a few
more features that we hope will be useful to future users of the image.

* multiple tty.js instances to provide cqlsh and shell access
* an easy way to start/stop Cassandra from the browser
* make the web setup look a little nicer
* upgrade to Cassandra 2.0.7

This is all pretty easy to implement, but as we add features & functionality, it's getting more
important that the process be repeatable and, ideally, not be something that's in my head alone.
This is where [Packer](http://packer.io) comes in.
