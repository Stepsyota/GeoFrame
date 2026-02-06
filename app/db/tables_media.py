from sqlalchemy import Column, Integer, String, BIGINT, Double, CHAR, Enum, UniqueConstraint, ForeignKey, \
    TIMESTAMP, LargeBinary
from sqlalchemy.orm import relationship
import enum
from app.db.tables_common import Base


class MediaType(enum.Enum):
    image = "image"
    video = "video"

class MediaFile(Base):
    __tablename__ = "media_files"

    id = Column(Integer, primary_key=True)
    path = Column(String, index=True, nullable=False)
    filename = Column(String, index=True, nullable=False)
    size_bytes = Column(BIGINT, nullable=False)
    created_at_fs = Column(TIMESTAMP(timezone=True), nullable=False)
    taken_at = Column(TIMESTAMP(timezone=True), nullable=True)
    sha256 = Column(CHAR(64), unique=True, nullable=False)
    width = Column(Integer, nullable=False)
    height = Column(Integer, nullable=False)
    gps_lat = Column(Double, nullable=True)
    gps_lon = Column(Double, nullable=True)
    altitude = Column(Double, nullable=True)
    media_type = Column(Enum(MediaType), nullable=False)

    image = relationship(
        "MediaImage",
        uselist=False,
        back_populates="media",
        cascade="all, delete-orphan"
    )
    video = relationship(
        "MediaVideo",
        uselist=False,
        back_populates="media",
        cascade="all, delete-orphan"
    )

    __table_args__ = (
        UniqueConstraint("path", "filename", name= "unique_path_filename"),
    )

class MediaImage(Base):
    __tablename__ = "media_images"

    media_id = Column(
        Integer,
        ForeignKey("media_files.id", ondelete="CASCADE"),
        primary_key=True
    )
    phash = Column(LargeBinary(8), index=True, nullable=False)

    media = relationship("MediaFile", back_populates="image")


class MediaVideo(Base):
    __tablename__ = "media_videos"

    media_id = Column(
        Integer,
        ForeignKey("media_files.id", ondelete="CASCADE"),
        primary_key=True
    )
    duration = Column(Double, nullable=False)

    media = relationship("MediaFile", back_populates="video")


class MediaLivePhoto(Base):
    __tablename__ = "media_live_photos"

    id = Column(Integer, primary_key=True)

    image_media_id = Column(
        Integer, ForeignKey("media_files.id"), nullable=False
    )

    video_media_id = Column(
        Integer, ForeignKey("media_files.id"), nullable=False
    )

    image_media_file = relationship(
        "MediaFile",
        foreign_keys=[image_media_id]
    )

    video_media_file = relationship(
        "MediaFile",
        foreign_keys=[video_media_id]
    )

    __table_args__ = (
        UniqueConstraint("image_media_id", "video_media_id", name= "unique_live_photo_pair"),
    )