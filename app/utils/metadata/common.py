from datetime import datetime
from pathlib import Path
from app.utils.hash import sha256_hash


def extract_common_meta(path : Path) -> dict:
    return {
        "path" : path,
        "filename" : path.name,
        "size_bytes" : path.stat().st_size,
        "created_at_fs" : datetime.fromtimestamp(path.stat().st_ctime),
        "sha256" : sha256_hash(path),
    }