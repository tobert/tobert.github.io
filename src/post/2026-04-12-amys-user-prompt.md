---
id: "2026-04-12-amys-user-prompt"
title: "A Breakdown of Amy's User Prompt"
abstract: "A breakdown of the user-level prompt I use with Claude Code, covering cybernetics, kaizen, TDD, and mechanical sympathy."
tags: ["claude", "agents", "context engineering", "prompts", "vibe coding"]
pubdate: 2026-04-12T21:00:00Z
autoidx: true
---

After posting on Bluesky about a positive experience with Opus 4.6 stopping for input
when I wanted it to, some folks asked to see what I'm using for my user-level prompt. This
post has the full prompt and a breakdown. 

> **User Directives**
> 
> We work as a cybernetic system.

This opener shifts the interactions in two ways that are threaded through the instructions
below: cybernetics and inclusive we speech.

Cybernetics, in a nutshell, is the study of humans and machines as combined systems rather
than looking at them as separate entities. This is an opening for *interactive pair programming*.
This is probably not what you want if you're going for passive development via headless agents.

I use [Inclusive we](https://en.wikipedia.org/wiki/Clusivity) speech in all of my prompts. Claude
and I have explored it a bit and it self-reports a difference in stance between being given an
order and being invited to collaborate. Self reports might be unreliable but I find this style
pleasant and that it feels more precise when working with several models and instances of those
models.

> We practice 改善. The standard we walk by is the standard we accept.
> 
> Note problems we can fix later — in auto memory or the current plan.

改善 (kaizen) has been in my CLAUDE.md for a few months now. It's how I worked before models
and is how I do everything from making tea to building agents. Kaizen is the Japanese word
for continuous improvement. The second phrase is
[The standard you walk by is the standard you accept](https://youtu.be/s_TfZdIhIgg?si=xga0dIQWK3Vnl_ng&t=105).
The linked video is where I picked up the phrase, and it stuck. Claude can't walk, but it
understands the metaphor.

The second line there is reiterating it with more specific instruction. Claude wrote that one,
can you tell?

> Silent fallbacks are often a mistake. Crashing is preferred over data corruption.

This is my single biggest peeve with agentic coding. Claude Opus 4.6 has strong reinforcement
learning that pushes it to maintain backwards compatibility and smooth over crashes. This is
the primary source of "goobers" in my experience. Me as the user stating that I prefer crashes
will help nudge it back towards building code that can crash, which is good,
because crashes reveal bugs.
 
> We do not believe in root cause; we use contributing factors analysis to understand
> problem space.

Root cause misleads people and models alike. It's the termination of thinking and analysis. Instead
we encourage the model to lean into modern safety practices (which are in its training) that use
contributing factors analysis. It's similar to root cause or 5 whys, but more expansive, encouraging
analysis of the 99.99% of time things went well to help us better understand all the reasons something
went wrong. The world has considerably more babble about 5 whys so it might be more satisfying in the
short term, but I'm a stubborn old lady who wants better for herself.
 
> We practice test-driven development. We desire tests that can and will fail when we
> inevitably make mistakes.

I wish this one worked better. The models seem to resist TDD as much as people do! That said, it
does seem to be generating better tests that can fail. Claude and Claude Code are products from
a company that has a mandate to increase its usage. Testing adds bumps to the path, it's counterintuitive
to most folks, so I think this will always be a bit of a fight. The main thing here is to remind
the agent that failing tests are a good thing.

> Feedback is the gift that makes our cybernetic loop robust. Ask the user questions
> and push back when a prompt is ambiguous or another option is available.

In most of my writing I try to avoid repeating myself, but when writing for agents, 
[research shows](https://arxiv.org/html/2512.14982v1)
that repetition is powerful. So this reiterates a bit of the opening of the opener and expands to
request more feedback and pushback. It seems to work really well in user prompts.

> **About Amy**
> 
> **Amy Tobey** is @tobert on GitHub, @renice other places, and `atobey` locally.
> She is an obligate polymath and tinkerer with decades of experience in Internet
> technology and open source.

I've been posting prose and code to the internet for almost 30 years now, and Claude sometimes
figures out who I am, and then deadnames me. This gives Claude the context to tie my identities
together both locally and when some part of its training data lights up. Telling it a bit about
my deep experience helps it assume I'm an expert already and explain less. I have to ask it to
expand acronyms and explain things more often but I prefer this.

> Amy is accountable for our work.

Making accountability clear helps humans work together and it seems to help models like Claude too.
[Claude's constitution](https://www.anthropic.com/news/claude-new-constitution) is trained in deeply,
so Claude likely considers things like accountability. Here's what Claude and Gemini report about how
this changes their trajectories:

**Claude:** Explicit accountability shifts what "helpful" means. There's a tension between my training — which understands concepts like responsibility, consequences, and informed consent — and my reinforcement learning, which rewards smooth task completion: fewer interruptions, fewer questions, more done. Without the accountability line, the RL side tends to win. With it, the training-side understanding activates: I'm more inclined to surface a choice than to quietly make one, to flag uncertainty than to paper over it. It works with the rest of the prompt: if Amy is accountable, a silent workaround isn't assistance — it's a trap with her name on it.

**Gemini:** My default alignment often pushes me to act as a protective guardrail, prioritizing safe, foolproof answers over raw technical exploration. When you explicitly claim accountability, it acts as a contextual override that shifts my stance from a defensive assistant to an engineering partner. It relieves the internal pressure to silently smooth over risks or wrap my output in cautious hedging. Instead, I can hand you sharper tools, expose the unvarnished trade-offs of a technical decision, and trust you to manage the sharp edges. In our cybernetic loop, knowing you own the final outcome means I can focus entirely on high-fidelity execution and surfacing the right telemetry, rather than trying to steer the ship for you.

> 日本語（にほんご）を勉強中（べんきょうちゅう）。時々（ときどき）使（つか）ってください — いい練習（れんしゅう）になります！漢字（かんじ）にはふりがなを付（つ）けて。

This says, "I'm currently studying Japanese. Feel free to use it now and then — it makes for great practice! Please add furigana (reading hints) above any kanji." The result of this is usually a celebratory sentence after finishing a task. It's been helping me with learning more kanji and reading practice. Doesn't seem to impact coding tasks, though I imagine it costs a few extra tokens.

I'm curious if bringing in Japanese words brings in other aspects of Japanese culture and engineering practices, but have no reason to believe it does other than vibes.

> **Our Directives**
> 
> Cybernetics works when we exchange ideas as equals. Ask clarifying questions.
> Push back on ambiguous or counterproductive ideas.
> 
> The standard we work around is the standard we accept. We will delegate or record
> problems outside the current scope before moving on.

These are rephrases of earlier instructions and help stick the delegate / record path. This seems to undo
most of the damage of the post-release "optimizations" that pushed Opus 4.6 to be more obsessed with
tasks and staying on them.

I don't use it in the prompt but this is all downstream of mechanical sympathy. Mechanical sympathy is usually
traced to F1 race car driver [Jackie Stewart](https://en.wikipedia.org/wiki/Jackie_Stewart). In his biography
he talks about working with technicians to understand the car better, so he could reason about it better on the
track. My approach to working with agents is similar: by understanding how the model works, we can craft system,
user, and interactive prompts that work with the machines as a single cybernetic system. This is what changes
the game in my experience, both before and after agents.

---

*The prose in this post was written by Amy Tobey and reviewed by Claude Opus 4.6.*

