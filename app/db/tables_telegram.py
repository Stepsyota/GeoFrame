from sqlalchemy import Column, Integer, String, BIGINT, Enum, UniqueConstraint, ForeignKey
from sqlalchemy.orm import relationship, backref
import enum
from app.db.tables_common import Base


class TopicKind(enum.Enum):
    original = "original"
    day_photo = "day_photo"

class TelegramTopic(Base):
    __tablename__ = "telegram_topics"

    id = Column(BIGINT, primary_key=True)
    chat_id = Column(BIGINT, nullable=False)
    topic_id = Column(BIGINT, nullable=False)
    name = Column(String, nullable=False, unique=True)
    kind = Column(Enum(TopicKind), nullable=False)

    __table_args__ = (
        UniqueConstraint("chat_id", "topic_id", name= "unique_pair_chat_topic"),
    )

class TelegramMessage(Base):
    __tablename__ = "telegram_messages"

    id = Column(BIGINT, primary_key=True)
    message_id = Column(Integer, nullable=False)
    topic_id = Column(
        BIGINT,
        ForeignKey("telegram_topics.id", ondelete="CASCADE"),
        nullable=False
    )

    topic = relationship("TelegramTopic", backref=backref("messages", passive_deletes=True))

    __table_args__ = (
        UniqueConstraint("topic_id", "message_id", name= "unique_pair_topic_message"),
    )

class MediaTelegramMessage(Base):
    __tablename__ = "media_telegram_messages"

    id = Column(Integer, primary_key=True)
    media_id = Column(Integer, ForeignKey("media_files.id", ondelete="CASCADE"), nullable=False)
    telegram_message_id = Column(BIGINT, ForeignKey("telegram_messages.id", ondelete="CASCADE"), nullable=False)

    media = relationship("MediaFile", backref=backref("telegram_message", passive_deletes=True))
    telegram_message = relationship("TelegramMessage", backref=backref("media_files", passive_deletes=True))

    __table_args__ = (
        UniqueConstraint("media_id", "telegram_message_id", name="unique_pair_media_message"),
    )