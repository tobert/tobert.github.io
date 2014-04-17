---
id: "2014-04-17-new-blog"
title: "New Blog"
section: "post"
abstract: "Sometimes you need a sharper tool."
tags: ["blogging", "blogger", "golang"]
pubdate: 2014-04-17T16:30:00Z
---

I've been blogging a bit more often lately and as a result, I've been getting
annoyed with the limitations of Blogger. It was a fine service for the years
when I didn't write much. Now that I'm doing a lot more technical blogging,
I need a more customizable platform to generate my content.

I prefer to not have to manage servers to host a blog, so I'm going with
[Github Pages](https://pages.github.com/).
After trying a couple content generation systems including Jekyll and Hugo, I decided to
write my own. It's limited and small but works exactly the way I want it to. And the code is
small so it's easy to hack on. It is in the same repo as the content for
(@AlTobey Writes)[http://tobert.github.io/] so there's nothing to install except for
Go and a couple go gets. I don't keep a binary around, preferring 'go run' since it's
fast enough for regular use. Presenting:
[build-blog.go](https://github.com/tobert/tobert.github.io/blob/master/build-blog.go)

It's probably obvious that the base content is Bootstrap 3. I picked it because most
things just work.

My favorite feature of this new setup, and one that I'd like to encourage my readers
to use is the Edit button in the upper right-hand corner. It should automatically fork
my blog so you can edit content. If you find something wrong or that needs additional
clarity, I'm happy to take pull requests. It's a bit weird for a blog, but since I write
about technical topics most of the time, it makes it easier for me to push content without
worrying about getting it perfect first.

