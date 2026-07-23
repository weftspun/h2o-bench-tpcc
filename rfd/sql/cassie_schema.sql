-- cassie schema (PostgreSQL compatible)
-- Models collaborative sketch sessions: stroke append, beautify, undo.
-- Scale factor = number of concurrent canvases.
-- See rfd/0004-cassie-scaling.md for exact formulas.
-- Note: upstream PR self-describes as "a reasonable strawman, not verified-accurate."

-- CANVAS: scalefactor rows (1 per canvas)
CREATE TABLE IF NOT EXISTS canvas (
    c_id        BIGINT PRIMARY KEY,
    c_owner     VARCHAR(64),           -- "owner-{canvasId % 500}"
    c_extent    DOUBLE PRECISION,      -- CANVAS_EXTENT = 4096.0
    c_created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- CANVAS_SUBSCRIBER: scalefactor * 10 rows (SUBSCRIBERS_PER_CANVAS = 10)
CREATE TABLE IF NOT EXISTS canvas_subscriber (
    cs_id         BIGINT PRIMARY KEY,
    cs_canvas_id  BIGINT NOT NULL,
    cs_user_id    BIGINT NOT NULL,     -- canvasId * 1000000 + s
    cs_subscribed_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (cs_canvas_id) REFERENCES canvas(c_id)
);

CREATE INDEX IF NOT EXISTS idx_subscriber_canvas ON canvas_subscriber (cs_canvas_id);
CREATE INDEX IF NOT EXISTS idx_subscriber_user ON canvas_subscriber (cs_user_id);

-- STROKE: scalefactor * 5 seed rows (STROKES_PER_CANVAS_SEED = 5)
-- Also grows at runtime via AppendStrokePoints (30 points per appended stroke)
CREATE TABLE IF NOT EXISTS stroke (
    s_id         BIGINT PRIMARY KEY,
    s_canvas_id  BIGINT NOT NULL,
    s_seq        INTEGER NOT NULL,    -- stroke sequence within canvas
    s_point_count INTEGER NOT NULL,  -- 20 for seed, 30 for appended
    s_created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    s_beautified  BOOLEAN DEFAULT FALSE,
    FOREIGN KEY (s_canvas_id) REFERENCES canvas(c_id)
);

CREATE INDEX IF NOT EXISTS idx_stroke_canvas ON stroke (s_canvas_id, s_seq);

-- STROKE_POINT: scalefactor * 100 seed rows (5 strokes * 20 points)
-- x = (p * 13) % CANVAS_EXTENT, y = (p * 29) % CANVAS_EXTENT
CREATE TABLE IF NOT EXISTS stroke_point (
    sp_id        BIGINT PRIMARY KEY,
    sp_stroke_id BIGINT NOT NULL,
    sp_seq       INTEGER NOT NULL,    -- point index within stroke
    sp_x         DOUBLE PRECISION,   -- (p * 13) % 4096.0
    sp_y         DOUBLE PRECISION,   -- (p * 29) % 4096.0
    FOREIGN KEY (sp_stroke_id) REFERENCES stroke(s_id)
);

CREATE INDEX IF NOT EXISTS idx_point_stroke ON stroke_point (sp_stroke_id, sp_seq);

-- BEAUTIFIED_STROKE: 0 at load, runtime-only (produced by BeautifyStroke)
CREATE TABLE IF NOT EXISTS beautified_stroke (
    bs_id              BIGINT PRIMARY KEY,
    bs_original_stroke_id BIGINT NOT NULL,
    bs_canvas_id       BIGINT NOT NULL,
    bs_point_count     INTEGER NOT NULL,
    bs_created_at      TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (bs_original_stroke_id) REFERENCES stroke(s_id),
    FOREIGN KEY (bs_canvas_id) REFERENCES canvas(c_id)
);
