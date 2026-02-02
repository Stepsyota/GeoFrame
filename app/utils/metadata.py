from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from PIL import Image, ExifTags
import pillow_heif
import ffmpeg
from app.utils.hash import sha256_hash, phash_image

pillow_heif.register_heif_opener()


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

    # IMAGE
    phash : int | None = None

    # VIDEO
    duration : float | None = None

def extract_meta(path : Path) -> MediaMetadata:
    pass

def extract_common_meta(path : Path) -> dict:
    return {
        "path" : path,
        "filename" : path.name,
        "size_bytes" : path.stat().st_size,
        "created_at_fs" : datetime.fromtimestamp(path.stat().st_ctime),
        "sha256" : sha256_hash(path),
    }


def extract_photo_meta(path : Path) -> dict:
    try:
        with Image.open(path) as img:
            width, height = img.size
            phash = phash_image(path)
            # EXIF DATA
            exif = img.getexif()
            taken_at = None
            gps_lat = None
            gps_lon = None
            if exif:
                for k, v in exif.items():
                    print(ExifTags.TAGS[k], v)
                dt = exif.get(306) # Key in exif dict for datetime
                if dt:
                    taken_at = datetime.strptime(dt, "%Y:%m:%d %H:%M:%S")

                gps_info = exif.get(34853) # Key in exif dict for gpsinfo
                if gps_info:
                    gps_lat, gps_lon = gps_to_decimal(gps_info)

            return {
                "width" : width,
                "height": height,
                "taken_at" : taken_at,
                "gps_lat" : gps_lat,
                "gps_lon" : gps_lon,
                "phash" : phash
            }

    except Exception as e:
        print(f"PHOTO META SKIP {path}: {e}")
        return {}

def extract_video_meta(path : Path) -> dict:
    try:
        probe = ffmpeg.probe(path)
        video_stream = next(s for s in probe["streams"] if s["codec_type"] == "video")
        for k, v in video_stream.items():
            print(f"{k} : {v}")

        width = video_stream["width"]
        height = video_stream["height"]
        print(type(video_stream["tags"]["creation_time"]))
        taken_at = video_stream["tags"]["creation_time"]
        gps_lat = None
        gps_lon = None
        duration = video_stream["duration"]
        return {
            "width": width,
            "height": height,
            "taken_at": taken_at,
            "gps_lat": gps_lat,
            "gps_lon": gps_lon,
            "duration": duration
        }

    except Exception as e:
        print(f"VIDEO META SKIP {path}: {e}")
        return {}


def gps_to_decimal(gps_info) -> tuple:
    return None, None




from app.utils.scanner import scan_directory
from app.config import PHOTOS_DIR

files = scan_directory(PHOTOS_DIR)
#print(extract_common_meta(files[1]))
#print(extract_video_meta(files[1]))
# print(extract_common_meta(files[0]))
print(extract_photo_meta(files[0]))



