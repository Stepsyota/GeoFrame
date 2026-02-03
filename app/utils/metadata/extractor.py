from pathlib import Path
from app.utils.metadata.models import MediaMetadata
from app.utils.metadata.common import extract_common_meta
from app.utils.metadata.image import extract_photo_meta
from app.utils.metadata.video import extract_video_meta
from app.utils.file_type import is_image, is_video


def extract_meta(path : Path) -> MediaMetadata:
    common = extract_common_meta(path)

    if is_image(path):
        photo = extract_photo_meta(path)
        return MediaMetadata(**common, **photo)

    if is_video(path):
        video = extract_video_meta(path)
        return MediaMetadata(**common, **video)

    raise Exception(f"Unsupported media format: {path}")