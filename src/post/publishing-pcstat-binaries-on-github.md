---
id: "2014-05-06-publishing-pcstat-binaries-on-github"
title: "Publishing pcstat Binaries on Github"
abstract: "How I publish pcstat binaries on Github without littering git's history with large binary objects."
tags: ["golang", "github", "git", "linux", "pcstat"]
pubdate: 2014-05-06T23:00:00Z
autoidx: true
---

I wrote a little tool for inspecting which pages of a file are cached by the Linux kernel
called [pcstat](https://github.com/tobert/pcstat).

I wrote it in Go so that it would compile down to a static binary so it can be trivially
pulled down to a system and run without worrying about the site environment or having
root permissions. It needs to be easy for [Datastax](http://datastax.com) solutions
architects and [Cassandra users](http://planetcassandra.com) to use quickly when evaluating
problems on production systems. Go is perfect for this, because it doesn't even use
a libc; it has its own ASM for [syscall(2)](http://man7.org/linux/man-pages/man2/syscall.2.html)
so the binary works on any Linux kernel that supports
[mincore(2)](http://man7.org/linux/man-pages/man2/mincore.2.html), which is pretty much
everything these days.

Great, so now I have a binary. It is static and completely lacks the ELF headers for
used for loading shared libraries.

```
atobey@brak ~/src/pcstat $ go build
atobey@brak ~/src/pcstat $ ls -l pcstat
-rwxr-xr-x 1 atobey wheel 3049296 May  6 16:10 pcstat
atobey@brak ~/src/pcstat $ ldd pcstat
    not a dynamic executable
atobey@brak ~/src/pcstat $ eu-readelf -d /bin/bash |grep NEEDED |wc -l
4
atobey@brak ~/src/pcstat $ eu-readelf -d pcstat |wc -l
0
```

Initially, I pushed binaries out to my Linode, but I'm not comfortable with that since
it's just one little VM with a bandwidth cap. One option would be to push the binaries
to the PlanetCassandra S3 bucket on Datastax's tab, but that's a pain in the butt even
with scripts and ultimately inappropriate. What I really want is to push the binaries
through Github.

Checking binaries into git repos is a Bad Idea. Basically once the object is in your
history graph and public, you're stuck with it forever. A second repo might work, but
that's ugly and requires me to copy stuff around. I've been kicking around this idea
that abuses git branches for a while and it seems to work.

The easy way to do this is with `git checkout --orphan`, which creates a new branch
that has no parent so it has its own history. This means that if the branch is deleted,
`git gc` can remove all of the data since there are no downstream commits connected to
its history.

I asked teh Twitters if Github does regular `git gc` and got a couple affirmative answers.

<blockquote class="twitter-tweet" lang="en"><p>Does anybody know if Github does `git gc` on any kind
of schedule? Playing with a hack that would rely on gc ...</p>&mdash; アルトビー (@AlTobey) <a
href="https://twitter.com/AlTobey/statuses/463805260838891520">May 6, 2014</a></blockquote>
<script async src="//platform.twitter.com/widgets.js" charset="utf-8"></script>

<blockquote class="twitter-tweet" data-conversation="none" lang="en"><p><a
href="https://twitter.com/AlTobey">@altobey</a> You can disable the automatic GC and schedule it
yourself if you want. (Schedule via cron etc)</p>&mdash; Andrew Gross (@awgross) <a
href="https://twitter.com/awgross/statuses/463806079294402560">May 6, 2014</a></blockquote>
<script async src="//platform.twitter.com/widgets.js" charset="utf-8"></script>

<blockquote class="twitter-tweet" data-conversation="none" lang="en"><p><a
href="https://twitter.com/AlTobey">@AlTobey</a> I heard they do it on every push. Not sure if that
were or still is the case, though.</p>&mdash; Chris Vest (@chvest) <a
href="https://twitter.com/chvest/statuses/463808095831855104">May 6, 2014</a></blockquote>
<script async src="//platform.twitter.com/widgets.js" charset="utf-8"></script>

Good enough for now. Let's do it.

```
atobey@brak ~/src/pcstat $ go build && mv pcstat pcstat.x86_64
atobey@brak ~/src/pcstat $ GOARCH=386 go build && mv pcstat pcstat.x86_32
atobey@brak ~/src/pcstat $ git checkout --orphan 2014-05-02-01
atobey@brak ~/src/pcstat $ git add pcstat.x86_64
atobey@brak ~/src/pcstat $ git commit -m "Check in x86_64 binary."
atobey@brak ~/src/pcstat $ git add pcstat.x86_32
atobey@brak ~/src/pcstat $ git commit -m "Check in x86_32 binary."
atobey@brak ~/src/pcstat $ git push --set-upstream origin 2014-05-02-01
```

What's happening above:

1. build an x86_64 binary (brak is running 64-bit Arch Linux)
1. build a 32-bit binary using GOARCH=386
1. create an orphaned branch for the release date + a counter
1. check in the binaries
1. push to Github

Now, I can access the binaries on Github's servers instead of my own and get
SSL as a bonus. Note: use `curl -L` with Github's raw URLs because they are
often redirects you want curl to follow. Here's an example of how this is
useful. Note that I'm running as a regular user using commands that are almost
universally available on production Linux systems.

```
atobey@brak ~/src/pcstat $ curl -L -o pcstat64 https://github.com/tobert/pcstat/raw/2014-05-02-01/pcstat.x86_64
atobey@brak ~/src/pcstat $ curl -L -o pcstat32 https://github.com/tobert/pcstat/raw/2014-05-02-01/pcstat.x86_64
atobey@brak ~/src/pcstat $ chmod 755 pcstat64 pcstat32
atobey@brak ~/src/pcstat $ ./pcstat32 pcstat64
|----------+----------------+------------+-----------+---------|
| Name     | Size           | Pages      | Cached    | Percent |
|----------+----------------+------------+-----------+---------|
| pcstat   | 3049296        | 745        | 745       | 100.000 |
|----------+----------------+------------+-----------+---------|
atobey@brak ~/src/pcstat $ echo 1 |sudo tee /proc/sys/vm/drop_caches
1
atobey@brak ~/src/pcstat $ ./pcstat64 pcstat32 pcstat64
|----------+----------------+------------+-----------+---------|
| Name     | Size           | Pages      | Cached    | Percent |
|----------+----------------+------------+-----------+---------|
| pcstat32 | 3049296        | 745        | 0         | 000.000 |
| pcstat64 | 3049296        | 745        | 745       | 100.000 |
|----------+----------------+------------+-----------+---------|
```

In the above example, I download both the 32 and 64-bit builds of pcstat to my
Linux workstation, "brak". The `chmod 755` makes the binaries executable. Then
I use the 32-bit binary to find out if Linux cached the 64-bit binary in a kind
of silly oroborus test. Since the files were recently written, 100% of pages
are cached. After instructing the Linux kernel to drop all caches I can see
that the pcstat32 binary is no longer cached while the pcstat64 binary is (because
it must be read to excute it).

I need to verify if my theory about cleanup actually works, but at the end of
the day I'm more interested in getting this tool in users' hands than whether
or not I'll run up against the repo size limit. If you happen to know for sure
please tweet at [@AlTobey](https://twitter.com/AlTobey) or file a an
[issue on pcstat](https://github.com/tobert/pcstat/issues/new).

Enjoy!<br/>
-Al
