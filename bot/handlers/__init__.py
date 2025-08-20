from .basic import hello, balls
from .groups import register_group, list_groups
from .paths import set_path, get_path
from .media import scan_directory, send_not_compressed_photo

from telegram.ext import CommandHandler

def register_handlers(app):
    handlers = [
        ("hello", hello),
        ("balls", balls),
        ("register", register_group),
        ("list_groups", list_groups),
        ("set_path", set_path),
        ("get_path", get_path),
        ("scan_directory", scan_directory),
        ("send_not_compressed_photo", send_not_compressed_photo),
    ]
    for command, function in handlers:
        app.add_handler(CommandHandler(command, function))