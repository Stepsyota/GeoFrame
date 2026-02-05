import pytest
from app.db.crud_media import create_mediafile, get_mediafile
from tests.factories import image_metadata
from app.db.tables_media import MediaType


@pytest.mark.asyncio
async def test_create_image(session):
    metadata = image_metadata()
    media = await create_mediafile(session, metadata)

    assert media.id is not None
    assert media.media_type == MediaType.image
    assert media.image is not None
    assert media.image.phash == metadata.phash
    assert media.video is None

@pytest.mark.asyncio
async def test_get_image(session):
    metadata = image_metadata()
    media = await create_mediafile(session, metadata)

    fetched = await get_mediafile(session, media.id)

    assert fetched is not None
    assert fetched.id is not None
    assert fetched.media_type == MediaType.image
    assert fetched.image is not None
    assert fetched.image.phash == metadata.phash
    assert fetched.video is None