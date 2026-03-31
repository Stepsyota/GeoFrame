from aiogram import Router, types
from aiogram.filters import Command
from aiogram.types import FSInputFile
import asyncio

from app.bot.middlewares.uploader import get_or_create_topic
from app.config import CHAT_ID, PHOTOS_DIR
from app.db.tables_telegram import TopicKind

router = Router()


@router.message(Command("upload_photos"))
async def upload_photos(message : types.Message):
    pass