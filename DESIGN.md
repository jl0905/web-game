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

## 3. Core loop

```
Hang out in town → pick an activity (gather / craft / dungeon / minigame)
→ earn XP + items → level up / find something rare → sell, trade, or equip
→ show off in town → repeat
```

- **Session length target:** meaningful progress in 15 minutes; depth for
  2-hour sessions.
- **The town is the hub.** All loops start and end in a shared social space.

## 4. The skill web — how systems intertwine

Rule of design: **every skill must consume from ≥2 other skills and supply
≥2 other skills.** When we spec a new skill, we draw its edges in this graph
first; if it doesn't connect, it doesn't ship.

Each row is a launch skill; every cell is a real recipe or dependency, not
flavor.

| Skill | Consumes (from) | Supplies (to) |
|---|---|---|
| **Farming** | Tools (Smithing) · fertilizer from fish scraps (Fishing) · tamed creatures (Combat) | Ingredients, crop quality carries into meals (Cooking) · worm bait from tilled soil (Fishing) · animal feed (internal) |
| **Fishing** | Bait (Farming, Mining) · rods & hooks (Smithing) | Premium ingredients (Cooking) · fertilizer from scraps (Farming) · aquarium trophies (Housing later) |
| **Mining** | Picks & supports (Smithing) · meals & lamp fuel (Cooking, Farming) · escort through deep layers (Combat) | Ore & gems (Smithing; Jewelry later) · grub bait (Fishing) · stone (Construction later) |
| **Combat** | Weapons & armor (Smithing) · buff meals (Cooking) · dungeon keys (crafted) | Tameable creatures (Farming) · hides & essences (Smithing) · monster parts (Cooking) · exclusive deep-layer access (Mining) |
| **Smithing** | Ore & gems (Mining) · hides & essences (Combat) · fuel (Woodcutting later) | Tools (all gatherers) · weapons & armor + affix rerolls (Combat) · hooks & fittings (Fishing) · machine parts (Farming) |
| **Cooking** | Crops (Farming) · fish (Fishing) · monster parts (Combat) · cookware (Smithing) | Dungeon buff meals (Combat) · stamina food (all gatherers) · animal feed (Farming) · fair entries (events) |

Highlights that make the web feel alive rather than mechanical:

- **Quality flows through chains:** a five-star meal needs a five-star crop —
  ingredient quality carries downstream.
- **Combat ⇄ Mining is bidirectional:** deep layers hold hostile fauna, so
  miners bring fighters; fighters want the depths' exclusive materials.
- **Smith affix rerolls** are the single bridge between crafted reliability
  and dungeon-drop variance.
- **Dungeon parties provision like expeditions:** meals, potions, and gear
  from three different specialists go into one serious run.

**The market square** is where these edges become social: direct
player-to-player trade first, a posted-offer market board later.

## 5. Skill depth standard

Every launch skill must ship with all five layers:

1. **An active core verb** — a hands-on interaction with skill expression,
   never click-and-wait.
2. **A discovery space** — collection-book content: species, veins, strains,
   recipes. Rarity tiers with visible odds; long-odds chase items.
3. **A progression tree** — unlocks that change *how* you play, not just
   +5% speed.
4. **Economy edges** — the graph connections above; ≥2 in, ≥2 out.
5. **An endgame** — something masters chase forever: mutations, leaderboards,
   prestige cosmetics, once-per-world events.

Skills level 1–50 at launch (headroom to raise). Levels gate tools, zones,
recipes, and techniques.

---

## 6. Skills

Each skill: game design first, then technical notes.

### 6.1 Combat

**Design.** Real-time bullet-hell combat. Enemies fire readable projectile
patterns — rings, spirals, aimed shots, walls — and the player survives by
moving, not by stats alone. WASD movement, aim-at-cursor primary fire, 1–2
cooldown abilities (dash, bomb, special shot). Bosses are multi-phase
pattern fights with clear telegraphs: learnable, fair, spectacular in a
party.

- **Weapon archetypes as sub-skills:** e.g. blades (short-range arcs),
  bows (long-range pierce), staves (slow AoE). Each levels independently
  with its own technique unlocks; your "combat level" is derived from them.
- **Dungeons:** instanced, party of 1–4, 10–20 minute runs from portals at
  the town's edge, generated from hand-made room templates, tiered by
  difficulty. Higher tiers gated by combat level and crafted keys.
- **Risk banking:** dungeon loot is only banked at run completion or
  checkpoints. Dying forfeits the run's unbanked loot — never your
  character or equipped gear.
- **Loot:** rarity tiers (Common/Uncommon/Rare/Epic) with randomized
  affixes (fire rate, projectile speed, dodge stamina...). Crafted gear is
  the reliable baseline; drops are the variance; smiths reroll affixes.
