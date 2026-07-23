# RFD 4: cassie scaling

**State:** discussion  
**Scale knob:** concurrent canvases (direct)  
**Source:** weftspun/scenario-tpcc-bench PR #10 (PostgreSQL-only, self-described strawman)

Models collaborative sketch sessions: stroke append, beautify, undo. BEAUTIFIED_STROKE is runtime-only (produced by BeautifyStroke). All loader values are deterministic modular arithmetic — no RNG distribution class at all.

| Table | Rows | Formula |
|---|---|---|
| CANVAS | scalefactor | 1 per canvas |
| CANVAS_SUBSCRIBER | scalefactor × 10 | 10 per canvas |
| STROKE (seed) | scalefactor × 5 | 5 per canvas |
| STROKE_POINT | scalefactor × 100 | 20 points × 5 strokes |
| BEAUTIFIED_STROKE | 0 at load | runtime-only |

Fixed: STROKES_PER_CANVAS_SEED=5, POINTS_PER_SEED_STROKE=20, SUBSCRIBERS_PER_CANVAS=10, POINTS_PER_APPENDED_STROKE=30 (runtime), CANVAS_EXTENT=4096.0. Explicitly the least-verified scenario — carry the "strawman" caveat forward.
