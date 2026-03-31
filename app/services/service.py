from sqlalchemy.ext.asyncio import AsyncSession

from app.db.crud_telegram import create_topic
from app.db.database import async_session_factory
from app.db.tables_media import MediaFile
from app.db.tables_telegram import TopicKind
from app.utils.scanner import scan_directory
from app.config import PHOTOS_DIR
from app.db.crud_media import create_mediafile, get_mediafiles
from app.utils.metadata.extractor import extract_meta
import asyncio
from pathlib import Path


async def add_file_to_db(file : Path):
    async with async_session_factory() as session:
        # if metadata.phash is not None:
        #     media.image = MediaImage(phash=metadata.phash)
        #     media.media_type = MediaType.image
        # elif metadata.duration is not None:
        #     media.video = MediaVideo(duration=metadata.duration)
        #     media.media_type = MediaType.video
        # else:
        #     raise ValueError("Cannot determine media type: no phash and no duration")
        await create_mediafile(session, extract_meta(file))

async def add_files_to_db():
    files = scan_directory(PHOTOS_DIR)
    for file in files:
        await add_file_to_db(file)

async def get_files_from_db() -> list[MediaFile]:
    async with async_session_factory() as session:
        files = await get_mediafiles(session)
        return files

asyncio.run(add_files_to_db())
#print(asyncio.run(get_files_from_db()))

async def create_topic_and_add_to_db(name : str, kind : TopicKind):
    async with async_session_factory() as session:
        chat_id = ...
        await create_topic(session, name=name, kind=kind, chat_id=chat_id)