- **Discovery/endgame:** bestiary with pattern-mastery badges (no-hit
  clears), dungeon speedrun leaderboards, rare boss-variant encounters.
- **Skill expression:** an under-geared expert clears content a well-geared
  novice can't.

**Technical.** The hard system. Hundreds of projectiles on screen require
deterministic simulation: server seeds each pattern; clients replay it
locally; only hits, HP, and loot are server-authoritative. The WASM sim
core runs identically on client and server (anti-cheat + prediction).
Dungeon instances are single-server-authoritative rooms; fixed-timestep
sim (60 Hz) with interpolation for remote players. Pattern definitions are
data (JSON/DSL), not code, so designers iterate without rebuilds.

### 6.2 Fishing

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

### 6.3 Farming

**Design.** Every player can claim a personal plot at the town's edge
(instanced, visitable by invitation). Till, plant, water, fertilize,
harvest across a seasonal cycle. Crops need real husbandry — they wilt and
die if neglected; scarecrows, sprinklers, and greenhouses are earned
infrastructure, not decorations.

- **Genetics:** cross-breed seed strains for quality stars, size, growth
  speed, and cosmetic mutations. Strains are tradeable — a master farmer's
  seed line is a brand. Giant crops for the seasonal town fair.
- **Animals:** pens for chickens → cows → exotic creatures tamed in
  dungeons, each with produce, moods, and care needs.
- **Progression:** hand tools → irrigation → greenhouse (season-breaking)
  → beehives (cross-pollination mechanics) → exotic husbandry.
- **Economy edges:** in — tools (Smithing), fertilizer (Fishing), tames
  (Combat); out — ingredients (Cooking), bait (Fishing), reagents
  (Alchemy later), feed loops internal.
- **Endgame:** perfect-quality strain breeding, fair leaderboards, rare
  mutation collection.

**Technical.** Plots are per-player persistent state that simulates
*offline* — the server advances growth/wilt on a coarse tick (per real
hour), applying weather and care history. Genetics = each seed carries a
small genome (a handful of alleles); breeding is deterministic mixing +
mutation roll, so rare strains are discoverable but reproducible. Plot
instancing keeps town server load flat.

### 6.4 Mining

**Design.** A real underground, not a rock-tapping animation. Below the
mine entrance are procedurally generated cave layers — darker, richer, and
more dangerous with depth. Rock is destructible; you dig your own tunnels,
place torches and supports, and haul ore back out. Encumbrance makes the
trip out part of the game.

- **Danger scales with depth:** cave-ins (unsupported tunnels collapse),
  gas pockets, underground lakes, and hostile cave fauna using the combat
  system — the deepest veins effectively require a mining+combat party.
- **Discovery:** ore types by strata, reading rock faces to follow veins
  (prospecting knowledge that's genuinely learnable), rare geodes and
  fossils, a once-per-world-event meteorite fall.
- **Progression:** pickaxe tiers gate rock hardness; lanterns, support
  engineering, blasting (crafted explosives), ore-sense techniques.
- **Claims:** within a personal claim, your tunnels persist — over months a
  mine becomes *your* mine, and you can invite others in.
- **Economy edges:** in — picks/supports (Smithing), meals & light fuel
  (Cooking/Farming), muscle (Combat); out — ore/gems (Smithing, later
  Jewelry), grubs for bait (Fishing), stone (Housing/Construction later).

**Technical.** The underground is chunked voxel-ish 2D strata (tile grid
per layer), generated deterministically from world seed + layer index, with
player edits stored as deltas. Personal-claim layers are per-player
persisted deltas; shared upper layers are world state. Cave-in simulation
is a local support-propagation check on dig events — cheap, deterministic,
server-authoritative. Fauna reuses the combat sim; a mine layer with
monsters is just a dungeon room with destructible walls.

### 6.5 Smithing (crafting)

**Design.** The economy's forge. Smelt ore into bars, then work bars into
tools, weapons, and armor at the anvil. The active verb: a timing/heat
minigame — keep the workpiece in temperature band, strike accurately;
execution quality carries into item quality (matching farming's quality
stars). Master smiths also **reroll one affix** on dungeon-dropped gear —
the single bridge between crafted reliability and dropped variance —
and engrave maker's marks (a smith's reputation is literally on the item).

- **Economy edges:** in — ore/gems (Mining), fuel, monster hides/essences
  (Combat); out — tools (every gatherer), weapons/armor (Combat), hooks &
  fittings (Fishing), machine parts (Farming infrastructure).
- **Endgame:** legendary recipe discovery, quality-perfect crafts,
  commissioned work economy.

**Technical.** Recipes and quality curves are data-driven. Crafting rolls
resolve server-side from the client's minigame performance summary
(validated bounds). Maker's-mark = crafter ID stamped in item metadata —
also our provenance/anti-dupe trail.

### 6.6 Cooking (crafting)

