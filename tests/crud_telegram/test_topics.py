import pytest
from sqlalchemy.exc import IntegrityError

from app.db.crud_telegram import create_topic, get_topic_by_id, get_topics_by_chat, delete_topic
from app.db.tables_telegram import TopicKind, TelegramTopic
from tests.crud_telegram.factories import topic_factory


@pytest.mark.asyncio
async def test_create_topic(session):
    topic_data = topic_factory()
    topic = await create_topic(
        session,
        chat_id=topic_data.chat_id,
        topic_id=topic_data.topic_id,
        name=topic_data.name,
        kind = topic_data.kind
    )

    assert topic is not None
    assert topic.id is not None
    assert topic.chat_id == topic_data.chat_id
    assert topic.name == topic_data.name
    assert topic.kind == topic_data.kind

@pytest.mark.asyncio
async def test_check_unique_chat_topic(session):
    topic_data_1 = topic_factory()
    topic_data_2 = topic_factory(chat_id=topic_data_1.chat_id, topic_id=topic_data_1.topic_id, \
                                 name= "Topic2", kind=TopicKind.original)
    topic_1 = await create_topic(
        session,
        chat_id=topic_data_1.chat_id,
        topic_id=topic_data_1.topic_id,
        name=topic_data_1.name,
        kind = topic_data_1.kind
    )
    with pytest.raises(IntegrityError):
        topic_2 = await create_topic(
            session,
            chat_id=topic_data_2.chat_id,
            topic_id=topic_data_2.topic_id,
            name=topic_data_2.name,
            kind = topic_data_2.kind
        )


@pytest.mark.asyncio
async def test_create_topics_in_same_chat(session):
    topic_data_1 = topic_factory(chat_id=100, topic_id=53, name="Topic1", kind=TopicKind.original)
    topic_data_2 = topic_factory(chat_id = 100, topic_id=51, name="Topic2", kind=TopicKind.day_photo)
    topic_1 = await create_topic(
        session,
        chat_id=topic_data_1.chat_id,
        topic_id=topic_data_1.topic_id,
        name=topic_data_1.name,
        kind = topic_data_1.kind
    )
    topic_2 = await create_topic(
        session,
        chat_id=topic_data_2.chat_id,
        topic_id=topic_data_2.topic_id,
        name=topic_data_2.name,
        kind = topic_data_2.kind
    )
    assert topic_1 is not None
    assert topic_2 is not None
    assert topic_1.id != topic_2.id
    assert topic_1.chat_id == topic_2.chat_id
    assert topic_1.topic_id == topic_data_1.topic_id
    assert topic_2.topic_id == topic_data_2.topic_id
    assert topic_1.name == topic_data_1.name
    assert topic_2.name == topic_data_2.name
    assert topic_1.kind == topic_data_1.kind
    assert topic_2.kind == topic_data_2.kind

@pytest.mark.asyncio
async def test_get_topic_by_id(session):
    topic_data = topic_factory()
    topic = await create_topic(
        session,
        chat_id=topic_data.chat_id,
        topic_id=topic_data.topic_id,
        name=topic_data.name,
        kind = topic_data.kind
    )
    fetched = await get_topic_by_id(session, topic.id)

    assert fetched is not None
    assert fetched.id is not None
    assert fetched.id == topic.id
    assert fetched.chat_id == topic.chat_id
    assert fetched.name == topic.name
    assert fetched.kind == topic.kind

@pytest.mark.asyncio
async def test_get_topic_by_invalid_id(session):
    result = await get_topic_by_id(session, -1)
    assert result is None

@pytest.mark.asyncio
async def test_get_topics_by_chat(session):
    topic_data_1 = topic_factory(chat_id=100, topic_id=53, name="Topic1", kind=TopicKind.original)
    topic_data_2 = topic_factory(chat_id = 100, topic_id=51, name="Topic2", kind=TopicKind.day_photo)
    topic_1 = await create_topic(
        session,
        chat_id=topic_data_1.chat_id,
        topic_id=topic_data_1.topic_id,
        name=topic_data_1.name,
        kind = topic_data_1.kind
    )
    topic_2 = await create_topic(
        session,
        chat_id=topic_data_2.chat_id,
        topic_id=topic_data_2.topic_id,
        name=topic_data_2.name,
        kind = topic_data_2.kind
    )
    topics = await get_topics_by_chat(session, topic_data_1.chat_id)

    result_ids = {t.id for t in topics} # set
    expected_ids = {topic_1.id, topic_2.id} # set

    assert len(topics) == 2
    assert all(isinstance(t, TelegramTopic) for t in topics)
    assert result_ids == expected_ids

@pytest.mark.asyncio
async def test_get_topics_by_chat_with_other_topics(session):
    topic_data_1 = topic_factory(chat_id=100, topic_id=53, name="Topic1", kind=TopicKind.original)
    topic_data_2 = topic_factory(chat_id = 100, topic_id=51, name="Topic2", kind=TopicKind.day_photo)
    topic_data_3 = topic_factory(chat_id=86, topic_id=73, name="Topic3_with_other_chat_id", kind=TopicKind.original)
    topic_1 = await create_topic(
        session,
        chat_id=topic_data_1.chat_id,
        topic_id=topic_data_1.topic_id,
        name=topic_data_1.name,
        kind = topic_data_1.kind
    )
    topic_2 = await create_topic(
        session,
        chat_id=topic_data_2.chat_id,
        topic_id=topic_data_2.topic_id,
        name=topic_data_2.name,
        kind = topic_data_2.kind
    )
    topic_3 = await create_topic(
        session,
        chat_id=topic_data_3.chat_id,
        topic_id=topic_data_3.topic_id,
        name=topic_data_3.name,
        kind = topic_data_3.kind
    )
    topics = await get_topics_by_chat(session, topic_data_1.chat_id)

    result_ids = {t.id for t in topics} # set
    expected_ids = {topic_1.id, topic_2.id} # set

    assert len(topics) == 2
    assert all(isinstance(t, TelegramTopic) for t in topics)
    assert result_ids == expected_ids

@pytest.mark.asyncio
async def test_get_topics_by_chat_empty_db(session):
    topics = await get_topics_by_chat(session, 1)
    assert topics == []

@pytest.mark.asyncio
async def test_delete_topic(session):
    topic_data = topic_factory()
    topic = await create_topic(
        session,
        chat_id=topic_data.chat_id,
        topic_id=topic_data.topic_id,
        name=topic_data.name,
        kind = topic_data.kind
    )
    deleted = await delete_topic(session, topic.id)

    assert deleted is not None
    assert deleted.id is not None
    assert deleted.id == topic.id
    assert deleted.chat_id == topic.chat_id
    assert deleted.name == topic.name

    fetched = await get_topic_by_id(session, deleted.id)
    assert fetched is None

@pytest.mark.asyncio
async def test_delete_invalid_topic(session):
    deleted = await delete_topic(session, 1)
    assert deleted is None