# Design Document — Working Title: *Emberholm*

**Status:** Draft 2 — for discussion. Nothing here is final.

**Inspirations** (referenced only here; the design below stands on its own):
RuneScape, Diablo, Club Penguin, Realm of the Mad God, WebFishing,
Fisch, Stardew Valley, Minecraft.

## 1. One-liner

A browser-based multiplayer world where a cozy social town sits at the
center of deep, intertwined skill systems — gathering, crafting, and
bullet-hell dungeon combat — all feeding one player-driven economy.

## 2. Pillars

1. **Low-friction hangout first.** Open a browser tab and be in town with
   friends in under 10 seconds. No install, no character wall. The town is
   fun even if you never fight anything.
2. **Progress you can feel.** Numbers go up, and each number unlocks
   something concrete: a new tool, a new technique, a new area, a visible
   cosmetic.
3. **Loot worth talking about.** Rare finds — a mutated fish, a perfect
   crop, a geode, a well-rolled weapon — give players stories and fuel the
   trade economy.
4. **The world is the menu.** Prefer walking to a furnace over opening a
   crafting UI; prefer a market square over an auction-house window.
5. **Every skill is a whole game.** A player who only fishes should feel
   they're playing a great fishing game. Depth per skill over breadth of
   shallow skills.
6. **One economy, one world.** Skills are designed as organs of a single
   body: every skill consumes another skill's outputs and produces inputs
   someone else needs. No system is an island bolted on.

## 3. CURRENT FEATURE TO IMPLEMENT
### Fishing


**Design.** The town's campfire — a chill, social activity with a skill
ceiling. Casting is aimed (direction + power); hooking triggers a tension
minigame: keep the line in a moving sweet-spot band; big fish fight in
phases (runs, dives, jumps).


- **Discovery:** different bodies of water have different fish data drop tables. What bites also depends on bait, weather, time of day, depth, and season. Shiny variants at published long odds (e.g. "1/3000") — visible-odds chase is the endgame hook. Trophy sizes recorded per species.
- **Progression:** rods → lines → lures → boats. Unlocks change the verb:
  deep-sea trawling, ice fishing, night fishing, legendary "sea monster"
  encounters that borrow bullet-hell mechanics (the fish fights *back*).
- **Pricing — length is value:** every catch rolls a length from its
  species' distribution, and a fish's base price scales with its length
  (longer = rarer roll = worth more). Shiny variants apply a ~10x
  multiplier *on top of* the length-scaled price — so a shiny is always
  exciting, but a long shiny is the jackpot. One formula, tunable per
  species: `price = speciesBase × lengthFactor × (shiny ? 10 : 1)`.
- **Economy edges:** in — bait (Farming/Mining), rods/hooks (Smithing);
  out — premium ingredients (Cooking), fertilizer from scraps (Farming),
  aquarium trophies (Housing).
- **Social:** you can fish from any water, and spot quality is unequal —
  so players naturally group toward the good spots and gather to chat and
  fish together.
- **Hotspot dynamics (what keeps the gathering alive):** the optimal spot
  is never static. Hotspots shift with weather, season, and time of day,
  and are telegraphed by visible in-world cues (fish jumping, birds
  circling, ripples). Finding the spot is gameplay; the crowd migrates
  together and the gathering keeps re-forming somewhere new, instead of
  one wiki-solved tile being camped forever.


**Technical.** Fish spawning is a server-side loot-table roll parameterized
by (zone, bait, weather, time, depth); odds tables are data-driven and
hot-reloadable. Length is rolled server-side per catch from a per-species
distribution (heavy right tail, so long fish stay rare); price derives
deterministically from species + length + shiny flag, never stored
independently — no price-tampering surface. The tension minigame runs client-side for feel with server
validation of the catch (input-trace spot checks to deter botting — fishing
is the most botted activity in every MMO). Weather/time are world-global
states the server broadcasts.
