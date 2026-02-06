import pytest
from sqlalchemy.exc import IntegrityError
from app.db.crud_telegram import create_message, get_message_by_id, delete_message, create_topic, delete_topic
from tests.crud_telegram.factories import message_factory, topic_factory


@pytest.mark.asyncio
async def test_create_message_with_topic_id(session):
    topic_data = topic_factory()
    topic = await create_topic(
        session,
        chat_id=topic_data.chat_id,
        topic_id=topic_data.topic_id,
        name=topic_data.name,
        kind = topic_data.kind
    )
    message_data = message_factory(topic_id=topic.id)
    message = await create_message(
        session,
        message_id=message_data.message_id,
        topic_id=message_data.topic_id
    )

    assert message is not None
    assert message.id is not None
    assert message.message_id == message_data.message_id
    assert message.topic_id == message_data.topic_id

@pytest.mark.asyncio
async def test_create_message_with_invalid_topic_id(session):
    message_data = message_factory(topic_id=-1)
    with pytest.raises(IntegrityError):
        message = await create_message(
            session,
            message_id=message_data.message_id,
            topic_id=message_data.topic_id
        )

@pytest.mark.asyncio
async def test_create_message_without_topic_id(session):
    message_data = message_factory(topic_id=None)
    with pytest.raises(IntegrityError):
        message = await create_message(
            session,
            message_id=message_data.message_id,
            topic_id=message_data.topic_id
        )

@pytest.mark.asyncio
async def test_get_message_by_id(session):
    topic_data = topic_factory()
    topic = await create_topic(
        session,
        chat_id=topic_data.chat_id,
        topic_id=topic_data.topic_id,
        name=topic_data.name,
        kind = topic_data.kind
    )
    message_data = message_factory(topic_id=topic.id)
    message = await create_message(
        session,
        message_id=message_data.message_id,
        topic_id=message_data.topic_id
    )

    fetched = await get_message_by_id(session, message.id)
    assert fetched is not None
    assert fetched.id == message.id
    assert fetched.message_id == message.message_id
    assert fetched.topic_id == message.topic_id

@pytest.mark.asyncio
async def test_get_message_by_invalid_id(session):
    message = await get_message_by_id(session, -1)
    assert message is None

@pytest.mark.asyncio
async def test_delete_message(session):
    topic_data = topic_factory()
    topic = await create_topic(
        session,
        chat_id=topic_data.chat_id,
        topic_id=topic_data.topic_id,
        name=topic_data.name,
        kind = topic_data.kind
    )
    message_data = message_factory(topic_id=topic.id)
    message = await create_message(
        session,
        message_id=message_data.message_id,
        topic_id=message_data.topic_id
    )

    deleted = await delete_message(session, message.id)
    assert deleted is not None
    assert deleted.id == message.id
    assert deleted.message_id == message.message_id
    assert deleted.topic_id == message.topic_id

    fetched = await get_message_by_id(session, deleted.id)
    assert fetched is None

@pytest.mark.asyncio
async def test_delete_message_by_invalid_id(session):
    deleted = await delete_message(session, -1)
    assert deleted is None

@pytest.mark.asyncio
async def test_check_unique_chat_message(session):
    topic_data = topic_factory()
    topic = await create_topic(
        session,
        chat_id=topic_data.chat_id,
        topic_id=topic_data.topic_id,
        name=topic_data.name,
        kind = topic_data.kind
    )

    message_data_1 = message_factory(topic_id=topic.id)
    message_data_2 = message_factory(topic_id=topic.id, message_id=message_data_1.message_id)

    message_1 = await create_message(
        session,
        message_id=message_data_1.message_id,
        topic_id=message_data_1.topic_id
    )
    with pytest.raises(IntegrityError):
        message_2 = await create_message(
            session,
            message_id=message_data_2.message_id,
            topic_id=message_data_2.topic_id
        )

@pytest.mark.asyncio
async def test_cascade_delete_message(session):
    topic_data = topic_factory()
    topic_created = await create_topic(
        session,
        chat_id=topic_data.chat_id,
        topic_id=topic_data.topic_id,
        name=topic_data.name,
        kind = topic_data.kind
    )

    message_data_1 = message_factory(topic_id=topic_created.id, message_id=14)
    message_data_2 = message_factory(topic_id=topic_created.id, message_id=15)

    message_1_created = await create_message(
        session,
        message_id=message_data_1.message_id,
        topic_id=message_data_1.topic_id
    )
    message_2_created = await create_message(
        session,
        message_id=message_data_2.message_id,
        topic_id=message_data_2.topic_id
    )
    topic_deleted = await delete_topic(session, topic_created.id)
    session.expunge(message_1_created) # CLEAN CACHE BECAUSE OBJECTS ARE STILL IN IT
    session.expunge(message_2_created) # UNTIL TRANSACTION WAS COMPLETED

    message_1 = await get_message_by_id(session, message_1_created.id)
    assert message_1 is None

    message_2 = await get_message_by_id(session, message_2_created.id)
    assert message_2 is None


@pytest.mark.asyncio
async def test_create_2_messages(session):
    topic_data = topic_factory()
    topic = await create_topic(
        session,
        chat_id=topic_data.chat_id,
        topic_id=topic_data.topic_id,
        name=topic_data.name,
        kind = topic_data.kind
    )

    message_data_1 = message_factory(topic_id=topic.id, message_id=14)
    message_data_2 = message_factory(topic_id=topic.id, message_id=15)

    message_1 = await create_message(
        session,
        message_id=message_data_1.message_id,
        topic_id=message_data_1.topic_id
    )

    message_2 = await create_message(
        session,
        message_id=message_data_2.message_id,
        topic_id=message_data_2.topic_id
    )

    assert message_1.id != message_2
    assert message_1.topic_id == message_2.topic_id
    assert message_1.message_id == message_data_1.message_id
    assert message_2.message_id == message_data_2.message_id