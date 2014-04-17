@AlTobey Writes
===============

This is my blog. There are many like it ...

Front Matter
============

Similar to what Jekyll and Hugo use. It's YAML at the top of the file,
either HTML or Markdown and will be stripped before rendering.

    ---
    id: "2014-04-09-fio-output-explained"
    title: "Fio Output Explained"
    abstract: "Fio packs a lot of information into its output. This is a section-by-section breakdown of what it's telling you."
    tags: ["fio", "benchmark", "documentation"]
    pubdate: 2014-04-17T00:00:00Z
    ---

All fields shown above are required.

Generating Content
==================

    git clone git@github.com:tobert/tobert.github.io.git
    cd tobert.github.io
    vim src/post/whatever.md
    go run build-blog.go
    git add src/post/whatever.md
    git commit -a

License
=======

Content and code by Al Tobey is licensed under a Creative Commons Attribution 4.0 International License.

