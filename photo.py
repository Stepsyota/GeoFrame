from PIL import Image, ExifTags
from pillow_heif import register_heif_opener
import piexif

class Photo:
    pass

def get_heic_metadata(file_path):
    """Корректное извлечение метаданных из HEIC"""
    try:
        register_heif_opener()

        with Image.open(file_path) as img:
            # Получаем сырые EXIF-данные
            exif_data = img.info.get("exif")

            if not exif_data:
                return {"error": "No EXIF data found"}

            # Разбираем EXIF через piexif
            exif_dict = piexif.load(exif_data)

            # Форматируем метаданные
            metadata = {
                "basic": {
                    "format": img.format,
                    "size": img.size,
                    "mode": img.mode
                },
                "exif": {},
                "gps_raw": exif_dict.get("GPS", {})
            }

            # Обрабатываем доступные EXIF-теги
            for ifd in ["0th", "Exif", "GPS", "1st"]:
                if ifd in exif_dict:
                    for tag, value in exif_dict[ifd].items():
                        tag_name = ExifTags.TAGS.get(tag, tag)
                        try:
                            # Преобразуем bytes в строку где нужно
                            if isinstance(value, bytes):
                                value = value.decode('utf-8', errors='ignore').strip('\x00')
                            metadata["exif"][tag_name] = value
                        except:
                            metadata["exif"][tag_name] = str(value)

            return metadata

    except Exception as e:
        return {"error": str(e)}

def convert_gps(gps_raw):
    if not gps_raw:
        return None

    def to_deg(v):
        return v[0][0] / v[0][1] + v[1][0] / v[1][1] / 60 + v[2][0] / v[2][1] / 3600

    try:
        lat = to_deg(gps_raw[piexif.GPSIFD.GPSLatitude])
        lat_ref = gps_raw.get(piexif.GPSIFD.GPSLatitudeRef, b'N')
        if lat_ref in [b'S', 'S']:
            lat = -lat

        lon = to_deg(gps_raw[piexif.GPSIFD.GPSLongitude])
        lon_ref = gps_raw.get(piexif.GPSIFD.GPSLongitudeRef, b'E')
        if lon_ref in [b'W', 'W']:
            lon = -lon

        return {"latitude": lat, "longitude": lon}
    except:
        return None