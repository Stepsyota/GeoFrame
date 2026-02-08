from aiogram import Router, types
from aiogram.filters import Command, CommandStart


router = Router()

@router.message(CommandStart())
async def start_handler(message : types.Message):
    await message.reply("Hello!")

@router.message(Command("id"))
async def get_id(message: types.Message):
    chat = message.chat
    await message.reply(
        f"chat_id: {chat.id}\n"
        f"type: {chat.type}\n"
        f"is_forum: {getattr(chat, 'is_forum', None)}"
    )