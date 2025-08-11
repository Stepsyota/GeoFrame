from config import TOKEN

from telegram.ext import ApplicationBuilder
from bot.handlers import register_handlers

import logging
from logger import setup_logger

from db import init_db

def main():
    setup_logger()
    logging.info("BOT STARTED")
    app = ApplicationBuilder().token(TOKEN).build()
    register_handlers(app)
    app.run_polling()

if __name__ == '__main__':
    init_db()
    main()