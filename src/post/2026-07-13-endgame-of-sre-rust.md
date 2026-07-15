---
id: "2026-07-13-endgame-of-sre-rust"
title: "The Endgame of SRE"
abstract: "The Endgaame of SRE port to Rust from RPGMaker"
tags: ["sre", "rust", "bevy", "srecon"]
pubdate: 2026-07-13T23:00:00Z
autoidx: true
---


A few years ago I wrote a talk for [QCon SF 2022](https://qconsf.com/speakers/amytobey) and
[SREcon23 Americas](https://www.usenix.org/conference/srecon23americas/presentation/tobey)
that sought to weave resilience engineering concepts together with the realities of software engineering. The usual rule
I have for a great talk is to hammer one topic home and have maybe one or two more ideas go along with it. This talk
wanted to touch on dozens of things, and really, that network of things (psychological safety, incidents,
production pressure, communication issues, service level objectives, etc.) is the one big thing I wanted people
to take away. Here's the abstract:

> The containers are deployed and the builds are green. Yaml flows through the system, linted, reviewed, tested, and shipped with ease and regularity. Our intrepid SRE finds themself at a crossroads. The infrastructure is great but teams still struggle to maintain error budgets. These developers need help, and it doesn't seem like anyone else is coming to help them. We embark on a journey, to find out where resilience exists and how to make more. Join Amy on an epic quest into sociotechnical thinking, exploring ways SREs can impact reliability at scale beyond the bits and bytes that got us this far.

Or, more succinctly:

![Bluesky post from Sarah M., @sarahofswords.bsky.social: quote, SRE game: an overlap where Shadowrun meets Stardew Valley. fox decker with an eyepatch is waiting to hackety-hack.](/images/bluesky-sarahofswords-sre-game-2026-07-13.png)

Slides were not going to work for this. I've done several alternate presentation formats in the past, and it felt like
it was time to try an idea I've kicked around for years: what if I present using a JRPG? I picked up
[RPGMaker MZ](https://store.steampowered.com/app/1096900/RPG_Maker_MZ/)
to do a proof of concept along with the lovely [VisuStella MZ Sample Game Project](https://visustella.itch.io/visumz-sample)
to get started. RPGMaker MZ is amazing for prototyping, so I had something workable within an hour. The SREcon
talk is on [YouTube](https://www.youtube.com/watch?v=BEs6j-BOl20) if you want to see how I presented it.

Life happened and I got distracted and never packaged the game in a way that I could share. I was really
enjoying Claude Opus 4.6 in late 2025, and had it start on a port of the game to Rust and [Bevy](https://bevy.org).
I had Claude try to do the port fairly hands off, and got some sprites on screen,
but progress was slow so I set it aside for a while. More recently, I've been using Fable 5 more
and decided to have it try to do better, and rapidly got it to a more recognizable but glitched state, then
chipped away at that with Fable doing big hunks of work largely on its own. Once it was running well
on Linux I sent another Fable off to get it compiling to wasm and loading in a web page so I could do
this post. I checked in on it a few times as it worked but didn't have to interrupt or aid it until
the final check.

After a couple days of iteration, I am pleased to present The Endgame of SRE, the Rust rewrite. If your
browser supports it, you'll see the game below here.

`Click into it` so it gets your keyboard, then walk around using `arrow keys`, and the `e` key to interact
with NPCs and the items in the inn. `escape` will get you out of a conversation. The final retrospective
is an interaction with the table just inside the inn door. The NPCs there will repeat their prior
lines as they are just copies.

<div style="position: relative; width: 100%; aspect-ratio: 16 / 9; margin: 1em 0;">
<iframe src="/sregame/" title="The Endgame of SRE"
        style="position: absolute; inset: 0; width: 100%; height: 100%; border: 2px solid #444; border-radius: 4px;"
        allow="fullscreen" loading="lazy"></iframe>
</div>

You may also [play it fullscreen](/sregame/).

Enjoy :)

## Links

- [github.com/tobert/sregame](https://github.com/tobert/sregame)
- [bevy_ecs_tilemap](https://github.com/StarArawn/bevy_ecs_tilemap), `atlas` mode on web (WebGL2 has no texture arrays)
- Python converter ports RPGMaker's own autotile-decode algorithm; maps baked to plain PNG atlases at build time
- map data embedded at compile time (no filesystem in browsers); ~8MB gzipped wasm + pixel art
- art: Visustella Fantasy Tiles MZ, licensed for reuse

## Screenshots

![Overhead pixel-art view of the Town of Endgame: Amy's player character stands at the edge of a terracotta-brick plaza ringed by stone paths, lamp posts, flowering trees, and a lily pond. A wild-haired NPC waits on the path nearby, and a blue-roofed brick storefront anchors the east side of the square.](/images/sregame-town-plaza-2026-07-13.png)

![The town inn, a broad orange-roofed building with a wooden sign reading INN hanging over its door. A black-and-tan doggo stands in the grass out front, and stone paths wind past a well and picket fences toward the plaza.](/images/sregame-inn-sign-2026-07-13.png)

![A dialogue box covers the lower third of the screen at presentation scale: a large portrait of Paws Alljohn, a wizened cyberpunk character with white hair and a gold-armored visor, beside his line: Haha not really. These folks have the best SREs and infrastructure money can buy, and they still have incidents! Behind the box, the town square with NPCs on the flowered paths.](/images/sregame-paws-alljohn-sres-2026-07-13.png)

![Amy's character stands outside Team Disconnect's brick storefront beside a wolf-headed NPC in blue armor. A wooden plaque with a coin purse hangs on the blue-shingled roof above the heavy wooden door, with pumpkin and lily planters flanking the entrance and an autumn orchard rising behind.](/images/sregame-disconnect-storefront-2026-07-13.png)

![Inside Team Disconnect: two wood-paneled shop rooms with bookshelves, a crystal ball on a red-draped table, and boots and armor on display shelves. Amy stands face to face with Managear Greg, an NPC in a goggled aviator helmet, in the central hallway. His dialogue box reads: My people work so hard and it feels like we just cant win. I really hope this retro will help.](/images/sregame-managear-greg-2026-07-13.png)


