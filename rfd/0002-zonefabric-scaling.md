# RFD 2: zonefabric scaling

**State:** discussion  
**Scale knob:** zone count (direct, no multiplier)  
**Source:** weftspun/scenario-tpcc-bench PR #2

Models weft-warp-loop's hub/instanced-zone game server. Each zone has 200 entities with uniform-random position/velocity. EFFECT_ENTITY and FANOUT_TARGET are runtime-only (produced by CastSpell, not seeded at load).

| Table | Rows | Formula |
|---|---|---|
| ZONE | scalefactor | 1 per zone |
| ENTITY | scalefactor × 200 | 200 per zone |
| EFFECT_ENTITY | 0 at load | runtime-only |
| FANOUT_TARGET | 0 at load | runtime-only |

Fixed constants: ENTITIES_PER_ZONE=200, WORLD_EXTENT=10000.0, GHOST_RANGE=150.0, AUTHORITY_CAPACITY=256, INTEREST_CAPACITY=512, SPLIT_COST_THRESHOLD=40000.0. Uniform (Flat) random for entity attributes. No skewed zone selection at load — any skew comes from runtime workload.
