import pathlib
import sys


def main() -> int:
    if len(sys.argv) != 3:
        sys.stderr.write("Uzycie: gen_secrets.py secrets.env secrets.h\n")
        return 1

    source = pathlib.Path(sys.argv[1])
    dest = pathlib.Path(sys.argv[2])
    if not source.is_file():
        sys.stderr.write("Brak pliku secrets.env. Skopiuj secrets.env.example.\n")
        return 1

    values = {}
    for raw in source.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        values[key.strip()] = value

    required = ("WIFI_SSID", "WIFI_PASSWORD", "PANEL_PASSWORD")
    missing = [key for key in required if key not in values]
    if missing:
        sys.stderr.write("Brak pol w secrets.env: " + ", ".join(missing) + "\n")
        return 1
    if values["WIFI_SSID"].strip() == "" or values["PANEL_PASSWORD"] == "":
        sys.stderr.write("WIFI_SSID i PANEL_PASSWORD nie moga byc puste.\n")
        return 1

    def esc(text: str) -> str:
        return text.replace("\\", "\\\\").replace('"', '\\"')

    header = (
        "#pragma once\n"
        f'#define APP_WIFI_SSID "{esc(values["WIFI_SSID"])}"\n'
        f'#define APP_WIFI_PASSWORD "{esc(values["WIFI_PASSWORD"])}"\n'
        f'#define APP_PANEL_PASSWORD "{esc(values["PANEL_PASSWORD"])}"\n'
    )
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_text(header, encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
