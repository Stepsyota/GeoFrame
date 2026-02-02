from os import getenv
from dotenv import load_dotenv


load_dotenv()

BOT_TOKEN = getenv("BOT_TOKEN")
CHAT_ID = getenv("CHAT_ID")
PHOTOS_DIR = "/home/stepsyota/Projects/GeoFrame/Photos"

SUPPORTED_EXTENSIONS_IMAGE = (".jpg", ".jpeg", ".png", ".heic")
SUPPORTED_EXTENSIONS_VIDEO = (".mov", )
SUPPORTED_EXTENSIONS = SUPPORTED_EXTENSIONS_IMAGE + SUPPORTED_EXTENSIONS_VIDEO

DATABASE_URL = (
    f"postgresql+asyncpg://"
    f"{getenv('POSTGRESQL_USER')}:"
    f"{getenv('POSTGRESQL_PASSWORD')}@"
    f"{getenv('POSTGRESQL_HOST')}:"
    f"{getenv('POSTGRESQL_PORT')}/"
    f"{getenv('POSTGRESQL_DB')}"
)