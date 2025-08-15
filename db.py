from sqlalchemy import create_engine, Column, Integer, String, DateTime, BigInteger, ForeignKey
from sqlalchemy.orm import sessionmaker, declarative_base, relationship
from datetime import datetime, timezone

Base = declarative_base()

class Owner(Base):
    __tablename__ = 'Owners'

    id = Column(Integer, primary_key=True)
    user_id = Column(BigInteger, unique=True, nullable=False)
    full_name = Column(String, nullable=False)
    created_at = Column(DateTime, default=lambda: datetime.now(timezone.utc))
    media_path = Column(String, nullable=True)
    chats = relationship("Chat", back_populates="owner", cascade="all, delete-orphan")

class Chat(Base):
    __tablename__ = 'Chats'

    id = Column(Integer, primary_key=True)
    chat_id = Column(BigInteger, unique=True, nullable=False)
    chat_title = Column(String, nullable=False)
    created_at = Column(DateTime, default=lambda: datetime.now(timezone.utc))

    owner_id = Column(Integer, ForeignKey('Owners.id'), nullable=False)
    owner = relationship("Owner", back_populates="chats")

engine = create_engine('sqlite:///bot.db', echo=False)
SessionLocal = sessionmaker(bind=engine)

def init_db():
    Base.metadata.create_all(engine)

def get_or_create_owner(owner_id, owner_name):
    with SessionLocal() as session:
        owner = session.query(Owner).filter_by(user_id=owner_id).first()
        if not owner:
            owner = Owner(user_id=owner_id, full_name=owner_name)
            session.add(owner)
            session.commit()
            session.refresh(owner)
        return owner

def add_chat(chat_id, chat_title, owner_id, owner_name):
    with SessionLocal() as session:
        owner = session.query(Owner).filter_by(user_id=owner_id).first()
        if not owner:
            owner = Owner(user_id=owner_id, full_name=owner_name)
            session.add(owner)
            session.commit()
            session.refresh(owner)

        existing_chat = session.query(Chat).filter_by(chat_id=chat_id).first()
        if existing_chat:
            return False

        new_chat = Chat(chat_id=chat_id, chat_title=chat_title, owner=owner)
        session.add(new_chat)
        session.commit()
        return True

def get_chats_by_owner(owner_id):
    with SessionLocal() as session:
        owner = session.query(Owner).filter_by(user_id=owner_id).first()
        if not owner:
            return []
        return owner.chats

def set_owner_media_path(owner_id, path):
    with SessionLocal() as session:
        owner = session.query(Owner).filter_by(user_id=owner_id).first()
        if not owner:
            return False
        owner.media_path = path
        session.commit()
        return True