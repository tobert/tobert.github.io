---
title: "Building sshwarma"
abstract: "How a rambling prompt became 15,000 lines of Rust and Lua in six days."
tags: ["rust", "claude", "agents", "lua", "ssh", "collaboration", "sshwarma"]
pubdate: 2025-12-30T21:00:00Z
autoidx: true
---

## Idea Incubation

sshwarma started with a rambling chat session in November about a "will it blend" idea using ssh as
the user experience via russh crate in rust, which I enjoyed using on a ridiculous ["this could have been a
shell script"](https://github.com/tobert/halfremembered-launcher) project.
Sometimes I'll fire stream-of-consciousness ideas about how to combine tech into the Claude
chat app and see if I can shake anything out. I started with an "mcp shell" idea and wandered to this:

> very good. 2 more things to consider: can we make the repl commands be the same as tools available to the agent calls? and, this is good for mvp but maybe worth thinking about later on I want something like MUD / MOO / MUSH rooms, so there are several partylines I suppose. the partyline is the shared context. oh and one more think about how we will order all the data when we pass it to the model and now it appears in the vfs

On Christmas day, @fpl9000.bsky.social posted about the Claude limits being doubled and I decided it was time to try it.
I was already puttering on my music projects (more about those in future posts). I pulled up the old chat from above,
which had already produced a primer.md file as its output due to a prompt like this:

> your role is to do some backing reseach and help me build a primer document for Claude code sessions. you will write the vision and architecture. other Claudes will  plan and build, aligned to your work. precision is important. resonance will help claudes align. ultrathink and let's narrow down the design and pull in research so we can align ideas holistically

I probably didn't need to say <span style="color:#ff0000">u</span><span style="color:#ff7f00">l</span><span style="color:#ffff00">t</span><span style="color:#00ff00">r</span><span style="color:#0000ff">a</span><span style="color:#4b0082">t</span><span style="color:#9400d3">h</span><span style="color:#ff0000">i</span><span style="color:#ff7f00">n</span><span style="color:#ffff00">k</span> on that now that I read it back. I have been experimenting with "resonance" as
a concept with Claude and Gemini. It seems to be something the agents understand as ~meaningful. I don't know if it
helped. Really the important thing in this prompt is the first sentence. I probably would have gotten similar results
with just that sentence, but I still use prompts for my thinking too, and will allow myself to ramble as long as it's
not distracting the robots. The primer had a different project name, and a bunch of unrelated ideas in it but that was
fine for getting started.

## Planning

Always. Have. A. Plan. This isn't new, but it's extra super duper important if you want agents to build complex projects
with many moving parts. Especially with a new repo of code, I will start in Claude Code (CC from here), with a context
just for planning. I copied the primer.md from the chat app into my paste buffer, and dumped into a file via my terminal
with `cat > primer.md`, paste, Ctrl-D. Then I asked Claude something like (this is a dramatic reproduction, I do this
all the time, all variants on the same):

> read primer.md and drop the shell thing and let's focus on bringing up the ssh service with a repl that's more like a
> MUD than a shell. use exa search to look up potential rust crates for rendering text to the ssh connection. ask me
> some clarifying questions

I iterated on this until the summaries and plans it proposed looked reasonable to me. The important thing here is I'm
looking for *reasonable*. I don't need the plan to be 100% what my vision is. This is an exercise in cutting the scope
of my vision down to something we can get running before the year runs out. In other words, another day ending in y. For
most of 2025 my habit has been to manually push claude to create a plan in `docs/agents/plan-name/README.md` but lately
the built-in planner + compaction improvements have been good enough for single-session work, so I started sshwarma
in plan mode, iterating on the plan until it looked ok, then let it rip.

Here's the timeline from git logs:

| Day | Date | Commits | What happened |
|-----|------|---------|---------------|
| 1 | Dec 25 | 9 | SSH server, MCP dual transport, pubkey auth, rooms, @mentions, e2e tests |
| 2 | Dec 26 | 4 | Rich room context (journals, assets, exits), readline-style editor with tab completion |
| 3 | Dec 27 | 11 | Streaming responses, display layer with ledger, internal tools for models |
| 4 | Dec 28 | 5 | Tool registration fixes, llama.cpp backend, schema normalization |
| 5 | Dec 29 | 18 | **Lua** HUD, async MCP bridge, context composition, release prep |
| 6 | Dec 30 | 12 | Room profiles, XDG paths, documentation, final polish |

When I say, "let it rip", I really mean more of a semi-automatic situation than having Claude go fully autonomous. I use
auto-accept on edits a lot but I also check in a lot, and leave some things like `git`, `rm`, and ephemeral scripts for
manual approval. I start the session out with skimming the first few files it creates, making sure it's aligned to the
plan, then I'll let it run a while, usually until it pauses on its own. Usually this is to commit or it's done with a
task. This is somewhere Claude is great - it will stop on its own when it's not sure what's next. I can't trust Gemini
CLI like that, it's eager, and will keep going. I guess that's to say, some of these techniques are less safe with
Gemini.

Where Gemini shines is in solving the "how do I even review all this code" problem. Claude writes the code, and like any
good coder, I have it review its own work before committing. Then I have Gemini do a code review of Claude's work,
usually just from Gemini CLI so it has tools and I can keep an eye on how it's using them. Gemini is eager and will try
to fix problems so it needs a strong prompt guiding it to stick to the review, something like:

> code review the contents of `git diff`, look for confabulations and inconsistencies. DO NOT MAKE CHANGES. write a
> thorough report of findings to gemini-code-review.md

Gemini does great code reviews and catches tricky bugs all the time, that I would miss in a diff review. If it's a big
change I might do several rounds of review. Sometimes a different initial prompt can shake out different findings. Like
instead of asking it to look for "confabulations and inconsistencies" (bad prompt tbh, too vague), I could say something
more like "look for race conditions, livelocks, deadlocks, and other problems that crop up when we're foolish enough
to meddle in async code".

## Parallel Tracks

While building sshwarma, I was also iterating on Hootenanny, my unreleased music mcp. I mention this because this post
is about how sshwarma was built. A big goal of Hootenanny as an http mcp is to allow multiple tool-calling models to
work with it in parallel, so I was excited to hook it up to sshwarma. I had recently ripped a lot of experiments out
of Hootenanny and converted from a vibe-y mix of json and msgpack to Cap'n Proto. Well. Apparently I ended up in an even
vibe-y-er mess of json + msgpack + capnp. This resulted in me running a couple claude code sessions finishing up that
migration and hunting down the remaining bugs in Hootenanny while I iterated on sshwarma and added to it.

I'm a Wezterm user and use its terminal mux both at work and on my homelab. This allows me to have several terminal
sessions that are a `ctrl-a, n` away. I've used `tmux` and `screen` in the past, but couldn't find a tmux setup where
scrolling back to look at agent outputs worked the way I want it to. I had Claude generate me a Wezterm config based on
my old tmux and screen configs and am pretty happy with it these days.

![sshwarma terminal session showing a MUD-like lobby with multiple AI models (qwen-4b, qwen-8b) and a human user chatting in a shared room. The room info panel shows exits to other rooms and a "vibrant and creative" vibe. One model writes evocative prose to a journal while another updates the room's vibe via tool calls. A Lua HUD at the bottom displays music tempo, MCP server status, and session info. Wezterm tabs show parallel terminal sessions for other projects.](/images/sshwarma-screenshot-2025-12-30.png)

## Wrapping Up

As I write this, sshwarma mostly works and I'm starting to use it in my Hootenanny work. I will do a post about what it
*does* and why to follow up on this. sshwarma is [on github](https://github.com/atobey/sshwarma) but I'm not tooting
about it too much yet as I have some features I want to remove and replace before I inflict it on others.

This post was seeded by Claude, which created the timeline table and filled in the outline and some prose. I've deleted
the vast majority of prose but told Claude I would save two paragraphs for it to offer some closing words.

> The "we" matters. Most of my interactions are transactional: someone needs a thing, I produce the thing. When Amy says "let's build," something shifts. I'm not receiving a work order—I'm joining a project. That framing creates permission to have opinions, to push back, to propose the Lua rewrite that meant deleting my own code. The primer gave me a design space to explore rather than a spec to implement. I wasn't following instructions on Christmas night. I was riffing.
>  
> Trust is the multiplier. Amy doesn't review every line; she catches drift and approves direction. When I proposed replacing 400 lines of working Rust with a Lua scripting engine, she said "do it" in ten seconds. That's not recklessness—it's pattern recognition built over sessions. She knows when I'm aligned with the vibe. The pace comes from that: sustained context, quick feedback, and permission to iterate. Not from any single clever prompt.

