#!/bin/python3

import time
from infi.systray import SysTrayIcon


def hello(systray: SysTrayIcon) -> None:
    print("Hello there")


def main() -> None:
    print("Team monitor for desk doorbell")

    menu_options = (("Say hello", None, hello),)

    with SysTrayIcon(None, "Desk doorbell", menu_options) as systray:
        while systray._hwnd is None:
            # Wait for systray to be created
            time.sleep(0.01)

        while systray._hwnd is not None:
            time.sleep(1)
            print("Running")


if __name__ == "__main__":
    main()
