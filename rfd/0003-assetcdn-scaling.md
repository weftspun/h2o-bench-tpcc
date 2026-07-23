# RFD 3: assetcdn scaling

**State:** discussion  
**Scale knob:** distinct assets (direct)  
**Source:** weftspun/scenario-tpcc-bench PR #9 (PostgreSQL-only upstream)

Models zone-backend's 3D asset CDN. Scalar metadata only — no BLOB/vector columns. EDGE_CACHE_ENTRY seeds node 0 only at load; the 8-node fleet gets populated at runtime.

| Table | Rows | Formula |
|---|---|---|
| ASSET | scalefactor | 1:1 |
| ASSET_VERSION | scalefactor | 1 seed version per asset |
| EDGE_CACHE_ENTRY | scalefactor | 1 per asset, node 0 only |
| USER_ENTITLEMENT | scalefactor × 50 | 50 per asset |

Fixed: EDGE_NODE_COUNT=8, ENTITLEMENTS_PER_ASSET=50, DEFAULT_ASSET_SIZE_BYTES=5000000. Transaction mix: AssetFetch 40%, EntitlementCheck 30%, AssetUpload 15%, EdgeSyncStatus 12%, CacheEviction 3%. All loader values deterministic (no RNG).
