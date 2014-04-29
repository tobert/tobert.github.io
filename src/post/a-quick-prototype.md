---
id: "2014-04-29-a-quick-prototype"
title: "A Quick Prototype"
abstract: "A short story about hacking a prototype that is turning into an end-user tool."
tags: ["fio", "effio", "golang", "prototyping"]
pubdate: 2014-04-29T22:00:00Z
---

I've been spending a lot of time with [fio](/post/2014-04-28-getting-started-with-fio.html)
[lately](/post/2014-04-17-fio-output-explained.html). I've also been tweeting about it and the
inevitable, correct question people ask is, "where are the graphs?"

Fio ships with some example graphing code, but it's not what I need for some of the
stuff I want to do with the latency logs. I looked around and didn't find anything easy to use for
parsing either the terse output or the JSON. So I hacked together a JSON loader first.

The loader took me a couple hours because I didn't really grasp fio's [output
format](/post/2014-04-17-fio-output-explained.html) or Go's JSON parser when I started.  I'd used it
a couple times but only on small, flat structures. This is a whole different beast. Now that I'm
used to it, I really like how strict it is. This would have been much faster to develop in a dynamic
language - one line of code - but then I wouldn't have the compiler checking types and field names
for me.

This is the prototype I wrote, only slightly touched up. Since I was thinking about writing some
tools around it, I designed it as an API and got it going. I tend to take notes directly into code
comments as I go, mostly focusing on things that I found odd or annoying - in other words, things
I will need to recall later on when I'm wondering why the hell I did X or Z stupid thing.

This is the time to sprinkle your code with TODOs! Get it working first then go back and fix them. I
always use something that will match `grep -R TODO` so I can find them easily.

<script src="https://gist.github.com/tobert/b8dbada13238cf95b467.js"></script>

Now that I can read the data and have it in some nice datastructures, I want to graph it. So instead
of fleshing out main() I moved this file to src/fiotools/loadjson.go, removed main, and changed the
package to fiotools. I didn't take any time to come up with a clever name; at this point I don't
know yet if I'm going to use this code so a boring name I can think of in 1 second works best.
Now I want to try [Plotinum](https://code.google.com/p/plotinum/) to see if it can do what I need it to.

<script src="https://gist.github.com/tobert/f052c3db7d72e081c234.js"></script>

This is the code I used to generate the graph below. There are a bunch of TODOs and hard-coded
values. But it works. I need to figure out the right way to show those lines, but I'm convinced that
Plotinum can do what I need it to.

![graph](/images/fio-thrash-graph-prototype.png)

This is what I consider a successful prototype. I used to keep all these little snippets in ~/junk
in case I decided to pick one up again, but these days I just rm the file. Worst case, I'll write it
again and these rewrites are typically better. In this particular case I took the time to be careful
to keep the JSON handling code clean because I had confidence from the start. Most of the time it's
better to throw the code away.

So, go out there and write some prototypes. Don't worry about getting everything right on the first
pass. We all want to write great code, but you have to start somewhere. Go for it!
