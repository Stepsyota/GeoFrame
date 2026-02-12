from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from app.db.tables_media import MediaType


@dataclass
class MediaMetadata:
    # COMMON
    storage_path : Path
    size_bytes : int
    created_at_fs : datetime
    sha256 : str
    width : int
    height : int
    media_type : MediaType
    taken_at: datetime | None = None
    gps_lat : float | None = None
    gps_lon : float | None = None
    altitude : float | None = None

    # IMAGE
    phash : bytes | None = None

    # VIDEO
    duration : float | None = None