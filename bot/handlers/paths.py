from telegram import Update, InputFile
from telegram.ext import ApplicationBuilder, CommandHandler, ContextTypes

from db import get_or_create_owner, add_chat, get_chats_by_owner, set_owner_media_path

async def set_path(update: Update, context: ContextTypes.DEFAULT_TYPE) -> None:
   if not context.args:
       await update.message.reply_text('You need to specify the path in the format "/set_path <ABSOLUTE_PATH>"')
       return

   path = " ".join(context.args)
   if set_owner_media_path(update.effective_user.id, path):
       await update.message.reply_text(f"The path `{context.args}` was successfully added", parse_mode="Markdown")
   else:
       await update.message.reply_text("Owner not found. Please register first.")

async def get_path(update: Update, context: ContextTypes.DEFAULT_TYPE) -> None:
   user = update.effective_user
   owner = get_or_create_owner(user.id, user.full_name)

   if not owner:
        await update.message.reply_text("You are not registered yet. First, add the bot to the group and run /register.")
        return

   if not owner.media_path:
        await update.message.reply_text("The path has not been set yet. Use /setpath to set it.")
        return

   await update.message.reply_text(f"The current way to save media:\n`{owner.media_path}`", parse_mode="Markdown")