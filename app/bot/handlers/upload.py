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


@router.message(Command("topic"))
async def create_topic_cmd(message : types.Message):
    args = message.text.split(maxsplit=2)

    if len(args) < 3:
        await message.reply("Use: /topic <kind> <name>")
        return

    kind_text = args[1]
    name = args[2]

    if kind_text not in TopicKind.__members__:
        await message.reply(f"Unknown kind. You can use: {', '.join(TopicKind.__members__.keys())}")

    kind = TopicKind[kind_text]
    topic = await get_or_create_topic(bot=message.bot, chat_id=message.chat.id, kind=kind, name=name)
    await message.reply(f"Topic create/get: ID={topic.topic_id}, Name={topic.name}")