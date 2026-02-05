import pytest_asyncio
from sqlalchemy import NullPool
from sqlalchemy.ext.asyncio import create_async_engine, async_sessionmaker
from os import getenv
from dotenv import load_dotenv
from app.db.tables_media import Base


load_dotenv()

TEST_DATABASE_URL = (
    f"postgresql+asyncpg://"
    f"{getenv('POSTGRESQL_USER_TEST')}:"
    f"{getenv('POSTGRESQL_PASSWORD_TEST')}@"
    f"{getenv('POSTGRESQL_HOST_TEST')}:"
    f"{getenv('POSTGRESQL_PORT_TEST')}/"
    f"{getenv('POSTGRESQL_DB_TEST')}"
)

@pytest_asyncio.fixture(scope="session", autouse=True)
async def engine_fixture():
    engine = create_async_engine(TEST_DATABASE_URL, echo=False, poolclass=NullPool)

    async with engine.begin() as conn:
        await conn.run_sync(Base.metadata.create_all)

    yield engine
    async with engine.begin() as conn:
        await conn.run_sync(Base.metadata.drop_all)

@pytest_asyncio.fixture
async def session(engine_fixture):
    async_session_factory = async_sessionmaker(bind=engine_fixture, expire_on_commit=False)

    async with async_session_factory() as session:
        async with session.begin():
            yield session
            await session.rollback()