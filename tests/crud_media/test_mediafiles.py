from pathlib import Path
import pytest
from sqlalchemy.exc import IntegrityError
from app.db.crud_media import create_mediafile, get_mediafile, get_mediafiles
from tests.crud_media.factories import image_metadata, video_metadata
from app.db.tables_media import MediaFile


@pytest.mark.asyncio
async def test_create_mediafile(session):
    metadata_image = image_metadata()

    image = await create_mediafile(session, metadata_image)
    assert image.id is not None
    assert image.storage_path == str(metadata_image.storage_path)
    assert image.taken_at == metadata_image.taken_at
    assert image.sha256 == metadata_image.sha256
    assert image.media_type == metadata_image.media_type

@pytest.mark.asyncio
async def test_create_mediafile_duplicate_sha256(session):
    metadata_image_1 = image_metadata()
    metadata_image_2 = image_metadata()
    metadata_image_2.storage_path = Path("/tmp/test2.jpg")

    image_1 = await create_mediafile(session, metadata_image_1)
    with pytest.raises(IntegrityError):
        image_2 = await create_mediafile(session, metadata_image_2)

@pytest.mark.asyncio
async def test_create_mediafile_duplicate_storage_path(session):
    metadata_image_1 = image_metadata()
    metadata_image_2 = image_metadata()
    metadata_image_2.sha256 = "1"*64

    image_1 = await create_mediafile(session, metadata_image_1)
    with pytest.raises(IntegrityError):
        image_2 = await create_mediafile(session, metadata_image_2)

@pytest.mark.asyncio
async def test_get_mediafile_invalid_id(session):
    result = await get_mediafile(session, -1)
    assert result is None

@pytest.mark.asyncio
async def test_get_mediafiles(session):
    metadata_image = image_metadata()
    image = await create_mediafile(session, metadata_image)
    metadata_video = video_metadata()
    video = await create_mediafile(session, metadata_video)

    mediafiles = await get_mediafiles(session)

    result_ids = {m.id for m in mediafiles} # set
    expected_ids = {image.id, video.id} # set

    assert len(mediafiles) == 2
    assert all(isinstance(m, MediaFile) for m in mediafiles)
    assert  result_ids == expected_ids

@pytest.mark.asyncio
async def test_get_mediafiles_empty_db(session):
    mediafiles = await get_mediafiles(session)
    assert mediafiles == []