-- zonefabric schema (PostgreSQL compatible)
-- Models weft-warp-loop's hub/instanced-zone game server.
-- Scale factor = number of zones.
-- See rfd/0002-zonefabric-scaling.md for exact formulas.

-- ZONE: scalefactor rows (1 per zone)
CREATE TABLE IF NOT EXISTS zone (
    z_id              BIGINT PRIMARY KEY,
    z_region          VARCHAR(32),
    z_authority_cap   BIGINT,   -- AUTHORITY_CAPACITY = 256
    z_interest_cap    BIGINT,   -- INTEREST_CAPACITY = 512
    z_cost            DOUBLE PRECISION,  -- population^2, compared against SPLIT_COST_THRESHOLD
    z_population       INTEGER
);

-- ENTITY: scalefactor * 200 rows (ENTITIES_PER_ZONE = 200)
CREATE TABLE IF NOT EXISTS entity (
    e_id      BIGINT PRIMARY KEY,
    e_zone_id BIGINT NOT NULL,
    e_x       DOUBLE PRECISION,  -- [0, WORLD_EXTENT=10000.0]
    e_y       DOUBLE PRECISION,
    e_vx      DOUBLE PRECISION,  -- [-10, 10]
    e_vy      DOUBLE PRECISION,
    e_rtt_ms  INTEGER,           -- [10, 200]
    FOREIGN KEY (e_zone_id) REFERENCES zone(z_id)
);

CREATE INDEX IF NOT EXISTS idx_entity_zone ON entity (e_zone_id);

-- EFFECT_ENTITY: 0 at load, runtime-only (produced by CastSpell)
CREATE TABLE IF NOT EXISTS effect_entity (
    ee_id        BIGINT PRIMARY KEY,
    ee_source_id BIGINT NOT NULL,
    ee_zone_id   BIGINT NOT NULL,
    ee_x         DOUBLE PRECISION,
    ee_y         DOUBLE PRECISION,
    ee_created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- FANOUT_TARGET: 0 at load, runtime-only (produced alongside EFFECT_ENTITY)
CREATE TABLE IF NOT EXISTS fanout_target (
    ft_effect_id BIGINT NOT NULL,
    ft_entity_id BIGINT NOT NULL,
    PRIMARY KEY (ft_effect_id, ft_entity_id)
);
