# RFD 3: assetcdn scaling

**State:** discussion  
**Scale knob:** asset count (direct, no multiplier)  
**Source:** zone-backend's 3D asset CDN

Models zone-backend's 3D asset CDN. Each asset has one seed version,
one edge cache entry, and 50 user entitlements. Edge cache entries are
seeded at edge node 0 only; runtime populates nodes 0-7.

## Row counts

| Table | Rows | Formula |
|-------|------|---------|
| ASSET | scalefactor | 1 per asset |
| ASSET_VERSION | scalefactor | 1 seed version per asset |
| EDGE_CACHE_ENTRY | scalefactor | 1 per asset (node 0 at load) |
| USER_ENTITLEMENT | scalefactor × 50 | 50 per asset |

Fixed constants: DEFAULT_ASSET_SIZE_BYTES=5000000, EDGE_NODE_COUNT=8,
CACHE_STALE_TICKS=10000.

## Runtime operations

- **CacheLookup**: read edge_cache_entry by (asset_id, edge_node).
  If stale or missing, read asset_version, write new edge_cache_entry.
- **EntitlementCheck**: read user_entitlement by (user_id, asset_id).
- **AssetUpload**: insert asset + asset_version + user_entitlement.

## Comparison baseline

CloudFront / Cloudflare cache hit ratio benchmarks. CDN edge cache
lookup is a single key read — comparable to FDB point read throughput
(90,000 reads/sec/core on memory engine).