**Design.** Turns the gatherers' output into the fighters' fuel. Recipe
discovery by experimentation (ingredient combinations reveal log entries),
a station-based active verb (chop/season/heat timing), and meal quality
derived from ingredient quality — a farm-to-table chain where a five-star
meal needs a five-star crop.

- **Meals grant dungeon buffs** (HP, dodge stamina, reload) — cooking is
  how non-combat players matter to combat players. Feasts are shared
  (table placed in town or dungeon lobby, party-wide buff) — a social
  object, not just a consumable.
- **Economy edges:** in — crops (Farming), fish (Fishing), monster parts
  (Combat), cookware (Smithing); out — buffs (Combat), stamina food (all
  gatherers), fair entries, feed (Farming).
- **Endgame:** full recipe log, legendary dishes from once-per-world
  ingredients, town feast events.

**Technical.** Recipe discovery = server-side combination table with
hashed lookups (clients can't datamine undiscovered recipes). Buff system
is shared infrastructure with equipment affixes (same stat-modifier
pipeline). Feast objects are world entities with an interaction radius.

---

## 7. World structure

- **Town (persistent, shared):** market square, crafting stations (furnace,
  anvil, kitchen, workbench), fishing pier, minigame corners, dungeon
  portal plaza.
- **Gathering zones (persistent, shared):** forest, lake, and upper mine
  layers around town. Low danger; some roaming monsters for early combat.
- **Personal instances:** farm plots, mine claims, (later) player houses.
- **Dungeons (instanced, party of 1–4):** tiered, template-generated,
  bullet-hell.

## 8. Progression sketch

| Hours | What's happening |
|---|---|
| 0–1 | Spawn in town; tutorial quest chain touches one loop of each skill; buy a hat. |
| 1–10 | Skills to ~15–20; first dungeon tier; first rare catch/crop/drop; first trade. |
| 10–50 | Specialization emerges (the town smith, the seed-line farmer, the deep miner); dungeon tiers 2–3. |
| 50+ | Mastery (50s), top-tier dungeons, market-making, once-per-world events, housing. |

## 9. Monetization & fairness (placeholder — needs discussion)

- If monetized at all: cosmetics only. No power, no XP boosts, no loot
  boxes. Assume free-to-play with everything earnable in-game for now.

## 10. Art & audio direction (early thoughts)

- Top-down or slightly angled 2D; chunky, readable shapes; friendly and
  slightly whimsical, not grimdark.
- Distinct silhouettes for equipment tiers; rarity readable at a glance.
- Light melodic town music; percussive tension underground and in dungeons.

## 11. Technical direction (high level)

- Browser-first, Canvas rendering, WASM deterministic simulation core
  shared by client and server (already prototyped for movement).
- Authoritative server over WebSockets. Town/gathering tolerate higher
  latency; bullet-hell combat uses seeded-deterministic patterns with
  server-authoritative hits.
- All content (patterns, species tables, recipes, genetics, strata) is
  data-driven — designers iterate without engine changes.
- Persistence tiers: world state (shared zones), per-player state (plots,
  claims, character), instance state (ephemeral dungeon runs).
- Start single-player-with-ghosts if needed; design all state to live
  server-side from day one.

## 12. Scope ladder

1. **M0 — Toy:** one town map, walkable character, chat. (Movement exists.)
2. **M1 — Loop:** mining (upper layers) + smithing + one 5-room bullet-hell
   dungeon (2–3 patterns, one boss). Single player. Proves gather→craft→
   fight→loot.
3. **M2 — Together:** multiplayer town, trading, co-op dungeon.
4. **M3 — Depth:** fishing, farming, cooking with full five-layer depth;
   dungeon tiers; the skill web live end to end.
5. **M4 — Home:** housing, market board, seasonal events, once-per-world
   events.

*(Depth staging option: each skill can launch with layers 1–3 and grow its
endgame layers in seasons, if M3 proves too heavy.)*

## 13. Open questions

1. **Name & theme:** "Emberholm" is a placeholder. What's the world's
   fiction? (It should explain portals, seasons, and the meteorite.)
2. **Death penalty:** current proposal — forfeit the run's unbanked loot,
   keep character and equipped gear. Harsh enough? Too harsh for an
   all-ages audience?
3. **Audience/tone:** all-ages (affects chat moderation scope!) or teen+?
4. **How multiplayer is MVP?** Is single-player M1 acceptable, or is
   "friends in a room" the whole point and must come first?
5. **M1 skill pair:** I picked mining+smithing to prove the loop (shortest
   path to combat gear). Argue for fishing-first (social identity earlier)?
6. **Woodcutting/Construction:** the graph wants wood (fuel, supports,
   housing). Launch skill #7 or fold into mining as "forestry" later?
7. **Perspective:** pure top-down vs. 3/4 view — affects all art forward.
