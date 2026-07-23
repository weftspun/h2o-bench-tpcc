-- assetcdn schema (PostgreSQL compatible)
-- Models zone-backend's 3D asset CDN.
-- Scale factor = number of distinct assets.
-- See rfd/0003-assetcdn-scaling.md for exact formulas.

-- ASSET: scalefactor rows (1:1)
CREATE TABLE IF NOT EXISTS asset (
    a_id          BIGINT PRIMARY KEY,
    a_owner       VARCHAR(64),       -- "owner-{assetId % 500}"
    a_size_bytes  BIGINT,            -- DEFAULT_ASSET_SIZE_BYTES = 5000000
    a_created_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- ASSET_VERSION: scalefactor rows (1 seed version per asset, version=1)
CREATE TABLE IF NOT EXISTS asset_version (
    av_id         BIGINT PRIMARY KEY,
    av_asset_id   BIGINT NOT NULL,
    av_version    INTEGER NOT NULL,  -- always 1 at load
    av_content_hash VARCHAR(64),     -- "hash-{assetId}-v1"
    av_storage_uri  VARCHAR(200),    -- "s3://assets/{assetId}/v1"
    av_size_bytes   BIGINT,
    av_created_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (av_asset_id) REFERENCES asset(a_id)
);

CREATE INDEX IF NOT EXISTS idx_asset_version_asset ON asset_version (av_asset_id);

-- EDGE_CACHE_ENTRY: scalefactor rows (1 per asset, edge node 0 only at load)
-- EDGE_NODE_COUNT = 8 (fixed fleet, populated at runtime not load)
CREATE TABLE IF NOT EXISTS edge_cache_entry (
    ece_id         BIGINT PRIMARY KEY,
    ece_asset_id   BIGINT NOT NULL,
    ece_edge_node  INTEGER NOT NULL,  -- 0 at load, 0-7 at runtime
    ece_size_bytes BIGINT,
    ece_stale_ticks BIGINT DEFAULT 0, -- CACHE_STALE_TICKS = 10000
    ece_cached_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (ece_asset_id) REFERENCES asset(a_id)
);

CREATE INDEX IF NOT EXISTS idx_edge_cache_node ON edge_cache_entry (ece_edge_node, ece_asset_id);

-- USER_ENTITLEMENT: scalefactor * 50 rows (ENTITLEMENTS_PER_ASSET = 50)
CREATE TABLE IF NOT EXISTS user_entitlement (
    ue_id        BIGINT PRIMARY KEY,
    ue_asset_id  BIGINT NOT NULL,
    ue_user_id   BIGINT NOT NULL,    -- assetId * 1000000 + u
    ue_granted_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (ue_asset_id) REFERENCES asset(a_id)
);

CREATE INDEX IF NOT EXISTS idx_entitlement_user ON user_entitlement (ue_user_id);
CREATE INDEX IF NOT EXISTS idx_entitlement_asset ON user_entitlement (ue_asset_id);
