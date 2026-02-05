from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.orm import joinedload

from app.db.tables import MediaFile, MediaType, MediaImage, MediaVideo, MediaLivePhoto
from app.utils.metadata.models import MediaMetadata


# MediaFile
async def create_mediafile(session : AsyncSession, metadata : MediaMetadata) -> MediaFile:
    media = MediaFile(
        path=str(metadata.path.parent),
        filename=metadata.filename,
        size_bytes=metadata.size_bytes,
        created_at_fs=metadata.created_at_fs,
        taken_at=metadata.taken_at,
        sha256=metadata.sha256,
        width=metadata.width,
        height=metadata.height,
        gps_lat=metadata.gps_lat,
        gps_lon=metadata.gps_lon,
        altitude=metadata.altitude,
    )

    if metadata.phash is not None:
        media.image = MediaImage(phash=metadata.phash)
        media.media_type = MediaType.image
    elif metadata.duration is not None:
        media.video = MediaVideo(duration=metadata.duration)
        media.media_type = MediaType.video
    else:
        raise ValueError("Cannot determine media type: no phash and no duration")

    session.add(media)
    await session.flush()
    await session.refresh(media, ["image", "video"])
    return media


async def get_mediafile(session : AsyncSession, id : int) -> MediaFile:
    result = await session.execute(
        select(MediaFile)
        .options(
            joinedload(MediaFile.image),
            joinedload(MediaFile.video)
        )
        .where(MediaFile.id == id)
    )
    return result.scalar_one_or_none()

async def get_mediafiles(session : AsyncSession) -> list[MediaFile]:
    result = await session.execute(
        select(MediaFile)
    )
    return result.scalars().all()

    
#MediaLivePhoto
async def create_live_photo(session : AsyncSession, image_id : int, video_id : int):
    live_photo = MediaLivePhoto(image_media_id=image_id, video_media_id=video_id)
    session.add(live_photo)
    await session.flush()
    return live_photo

async def get_live_photo(session : AsyncSession, live_photo_id : int):
    result = await session.execute(
        select(MediaLivePhoto)
        .options(joinedload(MediaLivePhoto.image_media_file), joinedload(MediaLivePhoto.video_media_file))
        .where(MediaLivePhoto.id == live_photo_id)
    )
    return result.scalar_one_or_none()