#!/bin/python3

from collections.abc import Generator
import io
import os
import time
from pathlib import Path
import argparse
from infi.systray import SysTrayIcon
import serial

CONFIG = {
    "port": None,
    "go_away": True,
}


def send(cmd: str) -> None:
    with serial.Serial(CONFIG["port"], timeout=1) as s:
        s.write(cmd.encode())


def send_rgb(rgb: str) -> None:
    print(f"Sending RGB: {rgb}")
    send(f"#{rgb}\n")


def send_notification() -> None:
    send("!\n")


def go_away(_: SysTrayIcon) -> None:
    print("Changing to go away mode")
    send_rgb("FF000000")
    CONFIG["go_away"] = True


def yall_okay(_: SysTrayIcon) -> None:
    print("Changing to y'all okay mode")
    send_rgb("00FF0000")
    CONFIG["go_away"] = False


def notify(_: SysTrayIcon) -> None:
    print("Sending notification")
    send_notification()


def parse_teams_log(lines: list[str]) -> tuple[str | None, bool]:
    do_notify = False
    for line in lines[::-1]:
        if "StatusIndicatorStateService: Added" not in line:
            continue
        if "Added NewActivity" in line:
            do_notify = True
        elif "Added Available" in line:
            return ("00FF0000", do_notify)
        elif "Added Busy" in line:
            return ("FF000000", do_notify)
        elif "Added InAMeeting" in line:
            return ("FF000000", do_notify)
        elif "Added Away" in line:
            return ("FFFF0000", do_notify)
        elif "Added Offline" in line:
            return ("00000000", do_notify)
        else:
            print(f"Unknown line: {line}")

    return (None, do_notify)


def follow(file: io.TextIOWrapper) -> Generator[str, None, None]:
    file.seek(0, os.SEEK_END)
    while True:
        line = file.readline()
        yield line


def main() -> None:
    print("Team monitor for desk doorbell")

    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-p", "--port", metavar="COM", help="Serial port to desk doorbell"
    )

    args = parser.parse_args()

    CONFIG["port"] = args.port

    path = Path("~/AppData/Roaming/Microsoft/Teams/logs.txt").expanduser()

    menu_options = (
        ("Go away", None, go_away),
        ("Y'all okay now", None, yall_okay),
        ("Notify", None, notify),
    )

    with path.open() as file:
        with SysTrayIcon(None, "Desk doorbell", menu_options) as systray:
            while systray._hwnd is None:
                # Wait for systray to be created
                time.sleep(0.01)
            yall_okay(systray)

            lines = file.readlines()
            color, do_notify = parse_teams_log(lines)
            if color:
                send_rgb(color)
            if do_notify:
                send_notification()

            for line in follow(file):
                if systray._hwnd is None:
                    return
                elif not line:
                    time.sleep(0.1)
                else:
                    color, do_notify = parse_teams_log([line])
                    if color:
                        send_rgb(color)
                    if do_notify:
                        send_notification()


if __name__ == "__main__":
    main()
