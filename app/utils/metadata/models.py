from dataclasses import dataclass
from datetime import datetime
from pathlib import Path


@dataclass
class MediaMetadata:
    # COMMON
    path : Path
    filename : str
    size_bytes : int
    created_at_fs : datetime
    sha256 : str
    width : int
    height : int
    taken_at: datetime | None = None
    gps_lat : float | None = None
    gps_lon : float | None = None
    altitude : float | None = None

    # IMAGE
    phash : bytes | None = None

    # VIDEO
    duration : float | None = None