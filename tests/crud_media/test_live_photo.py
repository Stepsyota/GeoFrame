import pytest
from app.db.crud_media import create_mediafile, create_live_photo, get_live_photo
from tests.factories import image_metadata, video_metadata


@pytest.mark.asyncio
async def test_create_live_photo(session):
    metadata_image = image_metadata()
    image = await create_mediafile(session, metadata_image)
    metadata_video = video_metadata()
    video = await create_mediafile(session, metadata_video)

    assert image is not None
    assert video is not None
    assert image.image is not None
    assert video.video is not None
    live_photo = await create_live_photo(session, image.id, video.id)

    assert live_photo.id is not None
    assert live_photo.image_media_file is not None
    assert live_photo.video_media_file is not None
    assert live_photo.image_media_file.filename == metadata_image.filename
    assert live_photo.video_media_file.filename == metadata_video.filename
    assert live_photo.image_media_file.image is not None
    assert live_photo.video_media_file.video is not None
    assert live_photo.image_media_file.image.phash == metadata_image.phash
    assert live_photo.video_media_file.video.duration == metadata_video.duration

@pytest.mark.asyncio
async def test_get_live_photo(session):
    metadata_image = image_metadata()
    image = await create_mediafile(session, metadata_image)
    metadata_video = video_metadata()
    video = await create_mediafile(session, metadata_video)

    assert image is not None
    assert video is not None
    assert image.image is not None
    assert video.video is not None
    live_photo = await create_live_photo(session, image.id, video.id)
    fetched = await get_live_photo(session, live_photo.id)

    assert fetched.id is not None
    assert fetched.id == live_photo.id
    assert fetched.image_media_file is not None
    assert fetched.video_media_file is not None
    assert fetched.image_media_file.filename == metadata_image.filename
    assert fetched.video_media_file.filename == metadata_video.filename
    assert fetched.image_media_file.image is not None
    assert fetched.video_media_file.video is not None
    assert fetched.image_media_file.image.phash == metadata_image.phash
    assert fetched.video_media_file.video.duration == metadata_video.duration

@pytest.mark.asyncio
async def test_get_live_photo_invalid_id(session):
    result = await get_live_photo(session, -1)
    assert result is None