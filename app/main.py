from aiogram import Bot, Dispatcher
import asyncio
from app.config import BOT_TOKEN
from app.bot.handlers.common import router as common_router
from app.bot.handlers.upload import router as upload_router
from app.bot.handlers.create_topic import router as create_topic_router


dp = Dispatcher()
dp.include_router(common_router)
dp.include_router(upload_router)
dp.include_router(create_topic_router)

async def main():
    bot = Bot(BOT_TOKEN)
    await dp.start_polling(bot)

if __name__ == "__main__":
    asyncio.run(main())