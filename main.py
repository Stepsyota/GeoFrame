from aiogram import Bot, Dispatcher, types
from aiogram.types import FSInputFile
import os

from aiogram.filters import CommandStart, Command
from dotenv import load_dotenv
import asyncio

load_dotenv()

BOT_TOKEN = os.getenv("BOT_TOKEN")
CHAT_ID = os.getenv("GEORGIA_CHAT_ID")
PHOTOS_DIR = "Photos"
dp = Dispatcher()


@dp.message(CommandStart())
async def start_handler(message : types.Message):
    await message.reply("Hello!")

@dp.message(Command("id"))
async def get_id(message: types.Message):
    chat = message.chat
    await message.reply(
        f"chat_id: {chat.id}\n"
        f"type: {chat.type}\n"
        f"is_forum: {getattr(chat, 'is_forum', None)}"
    )

@dp.message(Command("upload_photos"))
async def upload_photos(message : types.Message):
    bot = message.bot

    photos = get_photos_sorted(PHOTOS_DIR)

    if not photos:
        await message.reply("Папка с фото пуста 😢")
        return

    # 1️⃣ создаём тему
    topic = await bot.create_forum_topic(
        chat_id=CHAT_ID,
        name="📸 Загрузка фото"
    )

    thread_id = topic.message_thread_id

    await message.reply(
        f"Создана тема, загружаю {len(photos)} фото…"
    )

    # 2️⃣ загружаем фото
    for path in photos:
        file = FSInputFile(path)
        await bot.send_document(
            chat_id=CHAT_ID,
            message_thread_id=thread_id,
            document=file
        )

    await message.reply("Готово ✅")


async def main():
    bot = Bot(token=BOT_TOKEN)
    await dp.start_polling(bot)

async def create_topic(bot : Bot,  name : str):
    topic = await bot.create_forum_topic(
        chat_id=CHAT_ID,
        name=name
    )
    return topic.message_thread_id

async def send_message(bot : Bot, message_thread_id, text : str ="First message"):
    await bot.send_message(
        chat_id=CHAT_ID,
        message_thread_id=message_thread_id,
        text=text
    )

def get_photos_sorted(path : str) -> list[str]:
    files = [
        os.path.join(path, f)
        for f in os.listdir(path)
        if f.lower().endswith((".jpg", ".jpeg", ".png", ".heic", ".mov"))
    ]
    return sorted(files, key=os.path.getctime)

if __name__ == "__main__":
    asyncio.run(main())