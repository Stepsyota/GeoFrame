from datetime import datetime
from pathlib import Path

from app.db.tables_media import MediaType
from app.utils.metadata.models import MediaMetadata

def image_metadata() -> MediaMetadata:
    return MediaMetadata(
        storage_path=Path("/tmp/test.heic"),
        size_bytes=12345,
        created_at_fs=datetime.now(),
        taken_at=None,
        sha256="a" * 64,
        width=1920,
        height=1080,
        gps_lat=52.0,
        gps_lon=13.0,
        altitude=None,
        phash=b"\x00" * 8,
        duration=None,
        media_type=MediaType.image
    )

def video_metadata() -> MediaMetadata:
    return MediaMetadata(
        storage_path=Path("/tmp/test.mov"),
        size_bytes=54321,
        created_at_fs=datetime.now(),
        taken_at=None,
        sha256="b" * 64,
        width=3840,
        height=2160,
        gps_lat=None,
        gps_lon=None,
        altitude=None,
        phash=None,
        duration=13.5,
        media_type=MediaType.video
    )