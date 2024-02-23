#!/bin/python3

from collections.abc import Generator
import re
import io
import os
import time
from pathlib import Path
import argparse
from infi.systray import SysTrayIcon
import serial

RED = "FF000000"
GREEN = "00FF0000"
YELLOW = "FFA00000"
BLUE = "0000FF00"
OFF = "00000000"

CONFIG = {
    "port": None,
    "go_away": True,
    "color": OFF,
}


def send(cmd: str) -> None:
    try:
        with serial.Serial(CONFIG["port"], timeout=1) as s:
            s.write(cmd.encode())
    except serial.SerialException:
        print("Failed to connect to doorbell")


def send_rgb(rgb: str) -> None:
    print(f"Sending RGB: {rgb}")
    send(f"#{rgb}\n")


def send_notification() -> None:
    print("Sending notification")
    send("!\n")


def go_to_idle() -> None:
    print("Going to idle")
    send("I\n")


def go_away(_: SysTrayIcon) -> None:
    print("Changing to go away mode")
    CONFIG["go_away"] = True
    send_rgb(RED)


def yall_okay(_: SysTrayIcon) -> None:
    print("Changing to y'all okay mode")
    CONFIG["go_away"] = False
    send_rgb(CONFIG["color"])


def notify(_: SysTrayIcon) -> None:
    send_notification()


def status_to_color(s: str) -> str | None:
    if "Available" in s:
        return GREEN
    if "Busy" in s:
        return RED
    if "InAMeeting" in s:
        return RED
    if "OnThePhone" in s:
        return RED
    if "Presenting" in s:
        return RED
    if "Away" in s:
        return YELLOW
    if "BeRightBack" in s:
        return YELLOW
    if "Offline" in s:
        return OFF
    if "Unknown" not in s:
        print(f"Unknown line: {s}")
    return None


def parse_teams_log(lines: list[str]) -> tuple[str | None, bool | None]:
    do_notify: bool | None = None
    for line in lines[::-1]:
        if "StatusIndicatorStateService: Change status icon from NewActivity" in line:
            return (status_to_color(line.split(" to ")[1]), False)

        if "StatusIndicatorStateService: Added" not in line:
            continue
        elif "Added NewActivity" in line:
            do_notify = True
        else:
            word = re.search(r"Added (\w+) ", line).group(1)
            return (status_to_color(word), do_notify)

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
                CONFIG["color"] = color
                send_rgb(color)
            if do_notify is True:
                send_rgb(BLUE)
                send_notification()
            elif do_notify is False:
                go_to_idle()

            for line in follow(file):
                if systray._hwnd is None:
                    return
                elif not line:
                    time.sleep(0.1)
                else:
                    color, do_notify = parse_teams_log([line])
                    if color:
                        CONFIG["color"] = color
                        if not CONFIG["go_away"]:
                            send_rgb(color)
                    if do_notify is True:
                        send_rgb(BLUE)
                        send_notification()
                    elif do_notify is False:
                        go_to_idle()


if __name__ == "__main__":
    main()
