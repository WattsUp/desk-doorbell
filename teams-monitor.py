#!/bin/python3

import time
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


def main() -> None:
    print("Team monitor for desk doorbell")

    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-p", "--port", metavar="COM", help="Serial port to desk doorbell"
    )

    args = parser.parse_args()

    CONFIG["port"] = args.port
    # Test
    send_rgb("FF000000")
    time.sleep(0.5)
    send_rgb("00FF0000")
    time.sleep(0.5)
    send_rgb("0000FF00")
    time.sleep(0.5)
    send_rgb("000000FF")
    time.sleep(0.5)
    send_rgb("00000000")

    menu_options = (
        ("Go away", None, go_away),
        ("Y'all okay now", None, yall_okay),
        ("Notify", None, notify),
    )

    with SysTrayIcon(None, "Desk doorbell", menu_options) as systray:
        while systray._hwnd is None:
            # Wait for systray to be created
            time.sleep(0.01)
        yall_okay(systray)

        while systray._hwnd is not None:
            time.sleep(1)


if __name__ == "__main__":
    main()
