from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.orm import DeclarativeBase
from typing import TypeVar, Type
from app.db.tables_telegram import TopicKind, TelegramTopic, TelegramMessage, MediaTelegramMessage


ModelType= TypeVar("ModelType", bound=DeclarativeBase)
# COMMON
async def delete_entity(session : AsyncSession, model : Type[ModelType], id : int) -> ModelType | None:
    obj = await get_entity(session, model, id)
    if obj:
        await session.delete(obj)
        await session.flush()
    return obj

async def get_entity(session : AsyncSession, model : Type[ModelType], id : int) -> ModelType | None:
    return await session.get(model, id)

#TelegramTopics
async def create_topic(session : AsyncSession, chat_id : int, topic_id : int, name : str, kind : TopicKind):
    topic = TelegramTopic(chat_id=chat_id, topic_id=topic_id, name=name, kind=kind)
    session.add(topic)
    await session.flush()
    return topic

async def get_topic_by_id(session : AsyncSession, id : int):
    return await get_entity(session, TelegramTopic, id)

async def get_topics_by_chat(session : AsyncSession, chat_id : int):
    result = await session.execute(
        select(TelegramTopic).where(TelegramTopic.chat_id == chat_id)
    )
    return result.scalars().all()

async def delete_topic(session : AsyncSession, id : int):
    return await delete_entity(session, TelegramTopic, id)

#TelegramMessage
async def create_message(session : AsyncSession, message_id : int, topic_id : int | None = None):
    message = TelegramMessage(message_id=message_id, topic_id=topic_id)
    session.add(message)
    await session.flush()
    await session.refresh(message, ["topic"])
    return message

async def get_message_by_id(session : AsyncSession, id : int):
    return await get_entity(session, TelegramMessage, id)

async def delete_message(session : AsyncSession, id : int):
    return await delete_entity(session, TelegramMessage, id)

#MediaTelegramMessage
async def create_media_message(session : AsyncSession, media_id : int, telegram_message_id : int):
    media_message = MediaTelegramMessage(media_id=media_id, telegram_message_id=telegram_message_id)
    session.add(media_message)
    await session.flush()
    await session.refresh(media_message, ["media", "telegram_message"])
    return media_message

async def get_media_message_by_id(session : AsyncSession, id : int):
    return await get_entity(session, MediaTelegramMessage, id)