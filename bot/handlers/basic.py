from telegram import Update, InputFile
from telegram.ext import ApplicationBuilder, CommandHandler, ContextTypes

async def hello(update: Update, context: ContextTypes.DEFAULT_TYPE) -> None:
    await update.message.reply_text(f'Hello, my name is GeoFrame📸')

async def balls(update: Update, context: ContextTypes.DEFAULT_TYPE) -> None:
    await update.message.reply_photo('media/balls.jpg')