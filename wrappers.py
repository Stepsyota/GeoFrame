import logging
from functools import wraps
from telegram import Update
from telegram.ext import ContextTypes

def handler_wrapper(func):
    @wraps(func)
    async def wrapped(update: Update, context: ContextTypes.DEFAULT_TYPE, *args, **kwargs):
        try:
            return await func(update, context, *args, **kwargs)
        except Exception as e:
            logging.exception(f"Error in handler {func.__name__}: {e}")
            if update and update.message:
                await update.message.reply_text("⚠️ An error has occurred. Try again..")
    return wrapped