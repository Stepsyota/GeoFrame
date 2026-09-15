CREATE TABLE live_photos (
    id               INTEGER PRIMARY KEY AUTOINCREMENT,
    image_asset_id   INTEGER NOT NULL REFERENCES assets(id),
    video_asset_id   INTEGER NOT NULL REFERENCES assets(id),
    UNIQUE(image_asset_id),
    UNIQUE(video_asset_id)
);
