import hashlib
from pathlib import Path
from PIL import Image
import pillow_heif
import imagehash
from app.config import SUPPORTED_EXTENSIONS_IMAGE

pillow_heif.register_heif_opener()


def sha256_hash(path : Path, algo="sha256") -> str:
    suffix = path.suffix.lower()
    chunk_size = 8192 if suffix in SUPPORTED_EXTENSIONS_IMAGE else 65536
    h = hashlib.new(algo)
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(chunk_size), b""):
            h.update(chunk)
    return h.hexdigest()

def phash_image(path : Path) -> int | None:
    try:
        with Image.open(path) as img:
            h = imagehash.phash(img)
            return int(str(h), 16)
    except Exception as e:
        print(f"PHASH SKIP {path}: {e}")
        return None