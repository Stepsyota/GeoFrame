CREATE TABLE assets (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    source_path TEXT NOT NULL UNIQUE,
    original_filename TEXT NOT NULL,
    media_type TEXT NOT NULL CHECK (media_type IN ('image', 'video')),
    status TEXT NOT NULL DEFAULT 'active' CHECK (status IN ('active', 'trashed')),
    size_bytes INTEGER NOT NULL CHECK (size_bytes >= 0),
    sha256 TEXT UNIQUE CHECK (sha256 IS NULL OR length(sha256) = 64),
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

CREATE INDEX assets_captured_at_idx ON assets(captured_at);
CREATE INDEX assets_location_idx ON assets(gps_lat, gps_lon);
CREATE INDEX assets_status_idx ON assets(status);
