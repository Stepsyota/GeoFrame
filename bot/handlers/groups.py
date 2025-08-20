from telegram import Update, InputFile
from telegram.ext import ApplicationBuilder, CommandHandler, ContextTypes

from db import get_or_create_owner, add_chat, get_chats_by_owner, set_owner_media_path

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