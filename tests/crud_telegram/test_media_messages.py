import pytest
from sqlalchemy.exc import IntegrityError
from app.db.crud_telegram import create_topic, create_message, create_media_message, \
    get_media_message_by_id, get_message_by_id, delete_message
from app.db.crud_media import create_mediafile, delete_mediafile, get_mediafile
from tests.crud_telegram.factories import topic_factory, message_factory
from tests.crud_media.factories import image_metadata, video_metadata

@pytest.mark.asyncio
async def test_create_media_message(session):
    topic_data = topic_factory()
    topic = await create_topic(
        session,
        chat_id=topic_data.chat_id,
        topic_id=topic_data.topic_id,
        name=topic_data.name,
        kind=topic_data.kind
    )
    message_data_1 = message_factory(topic_id=topic.id, message_id=1)
    message_data_2 = message_factory(topic_id=topic.id, message_id=2)
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

    image_data = image_metadata()
    video_data = video_metadata()
    media_image = await create_mediafile(session, image_data)
    media_video = await create_mediafile(session, video_data)

    media_message_image = await create_media_message(
        session,
        media_id=media_image.id,
        telegram_message_id=message_1.id,
    )

    media_message_video = await create_media_message(
        session,
        media_id=media_video.id,
        telegram_message_id=message_2.id,
    )

    assert media_message_image.id is not None
    assert media_message_image.media_id == media_image.id
    assert media_message_image.telegram_message_id == message_1.id
    assert media_message_video.id is not None
    assert media_message_video.media_id == media_video.id
    assert media_message_video.telegram_message_id == message_2.id

@pytest.mark.asyncio
async def test_create_media_message_invalid_media(session):
    topic_data = topic_factory()
    topic = await create_topic(
        session,
        chat_id=topic_data.chat_id,
        topic_id=topic_data.topic_id,
        name=topic_data.name,
        kind=topic_data.kind
    )
    message_data = message_factory(topic_id=topic.id)
    message = await create_message(
        session,
        message_id=message_data.message_id,
        topic_id=message_data.topic_id
    )

    with pytest.raises(IntegrityError):
        media_message = await create_media_message(
            session,
            media_id=-1,
            telegram_message_id=message.id,
        )

@pytest.mark.asyncio
async def test_create_media_message_invalid_message(session):
    image_data = image_metadata()
    media = await create_mediafile(session, image_data)

    with pytest.raises(IntegrityError):
        media_message = await create_media_message(
            session,
            media_id=media.id,
            telegram_message_id=-1,
        )

@pytest.mark.asyncio
async def test_get_media_message(session):
    topic_data = topic_factory()
    topic = await create_topic(
        session,
        chat_id=topic_data.chat_id,
        topic_id=topic_data.topic_id,
        name=topic_data.name,
        kind=topic_data.kind
    )
    message_data = message_factory(topic_id=topic.id)
    message = await create_message(
        session,
        message_id=message_data.message_id,
        topic_id=message_data.topic_id
    )

    image_data = image_metadata()
    media_image = await create_mediafile(session, image_data)

    media_message = await create_media_message(
        session,
        media_id=media_image.id,
        telegram_message_id=message.id,
    )

    fetched = await get_media_message_by_id(session, media_message.id)

    assert fetched.id == media_message.id
    assert fetched.media_id == media_message.media_id
    assert fetched.telegram_message_id == media_message.telegram_message_id

@pytest.mark.asyncio
async def test_get_invalid_media_message(session):
    media_message = await get_media_message_by_id(session, -1)
    assert media_message is None

@pytest.mark.asyncio
async def test_check_unique_media_message(session):
    topic_data = topic_factory()
    topic = await create_topic(
        session,
        chat_id=topic_data.chat_id,
        topic_id=topic_data.topic_id,
        name=topic_data.name,
        kind=topic_data.kind
    )
    message_data = message_factory(topic_id=topic.id)
    message = await create_message(
        session,
        message_id=message_data.message_id,
        topic_id=message_data.topic_id
    )

    image_data = image_metadata()
    media_image = await create_mediafile(session, image_data)

    media_message_1 = await create_media_message(
        session,
        media_id=media_image.id,
        telegram_message_id=message.id,
    )

    with pytest.raises(IntegrityError):
        media_message_2 = await create_media_message(
            session,
            media_id=media_image.id,
            telegram_message_id=message.id,
        )

@pytest.mark.asyncio
async def test_cascade_delete_media_from_media_message(session):
    topic_data = topic_factory()
    topic = await create_topic(
        session,
        chat_id=topic_data.chat_id,
        topic_id=topic_data.topic_id,
        name=topic_data.name,
        kind=topic_data.kind
    )
    message_data = message_factory(topic_id=topic.id)
    message = await create_message(
        session,
        message_id=message_data.message_id,
        topic_id=message_data.topic_id
    )

    image_data = image_metadata()
    media_image = await create_mediafile(session, image_data)

    media_message = await create_media_message(
        session,
        media_id=media_image.id,
        telegram_message_id=message.id,
    )

    media_deleted = await delete_mediafile(session, media_image.id)
    assert media_deleted is not None

    fetched_media = await get_mediafile(session, media_deleted.id)
    assert fetched_media is None

    session.expunge(media_message)
    session.expunge(message)

    fetched_media_message = await get_media_message_by_id(session, media_message.id)
    assert fetched_media_message is None

    fetched_message = await get_message_by_id(session, message.id)
    assert fetched_message is not None

@pytest.mark.asyncio
async def test_cascade_delete_message_from_media_message(session):
    topic_data = topic_factory()
    topic = await create_topic(
        session,
        chat_id=topic_data.chat_id,
        topic_id=topic_data.topic_id,
        name=topic_data.name,
        kind=topic_data.kind
    )
    message_data = message_factory(topic_id=topic.id)
    message = await create_message(
        session,
        message_id=message_data.message_id,
        topic_id=message_data.topic_id
    )

    image_data = image_metadata()
    media_image = await create_mediafile(session, image_data)

    media_message = await create_media_message(
        session,
        media_id=media_image.id,
        telegram_message_id=message.id,
    )

    message_deleted = await delete_message(session, message.id)
    assert message_deleted is not None

    fetched_message = await get_message_by_id(session, message_deleted.id)
    assert fetched_message is None

    session.expunge(media_message) # CLEAN CACHE BECAUSE OBJECTS ARE STILL IN IT
    session.expunge(media_image)   # UNTIL TRANSACTION WAS COMPLETED

    fetched_media_message = await get_media_message_by_id(session, media_message.id)
    assert fetched_media_message is None

    fetched_media = await get_mediafile(session, media_image.id)
    assert fetched_media is not None