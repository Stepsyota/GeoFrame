import logging
from telegram import Update, InputFile
from telegram.ext import ApplicationBuilder, CommandHandler, ContextTypes

async def hello(update: Update, context: ContextTypes.DEFAULT_TYPE) -> None:
    await update.message.reply_text(f'Hello, my name is GeoFrame📸')

async def balls(update: Update, context: ContextTypes.DEFAULT_TYPE) -> None:
    await update.message.reply_photo('media/balls.jpg')

async def error_handler(update: object, context: ContextTypes.DEFAULT_TYPE) -> None:
    logging.exception(f"An error has occurred: {context.error}")
    if isinstance(update, Update) and update.message:
        await update.message.reply_text("⚠️ Oops, something went wrong.")