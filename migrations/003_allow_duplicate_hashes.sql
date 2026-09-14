CREATE TABLE assets_new (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    source_path TEXT NOT NULL UNIQUE,
    original_filename TEXT NOT NULL,
    media_type TEXT NOT NULL CHECK (media_type IN ('image', 'video')),
    status TEXT NOT NULL DEFAULT 'active' CHECK (status IN ('active', 'trashed')),
    size_bytes INTEGER NOT NULL CHECK (size_bytes >= 0),
    sha256 TEXT CHECK (sha256 IS NULL OR length(sha256) = 64),
    captured_at TEXT,
    width INTEGER CHECK (width IS NULL OR width > 0),
    height INTEGER CHECK (height IS NULL OR height > 0),
    gps_lat REAL CHECK (gps_lat IS NULL OR gps_lat BETWEEN -90.0 AND 90.0),
    gps_lon REAL CHECK (gps_lon IS NULL OR gps_lon BETWEEN -180.0 AND 180.0),
    altitude REAL,
    camera TEXT,
    favorite INTEGER NOT NULL DEFAULT 0 CHECK (favorite IN (0, 1)),
    indexed_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO assets_new
SELECT id, source_path, original_filename, media_type, status, size_bytes, sha256,
       captured_at, width, height, gps_lat, gps_lon, altitude, camera, favorite, indexed_at
FROM assets;

CREATE TABLE jobs_new (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    asset_id INTEGER NOT NULL REFERENCES assets_new(id) ON DELETE CASCADE,
    type TEXT NOT NULL CHECK (
        type IN ('hash', 'metadata', 'thumbnail', 'preview', 'phash', 'series_detect')
    ),
    status TEXT NOT NULL DEFAULT 'pending' CHECK (
        status IN ('pending', 'processing', 'done', 'failed')
    ),
    attempts INTEGER NOT NULL DEFAULT 0 CHECK (attempts >= 0),
    error TEXT,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    started_at TEXT,
    completed_at TEXT,
    UNIQUE (asset_id, type)
);

INSERT INTO jobs_new
SELECT id, asset_id, type, status, attempts, error, created_at, started_at, completed_at
FROM jobs;

DROP TABLE jobs;
DROP TABLE assets;
ALTER TABLE assets_new RENAME TO assets;
ALTER TABLE jobs_new RENAME TO jobs;

CREATE INDEX assets_sha256_idx ON assets(sha256);
CREATE INDEX assets_captured_at_idx ON assets(captured_at);
CREATE INDEX assets_location_idx ON assets(gps_lat, gps_lon);
CREATE INDEX assets_status_idx ON assets(status);
CREATE INDEX jobs_status_id_idx ON jobs(status, id);
