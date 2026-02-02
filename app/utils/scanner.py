from pathlib import Path
from app.config import SUPPORTED_EXTENSIONS

def scan_directory(path : str) -> list[Path]:
    return [
        p for p in Path(path).iterdir() if p.is_file() and p.suffix.lower() in SUPPORTED_EXTENSIONS
    ]