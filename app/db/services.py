from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession
from app.db.tables import MediaFile, MediaType, MediaImage, MediaVideo
from app.utils.metadata.models import MediaMetadata


async def get_files(session : AsyncSession) -> list[MediaFile]:
    result = await session.execute(
        select(MediaFile)
    )
    return result.scalars().all()

async def create_file(session : AsyncSession, metadata : MediaMetadata) -> MediaFile:
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
        media_type=(
            MediaType.video if metadata.duration is not None
            else MediaType.image

        )
    )
    session.add(media)
    await session.flush()

    if metadata.phash is not None:
        session.add(
            MediaImage(
                media_id = media.id,
                phash = metadata.phash
            )
        )
    if metadata.duration is not None:
        session.add(
            MediaVideo(
                media_id = media.id,
                duration = metadata.duration
            )
        )
    return media