# RFD 4: cassie scaling

**State:** discussion  
**Scale knob:** canvas count (direct, no multiplier)  
**Source:** collaborative sketch sessions

Models collaborative sketch sessions: stroke append, beautify, undo.
Each canvas has 10 subscribers, 5 seed strokes (20 points each),
and 100 seed stroke points. Runtime adds appended strokes (30 points)
and beautified strokes.

## Row counts

| Table | Rows | Formula |
|-------|------|---------|
| CANVAS | scalefactor | 1 per canvas |
| CANVAS_SUBSCRIBER | scalefactor × 10 | 10 per canvas |
| STROKE | scalefactor × 5 | 5 seed strokes per canvas |
| STROKE_POINT | scalefactor × 100 | 20 points × 5 strokes |
| BEAUTIFIED_STROKE | 0 at load | runtime-only |

Fixed constants: CANVAS_EXTENT=4096.0, SUBSCRIBERS_PER_CANVAS=10,
STROKES_PER_CANVAS_SEED=5, POINTS_PER_SEED_STROKE=20,
POINTS_PER_APPENDED_STROKE=30.

## Runtime operations

- **AppendStrokePoints**: insert stroke (30 points) + 30 stroke_point
  rows. Fan-out to 10 subscribers.
- **BeautifyStroke**: read stroke + points, insert beautified_stroke.
- **UndoStroke**: delete stroke + points (or tombstone).

## Comparison baseline

Excalidraw / tldraw collaborative editing benchmarks. Stroke append
is a range write — comparable to FDB batch commit throughput
(35,000 writes/sec/core on memory engine). Subscriber fan-out is
N reads where N = subscriber count.
