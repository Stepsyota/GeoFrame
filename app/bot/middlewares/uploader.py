from aiogram import Bot
from sqlalchemy import select

from app.db.database import async_session_factory
from app.db.tables_telegram import TelegramTopic, TopicKind
from app.config import CHAT_ID

existing_topics = {}

async def get_or_create_topic(bot : Bot, chat_id : int, kind : TopicKind, name : str):
    async with async_session_factory() as session:
        async with session.begin():
            result = await session.execute(
                select(TelegramTopic).where(
                    TelegramTopic.chat_id == chat_id,
                    TelegramTopic.kind == kind,
                    TelegramTopic.name == name
                )
            )
            topic = result.scalar_one_or_none()
            if topic:
                try:
                    await bot.edit_forum_topic(chat_id=chat_id, message_thread_id=topic.topic_id, name="AAAAAAAAAAAAAAAAAAAaa")
                    return topic
                except Exception as e:
                    print(f"GET/CREATE TOPIC SKIP: {e}")
                    await session.delete(topic) #????

            sent = await bot.send_message(chat_id, text=f"Creating a topic: {name}")
            topic_id= (await bot.create_forum_topic(chat_id=chat_id, name=name)).message_thread_id
            if topic_id is None:
                raise

            topic = TelegramTopic(chat_id=chat_id, topic_id=topic_id, kind=kind, name=name)
            session.add(topic)

        return topic

async def send_message(bot : Bot, message_thread_id, text : str ="First message"):
    await bot.send_message(
        chat_id=CHAT_ID,
        message_thread_id=message_thread_id,
        text=text
    )

async def upload_photos():
    pass




