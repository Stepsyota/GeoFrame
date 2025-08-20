import os
from telegram import Update, InputFile
from telegram.constants import ChatAction
from telegram.ext import ApplicationBuilder, CommandHandler, ContextTypes

from db import get_or_create_owner, add_chat, get_chats_by_owner, set_owner_media_path
from ..utils.media import get_sorted_files

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

async def send_not_compressed_photo(update: Update, context: ContextTypes.DEFAULT_TYPE) -> None:
    user = update.effective_user
    owner = get_or_create_owner(user.id, user.full_name)

    if not owner:
        await update.message.reply_text("You are not registered yet. First, add the bot to the group and run /register.")
        return

    if not owner.media_path:
        await update.message.reply_text("The path has not been set yet. Use /setpath to set it.")
        return

    chats = get_chats_by_owner(user.id)

    if not chats:
        await update.message.reply_text("You don't have any registered groups")
        return

    chat_id = chats[0].chat_id
    files = get_sorted_files(owner.media_path)
    if not files:
        await update.message.reply_text("The directory is empty.")
        return

    photo_exts = {".jpg", ".jpeg", ".png", ".gif", ".heic"}
    video_exts = {".mp4", ".mov", ".avi", ".webm"}

    for file_path in files:
        ext = file_path.suffix.lower()
        try:
            if ext in photo_exts:
                    await context.bot.send_document(chat_id=chat_id, document=file_path, thumbnail=file_path)

            elif ext in video_exts:
                with open(file_path, "rb") as file:
                    await context.bot.send_video(chat_id=chat_id, video=file)

        except Exception as e:
            await update.message.reply_text(f"Failed to send {file_path.name}: {e}")


