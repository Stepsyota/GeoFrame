import logging
import re

class SensitiveDataFilter(logging.Filter):
    token_pattern = re.compile(r'\d{8,10}:[A-Za-z0-9_-]{35,}')

    def filter(self, record: logging.LogRecord) -> bool:
        msg = record.getMessage()
        sanitized = self.token_pattern.sub("[TOKEN]", msg)
        if sanitized != msg:
            record.msg = sanitized
            record.args = ()
        return True

def setup_logger() -> logging.Logger:
    logger = logging.getLogger()
    logger.setLevel(logging.INFO)

    formatter = logging.Formatter(
        fmt="%(asctime)s - %(name)s - %(levelname)s - %(message)s",
        datefmt="%Y-%m-%d %H:%M:%S"
    )

    console_handler = logging.StreamHandler()
    console_handler.setFormatter(formatter)
    console_handler.addFilter(SensitiveDataFilter())

    logger.addHandler(console_handler)
    return logger