from aiogram import Router, F
from aiogram.filters import Command
from aiogram.types import Message, KeyboardButton, ReplyKeyboardMarkup, InlineKeyboardButton, InlineKeyboardMarkup, \
    ReplyKeyboardRemove
from aiogram.fsm.state import State, StatesGroup
from aiogram.fsm.context import FSMContext
from app.db.tables_telegram import TopicKind
from typing import Any

router = Router()


class CreateTopic(StatesGroup):
    title = State()
    confirm_title = State()
    type = State()



@router.message(Command("create_topic"))
async def command_create_topic(message : Message, state : FSMContext):
    await state.set_state(CreateTopic.title)
    await send_title_prompt(message)

async def send_title_prompt(message: Message, error : bool = False):
    text = "Enter the title of the topic:"
    if error:
        text = "Invalid title. Please enter the title again:"
    await message.answer(text=text, reply_markup=ReplyKeyboardRemove())

@router.message(CreateTopic.title)
async def process_title(message : Message, state : FSMContext):
    title = message.text.strip()
    if not title:
        await send_title_prompt(message, error=True)
        return

    await state.update_data(title=title)
    await go_to_confirm_title(message, state)

async def go_to_confirm_title(message: Message, state: FSMContext):
    data = await state.get_data()
    title = data.get("title", "<unknown>")
    await message.answer(
        text=f"You sure you want this title: {title}",
        reply_markup=ReplyKeyboardMarkup(
            keyboard=[[KeyboardButton(text="Yes"), KeyboardButton(text="No")]],
            resize_keyboard=True
        )
    )

@router.message(CreateTopic.confirm_title)
async def process_confirm_title(message : Message, state : FSMContext):
    text = message.text.casefold()
    if text == "yes":
        await go_to_type(message, state)

    elif text == "no":
        await state.set_state(CreateTopic.title)
        await send_title_prompt(message)
    else:
        await go_to_confirm_title(message, state)
        #await message.answer(text="Enter the title again", reply_markup=ReplyKeyboardRemove())



@router.message(CreateTopic.type)
async def process_type(message : Message, state : FSMContext):
    SUPPORTED_TYPES = ["Original", "Day photo"]
    if message.text not in SUPPORTED_TYPES:
        await message.answer(text="Unknown type")
        await state.set_state(CreateTopic.type)
        return

    data = await state.update_data(type=message.text)
    #await state.set_state(CreateTopic.type)
    await state.clear()
    await show_summary(message=message, data=data)


async def show_summary(message : Message, data : dict[str, Any]):
    title = data["title"]
    type = data["type"]
    await message.answer(
        f"So, you create topic with title {title} and type {type}",
        reply_markup=ReplyKeyboardRemove()
    )


@router.message(Command("cancel"))
@router.message(F.text.casefold() == "cancel")
async def cancel_handler(message: Message, state: FSMContext) -> None:
    """
    Allow user to cancel any action
    """
    current_state = await state.get_state()
    if current_state is None:
        return

    await state.clear()
    await message.answer(
        "Cancelled.",
        reply_markup=ReplyKeyboardRemove(),
    )





async def go_to_type(message: Message, state : FSMContext):
    await message.answer(
        text="Okay, now you need to choose type of topic:",
        reply_markup=ReplyKeyboardMarkup(
            keyboard=[
                [
                    KeyboardButton(text="Original"),
                    KeyboardButton(text="Day photo"),
                ],
            ],
            resize_keyboard=True
        ),
    )