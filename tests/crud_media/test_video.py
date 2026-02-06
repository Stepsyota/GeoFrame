import pytest
from app.db.crud_media import create_mediafile, get_mediafile
from tests.crud_media.factories import video_metadata
from app.db.tables_media import MediaType


@pytest.mark.asyncio
async def test_create_video(session):
    metadata = video_metadata()
    media = await create_mediafile(session, metadata)

    assert media.id is not None
    assert media.media_type == MediaType.video
    assert media.video is not None
    assert media.video.duration == metadata.duration
    assert media.image is None

@pytest.mark.asyncio
async def test_get_video(session):
    metadata = video_metadata()
    media = await create_mediafile(session, metadata)

    fetched = await get_mediafile(session, media.id)

    assert fetched is not None
    assert fetched.id is not None
    assert fetched.media_type == MediaType.video
    assert fetched.video is not None
    assert fetched.video.duration == metadata.duration
    assert fetched.image is None