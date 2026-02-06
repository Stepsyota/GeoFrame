from datetime import datetime
from pathlib import Path
from app.utils.metadata.models import MediaMetadata

def image_metadata() -> MediaMetadata:
    return MediaMetadata(
        path=Path("/tmp/test.jpg"),
        filename="test.jpg",
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
    )

def video_metadata() -> MediaMetadata:
    return MediaMetadata(
        path=Path("/tmp/test.mov"),
        filename="test.mov",
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
    )

def base_metadata() ->MediaMetadata:
    return MediaMetadata(
        path=Path("/tmp/test.abcd"),
        filename="test.abcd",
        size_bytes=54321,
        created_at_fs=datetime.now(),
        taken_at=None,
        sha256="b" * 64,
        width=120,
        height=60,
        gps_lat=None,
        gps_lon=None,
        altitude=None,
        phash=None,
        duration=None,
    )
