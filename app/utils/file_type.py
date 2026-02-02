from pathlib import Path
from app.config import SUPPORTED_EXTENSIONS_IMAGE, SUPPORTED_EXTENSIONS_VIDEO


def is_image(path : Path):
    if path.suffix.lower() in SUPPORTED_EXTENSIONS_IMAGE:
        return True
    return False

def is_video(path : Path):
    if path.suffix.lower() in SUPPORTED_EXTENSIONS_VIDEO:
        return True
    return False