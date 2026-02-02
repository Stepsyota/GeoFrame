from datetime import datetime
from pathlib import Path
from PIL import Image
from app.utils.hash import phash_image
from app.utils.gps import gps_to_decimal
import pillow_heif

pillow_heif.register_heif_opener()


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
                dt = exif.get(306) # Key in exif dict for datetime
                if dt:
                    taken_at = datetime.strptime(dt, "%Y:%m:%d %H:%M:%S")

                gps_info = exif.get(34853) # Key in exif dict for gps_info
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