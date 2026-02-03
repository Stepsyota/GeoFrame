from app.db.database import async_session_factory
from app.db.tables import MediaFile
from app.utils.scanner import scan_directory
from app.config import PHOTOS_DIR
from app.db.crud import create_file, get_files
from app.utils.metadata.extractor import extract_meta
import asyncio
from pathlib import Path


async def add_file_to_db(file : Path):
    async with async_session_factory() as session:
        async with session.begin():
            await create_file(session, extract_meta(file))

async def add_files_to_db():
    files = scan_directory(PHOTOS_DIR)
    for file in files:
        await add_file_to_db(file)

async def get_files_from_db() -> list[MediaFile]:
    async with async_session_factory() as session:
        files = await get_files(session)
        return files

#asyncio.run(add_files_to_db())
#print(asyncio.run(get_files_from_db()))


