import os
from telegram import Update
from telegram.constants import ChatAction
from telegram.ext import ApplicationBuilder, CommandHandler, ContextTypes

from db import get_or_create_owner, add_chat, get_chats_by_owner, set_owner_media_path

async def hello(update: Update, context: ContextTypes.DEFAULT_TYPE) -> None:
    await update.message.reply_text(f'Hello, my name is GeoFrame📸')

async def balls(update: Update, context: ContextTypes.DEFAULT_TYPE) -> None:
    await update.message.reply_photo('media/balls.jpg')

async def register_group(update: Update, context: ContextTypes.DEFAULT_TYPE) -> None:
    chat = update.effective_chat
    user = update.effective_user

    if chat.type not in ("group", "supergroup"):
        await update.message.reply_text("This command only works in a group or supergroup")
        return

    admins = await context.bot.get_chat_administrators(chat.id)
    admin_ids = [admin.user.id for admin in admins]

    if user.id not in admin_ids:
        await update.message.reply_text("Only the administrators of the group can register it")
        return

    if add_chat(chat.id, chat.title, user.id, user.full_name):
        await update.message.reply_text("Chat registered successfully")
    else:
        await update.message.reply_text("Chat already registered")

async def list_groups(update: Update, context: ContextTypes.DEFAULT_TYPE) -> None:
    user = update.effective_user
    chats = get_chats_by_owner(user.id)

    if not chats:
        await update.message.reply_text("You don't have any registered groups")
        return

    msg = "Your registered groups:\n"
    for chat in chats:
        msg += f"- {chat.chat_title} (ID: {chat.chat_id})\n"

    await update.message.reply_text(msg)

async def set_path(update: Update, context: ContextTypes.DEFAULT_TYPE) -> None:
   if not context.args:
       await update.message.reply_text('You need to specify the path in the format "/set_path <ABSOLUTE_PATH>"')
       return

   path = " ".join(context.args)
   if set_owner_media_path(update.effective_chat.id, path):
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


async def scan_directory(update: Update, context: ContextTypes.DEFAULT_TYPE) -> None:
    user = update.effective_user
    owner = get_or_create_owner(user.id, user.full_name)

    if not owner or not owner.media_path:
        await update.message.reply_text("❌ The path is not set. Use /setpath.")
        return

    path = owner.media_path

    if not os.path.exists(path):
        await update.message.reply_text(f"❌ The path `{path}` does not exist.", parse_mode="Markdown")
        return

    if not os.path.isdir(path):
        await update.message.reply_text(f"❌ The path `{path}` is not a directory.", parse_mode="Markdown")
        return

    await update.message.chat.send_action(ChatAction.TYPING)

    photo_exts = {".jpg", ".jpeg", ".png", ".gif", ".heic"}
    video_exts = {".mp4", ".mov", ".avi", ".webm"}
    photo_count = 0
    video_count = 0

    try:
        for root, dirs, files in os.walk(path):
            for file in files:
                ext = os.path.splitext(file)[1].lower()
                if ext in photo_exts:
                    photo_count += 1
                elif ext in video_exts:
                    video_count += 1
    except PermissionError:
        await update.message.reply_text("⚠️ There is no access to some files or folders.")
        return

    await update.message.reply_text(
        f"📂 The scan is complete!\n"
        f"📷 Photo: {photo_count}\n"
        f"🎥 Video: {video_count}\n"
        f"📊 Total media files: {photo_count + video_count}"
    )


def register_handlers(app):
    app.add_handler(CommandHandler("hello", hello))
    app.add_handler(CommandHandler("balls", balls))
    app.add_handler(CommandHandler("register", register_group))
    app.add_handler(CommandHandler("list_groups", list_groups))
    app.add_handler(CommandHandler("set_path", set_path))
    app.add_handler(CommandHandler("get_path", get_path))
    app.add_handler(CommandHandler("scan_directory", scan_directory))
