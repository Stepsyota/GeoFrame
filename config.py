from dotenv import load_dotenv
from os import getenv

load_dotenv()

TOKEN = getenv("BOT_TOKEN")

if not TOKEN or TOKEN == "":
    raise ValueError("Token was not found")