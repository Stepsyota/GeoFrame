from dataclasses import dataclass
from app.db.tables_telegram import TopicKind

@dataclass
class TopicInfo:
    chat_id : int
    topic_id : int
    name : str
    kind : TopicKind

def topic_factory(chat_id = 1, topic_id = 15, name="Some name", kind=TopicKind.original):
    return TopicInfo(
        chat_id=chat_id,
        topic_id=topic_id,
        name=name,
        kind=kind
    )

@dataclass
class MessageInfo:
    message_id : int
    topic_id : int | None = None

def message_factory(message_id = 100, topic_id = None):
    return MessageInfo(
        message_id=message_id,
        topic_id=topic_id
    )