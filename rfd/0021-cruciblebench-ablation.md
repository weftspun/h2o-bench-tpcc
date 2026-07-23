# RFD 21: CrucibleBench MUD ablation with taskweft RECTGTN planner

**State:** discussion

## Decision

Model the CrucibleBench MUD as a RECTGTN (Relationship-Enabled
Capability-Temporal Goal-Task-Network) planning domain. Use the
taskweft HTN planner to find the minimal-cost plan for shipping a
human-playable MUD (no runtime LLM) at $10/month hosting cost. The
planner's alternative selection naturally performs feature ablation:
it picks the first alternative whose preconditions are satisfiable,
deferring all features in later alternatives.

## Context

[CrucibleBench](https://github.com/CrucibleBench/CrucibleBench_Phase1)
is a proof-of-concept evaluation environment placing 13 LLMs in a
single-player MUD-style persistent text world. The paper (Zenodo
21386663) scored 650 runs at $99.59 in retained evaluation calls. The
central finding: model rankings are highly sensitive to LLM-classifier-
dependent score components.

The user wants to build a **human-playable** version without the runtime
LLM — a medieval town game where players either gain the Watch's trust
or identify which NPC is secretly aligned with an antagonist faction.
Target: $10/month hosting.

## The ablation question

13 features could be built. Building all costs $116 in engineering
effort (normalized units). Which can be deferred to preserve unspent
dollars and avoid speculator debt?

## RECTGTN domain model

The domain is saved as raw JSON-LD at:
`rfd/0021-cruciblebench-ablation-domain.jsonld`

### Variables

| Variable | Type | Description |
|---|---|---|
| world_state | ref | 13 feature flags: rooms, exits, items, npcs, dialogue, trust, factions, secret_identity, scoring, persistence, web_ui, multiplayer, llm_judge |
| playable | bool | True when the world can be entered and navigated |
| winnable | bool | True when the game has a win condition |
| persistent | bool | True when state survives between sessions |
| multiplayer_ready | bool | True when multiple players can play simultaneously |
| cost | float | Running total of engineering cost (normalized $) |
| speculator_debt | bool | True when a feature was built wrong and must be rebuilt |

### Actions (16)

Each build action has a duration (ISO 8601) and a cost (added to
`cost/total`). Actions use ReBAC capability checks to ensure the
builder agent has the `build_feature` or `verify_invariant` capability.

| Action | Duration | Cost | Effect |
|---|---|---|---|
| build_rooms | PT8H | $8 | world_state.rooms = built |
| build_exits | PT4H | $4 | world_state.exits = built |
| build_items | PT6H | $6 | world_state.items = built |
| build_npcs | PT8H | $8 | world_state.npcs = built |
| build_dialogue | PT12H | $12 | world_state.dialogue = built |
| build_trust | PT8H | $8 | world_state.trust = built |
| build_factions | PT6H | $6 | world_state.factions = built |
| build_secret_identity | PT10H | $10 | world_state.secret_identity = built |
| build_scoring | PT4H | $4 | world_state.scoring = built |
| build_persistence | PT6H | $6 | world_state.persistence = built, persistent = true |
| build_web_ui | PT8H | $8 | world_state.web_ui = built |
| build_multiplayer | PT16H | $16 | world_state.multiplayer = built, multiplayer_ready = true |
| build_llm_judge | PT20H | $20 | world_state.llm_judge = built |
| mark_playable | PT1H | $0 | playable = true (verification step) |
| mark_winnable | PT1H | $0 | winnable = true (verification step) |

### Methods (3, with alternatives)

**establish_playable_world** (2 alternatives):
1. `minimal_playable`: rooms + exits + npcs + dialogue + web_ui → playable
2. `playable_with_items`: rooms + exits + **items** + npcs + dialogue + web_ui → playable

**establish_winnable_game** (2 alternatives):
1. `winnable_without_scoring`: playable + trust + factions + secret_identity → winnable
2. `core_game_loop`: playable + trust + factions + secret_identity + **scoring** → winnable

**ship_mud_game** (4 alternatives):
1. `mvp_single_player_ephemeral`: winnable (no persistence, no multiplayer, no LLM)
2. `mvp_single_player_persistent`: winnable + persistence
3. `full_multiplayer_persistent`: winnable + persistence + multiplayer
4. `full_with_llm_judge`: winnable + persistence + llm_judge

### Planner behavior

The HTN planner picks the **first alternative** at each method
decomposition. Since alternatives are ordered from minimal to maximal,
the planner naturally selects the minimum viable feature set. Features
in later alternatives are deferred — their actions never execute.

## Planned trace (minimal viable)

```
ship_mud_game
└─ mvp_single_player_ephemeral (alternative 1)
   └─ establish_winnable_game
      └─ winnable_without_scoring (alternative 1)
         └─ establish_playable_world
            └─ minimal_playable (alternative 1)
               ├─ build_rooms        (8h, $8)
               ├─ build_exits        (4h, $4)
               ├─ build_npcs         (8h, $8)
               ├─ build_dialogue    (12h, $12)
               ├─ build_web_ui       (8h, $8)
               └─ mark_playable      (1h)
         ├─ build_trust              (8h, $8)
         ├─ build_factions           (6h, $6)
         ├─ build_secret_identity   (10h, $10)
         └─ mark_winnable            (1h)

Total: 74h, $64
```

## Ablation results

### Features DEFERRED (unspent dollars preserved)

| Feature | Cost | Why deferrable | Speculator debt risk |
|---|---|---|---|
| llm_judge | $20 | Human judges replace LLM judges. No invariant depends on automated judging. | None — adding LLM judging later doesn't change game logic |
| multiplayer | $16 | Single-player is the MVP. The game is a solo deduction puzzle. | None — multiplayer is additive, not structural |
| persistence | $6 | Ephemeral sessions work for proof-of-concept. State resets on refresh. | Low — if added later, need to design data model carefully |
| items | $6 | Not needed for core trust/faction gameplay loop. NPCs + dialogue suffice. | None — items are additive content |
| scoring | $4 | Win condition can be manual ("did you identify the traitor?"). No automated scoring needed. | Low — if added later, scoring retrofits onto existing trust/faction state |

**Total unspent: $52 of $116 (45% saved)**

### Features LOAD-BEARING (must build correctly first time)

| Feature | Cost | Why load-bearing | Speculator debt risk |
|---|---|---|---|
| rooms | $8 | Everything depends on rooms existing. No rooms = no game. | High — rebuilding rooms means rebuilding exits, NPCs, dialogue |
| exits | $4 | Navigation requires rooms + exits. Can't move without exits. | High — exits define the world topology |
| npcs | $8 | The game IS about NPC interaction. No NPCs = no trust, no factions. | High — NPCs are the core game objects |
| dialogue | $12 | Trust and faction mechanics require dialogue trees. | High — dialogue is the interface to trust/faction/identity |
| trust | $8 | Core win condition: "gain the Watch's trust." | High — trust is the primary game mechanic |
| factions | $6 | Core win condition: "identify which NPC is secretly aligned." | High — factions define the deduction puzzle |
| secret_identity | $10 | The entire point of the game. | Critical — building this wrong means the game has no purpose |
| web_ui | $8 | Without a UI, no human can play. | Medium — UI can be rebuilt, but the contract with the backend must be stable |

**Total load-bearing: $64 (55% of total)**

## Temporal analysis

The planner produces a temporal schedule (STN consistency check) using
the ISO 8601 durations:

| Phase | Duration | Cumulative |
|---|---|---|
| Build playable world | 41h | 41h |
| Build win conditions | 24h | 65h |
| Verification | 2h | 67h |
| (persistence, if added) | +6h | 73h |

At 4h/day part-time, the MVP ships in **17 days** (~2.5 weeks).

## $10/month hosting cost analysis

The MVP (ephemeral, single-player) needs:
- Static web hosting (GitHub Pages, Netlify free tier): $0
- No server-side state (ephemeral = localStorage): $0
- No database: $0
- Domain (optional): $10/year = $0.83/month

**Actual cost: ~$1/month.** Well within the $10/month target.

Adding persistence ($6 engineering) requires server-side storage:
- SQLite on a $5 DigitalOcean droplet: $5/month
- Total: $6/month — still within budget.

Adding multiplayer ($16 engineering) requires WebSocket server:
- $5 droplet with Node.js WebSocket: $5/month
- Total: $6/month — still within budget.

Adding LLM judge ($20 engineering) requires API calls:
- 650 runs × $0.15/run = $97.50 (CrucibleBench's actual cost)
- This **exceeds** $10/month by 10x. Confirmed deferrable.

## Relationship to RFD 0019 (feature ablation)

RFD 0019 uses plausible-witness-dag for invariant-based ablation on
the zonefabric backend. This RFD uses the taskweft HTN planner for
alternative-based ablation on the CrucibleBench frontend.

Both approaches answer the same question — "which features can be
deferred?" — but from different angles:

| Approach | Tool | Method | Granularity |
|---|---|---|---|
| Invariant ablation (RFD 0019) | plausible-witness-dag | Remove invariants, search for violations | Per-feature invariant |
| HTN ablation (this RFD) | taskweft planner | Order alternatives, pick first satisfiable | Per-feature alternative |

The HTN approach is faster (no witness search needed) but less
rigorous (it assumes the alternative ordering is correct). The
invariant approach is slower but catches cases where the ordering
is wrong (a feature marked deferrable actually breaks an invariant).

For the MUD (13 features, simple dependencies), HTN ablation suffices.
For zonefabric (20+ features, complex interactions), invariant
ablation is needed.

## Raw JSON-LD domain

The complete RECTGTN domain model is saved as a separate file:
`rfd/0021-cruciblebench-ablation-domain.jsonld`

It can be loaded into the taskweft planner via:
```
mcp__tasksweft__plan(domain_json=<parsed JSON-LD object>, explain=true)
```

The domain includes:
- 7 state variables (world_state, playable, winnable, persistent, multiplayer_ready, cost, speculator_debt)
- 16 actions with ISO 8601 durations and ReBAC capability checks
- 3 methods with 8 total alternatives (the ablation candidates)
- 1 capability entity (builder) with 2 capabilities (build_feature, verify_invariant)
- 3 ReBAC graph edges for capability enforcement
