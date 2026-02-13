---
id: "2026-02-12-building-kaish"
title: "Building kaish: 48,000 Lines of Rust in 25 Days"
abstract: "A shell for AI agents, built with one. 217 commits, 109 Claude sessions, and the messy reality of human-agent collaboration."
tags: ["rust", "claude", "agents", "shell", "mcp", "collaboration", "kaish"]
pubdate: 2026-02-12T21:00:00Z
autoidx: true
---

## 会sh (kaish)

kaish is a shell designed to be embedded in AI toolchains, providing a familiar subset of Bourne
shell syntax with stronger validation and an embedded-first design. Kaish provides several builtins for coreutils and
beyond that make up most of the shell tasks I observed my agents coming up with. It looks like this:

```sh
#!/usr/bin/env kaish

# glob tool replaces inline * expansion
# scatter/gather provide easy builtin parallelization
echo ""
echo "Lines of code per crate:"
glob "crates/*/src/lib.rs" \
    | scatter as=F limit=8 \
    | wc -l "$F" \
    | gather \
    | sort -k 2 -rn

for F in "main.rs" "app.tsx" "notes.md"; do
    case $F in
        *.rs)          echo "🦀 $F" ;;
        *.{js,ts,tsx}) echo "🌐 $F" ;;
        *)             echo "📄 $F" ;;
    esac
done
```

It is available now on [crates.io](https://crates.io/crates/kaish-repl) and [GitHub](https://github.com/tobert/kaish).

## The Name

会sh was originally prototyped as part of a more ambitious project,
[会術 Kaijutsu](https://github.com/tobert/kaijutsu) and was separate enough
it made sense to split it out. The k+ai works out nicely too, sortof "Korn-like shell
for AI".

## The Story

I started working with Claude Code in early 2025, both at work and on [personal projects](https://github.com/tobert).
Through building a small game in Godot and Bevy, I got the
hang of extruding code with Claude, and got more ambitious. I spent a lot of the doubled quota over
the 2025 holidays [building sshwarma](https://tobert.github.io/post/2025-12-30-building-sshwarma.html)
and continuing my music MCP. As I hit limits in the terminal, I kicked off a Bevy UI prototype and
while designing its collaboration model, I started drafting kaish to be a shell
that it could embed, giving full control over tool IO.

The first running versions of kaish had a lot of goofy ideas I threw in to get things rolling. The
language quickly drifted away from Bourne, bringing in some json features to accommodate MCPs, and
as features diverted it became clear a new shell language was not viable. So we stripped away
a lot of the experimental features and landed at an 80/20 rule: 会sh avoids more esoteric and
unsafe features of the shell, aiming to be compatible with strict shellcheck, but not full bash.
This removes a lot of complexity, so kaish can have fewer footguns and surprising behaviors.

I kept a few though.

```sh
#!/usr/bin/env kaish

# computing squares:
# seq generates an array internally
# scatter uses the array, assigning each item to locally scoped `$N`
# gather brings the json together into a list
# jq selects items from the list
seq 1 6 \
    | scatter as=N limit=4 \
    | echo "{\"n\": $N, \"sq\": $((N * N))}" \
    | gather format=json \
    | jq '[.[] | .out | fromjson] | sort_by(.n)' -r

# git is built in (git2-rs — no fork, no exec) (experimental)
git log --oneline | head -n 5

```

## The Architecture

Rather than a traditional interpreter, kaish has an embeddable kernel architecture. The
repl uses the same kernel as the MCP, and applications can embed kaish in-process or
as an external process over a socket. This allows for isolation options such as putting
the kaish kernel in a container or virtual machine.

```
kaish-kernel    — parser and runtime
kaish-client    — client trait + embedded/IPC implementations
kaish-mcp       — MCP server (can run sub-MCPs as a client)
kaish-schema    — Cap'n Proto definitions
kaish-glob      — standalone glob matching library
kaish-repl      — Interactive REPL, script runner, CLI
```

Another problem I wanted to solve in kaish was stringy typing, at least a little bit. Instead
of boiling the ocean with types, kaish leans into builtins so they can natively emit structured
data such as arrays to each other, even if it ends up smooshed into strings at the surface. This
compromise seems to work out well in the example above: tools like seq, glob, etc. return lists
and in kaish they don't get flattened.

## Try it!

To be honest I haven't used the repl as much yet, but use the MCP regularly now. You can get
the repl with `cargo install kaish-repl` or try the MCP:

```sh
cargo install kaish-mcp
claude mcp add kaish ~/.cargo/bin/kaish-mcp
gemini mcp add kaish ~/.cargo/bin/kaish-mcp
```

Check out the [README](https://github.com/tobert/kaish#mcp-integration) for more. PRs from you
and your agents are welcome.

One last example before I :wq. kaish has a `tokens` builtin for estimating token counts.

```sh
cat ~/src/tobert.github.io/src/post/2026-02-12-building-kaish.md | tokens
1314
```

Enjoy :)

---

*The prose in this post was written by Amy Tobey and reviewed by Claude Opus 4.6.
Code examples were created by Claude and edited to fit the post.*

