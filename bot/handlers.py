from telegram import Update
from telegram.ext import ApplicationBuilder, CommandHandler, ContextTypes

from db import get_or_create_owner, add_chat, get_chats_by_owner

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

def register_handlers(app):
    app.add_handler(CommandHandler("hello", hello))
    app.add_handler(CommandHandler("balls", balls))
    app.add_handler(CommandHandler("register", register_group))
    app.add_handler(CommandHandler("list_groups", list_groups))
