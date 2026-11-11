"""Write docs/install-qr.png: a QR code of the tag-pinned release download URL.

Usage:  py -3 tools/make_qr.py v1.0.0
The link names an exact tag, never "latest", so an old QR always installs the
version it was made for. Re-run for every release.
"""
import pathlib
import sys

import qrcode

REPO = "stevenjc2009-byte/sumvita"
ASSET = "Sumvita.vpk"


def main() -> int:
    if len(sys.argv) != 2 or not sys.argv[1].startswith("v"):
        print("usage: make_qr.py vX.Y.Z")
        return 2
    tag = sys.argv[1]
    url = f"https://github.com/{REPO}/releases/download/{tag}/{ASSET}"
    qr = qrcode.QRCode(error_correction=qrcode.constants.ERROR_CORRECT_M, box_size=10, border=4)
    qr.add_data(url)
    qr.make(fit=True)
    out = pathlib.Path(__file__).resolve().parent.parent / "docs" / "install-qr.png"
    qr.make_image(fill_color="black", back_color="white").save(out)
    print(url)
    print(out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
