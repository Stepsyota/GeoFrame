from datetime import datetime
from pathlib import Path
import ffmpeg


def extract_video_meta(path : Path) -> dict:
    try:
        probe = ffmpeg.probe(path)
        video_stream = next(s for s in probe["streams"] if s["codec_type"] == "video")

        width = video_stream["width"]
        height = video_stream["height"]
        taken_at = datetime.strptime(video_stream["tags"]["creation_time"], "%Y-%m-%dT%H:%M:%S.%fZ")
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