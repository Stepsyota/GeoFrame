import pytest
from sqlalchemy.exc import IntegrityError
from app.db.crud_media import create_mediafile, get_mediafile, get_mediafiles
from tests.factories import image_metadata, video_metadata, base_metadata
from app.db.tables_media import MediaFile


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
async def test_create_mediafile_without_type(session):
    metadata = base_metadata()

    with pytest.raises(ValueError, match="Cannot determine media type"):
        await create_mediafile(session, metadata)


@pytest.mark.asyncio
async def test_get_mediafile_invalid_id(session):
    result = await get_mediafile(session, -1)
    assert result is None

@pytest.mark.asyncio
async def test_get_mediafiles_empty_db(session):
    mediafiles = await get_mediafiles(session)
    assert mediafiles == []

@pytest.mark.asyncio
async def test_create_mediafile_filename_not_nullable(session):
    metadata = image_metadata()
    metadata.filename = None

    with pytest.raises(IntegrityError):
        await create_mediafile(session, metadata)

@pytest.mark.asyncio
async def test_create_mediafile_sha256not_nullable(session):
    metadata = image_metadata()
    metadata.sha256= None

    with pytest.raises(IntegrityError):
        await create_mediafile(session, metadata)

@pytest.mark.asyncio
async def test_create_mediafile_size_bytes_not_nullable(session):
    metadata = image_metadata()
    metadata.size_bytes = None

    with pytest.raises(IntegrityError):
        await create_mediafile(session, metadata)