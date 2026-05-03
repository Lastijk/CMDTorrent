#!/usr/bin/env python3
"""Проверка SHA1 первых N% частей скачанного файла по метаданным .torrent."""

from __future__ import annotations

import argparse
import binascii
import hashlib
import sys

import torrent_parser as tp


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Сверяет SHA1 кусков файла с полем pieces из торрента (первые percent% частей).",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
        epilog="Пример: python checksum.py -p 5 debian.iso.torrent ./debian.iso",
    )
    parser.add_argument(
        "-p",
        "--percent",
        dest="percent",
        type=int,
        default=100,
        metavar="N",
        help="Доля частей от общего числа (целое %%), считается как len(pieces)*N//100",
    )
    parser.add_argument(
        "torrent_file",
        metavar="FILE.torrent",
        help="Путь к .torrent",
    )
    parser.add_argument(
        "downloaded_file",
        metavar="OUTPUT",
        help="Путь к скачанному файлу на диске",
    )
    parser.add_argument(
        "-q",
        "--quiet",
        action="store_true",
        help="Только итог: без построчного вывода по кускам",
    )
    return parser


def main() -> int:
    args = build_parser().parse_args()
    if not (0 <= args.percent <= 100):
        print("error: -p / --percent must be between 0 and 100", file=sys.stderr)
        return 2

    try:
        data = tp.parse_torrent_file(args.torrent_file)
    except Exception as e:
        print(f"error: cannot read torrent: {e}", file=sys.stderr)
        return 2

    piece_hashes = data["info"]["pieces"]
    piece_length = data["info"]["piece length"]
    total = len(piece_hashes)
    pieces_to_check = total * args.percent // 100

    if pieces_to_check == 0:
        print(
            f"error: {args.percent}% of {total} pieces is 0; increase -p or use a larger torrent",
            file=sys.stderr,
        )
        return 2

    if not args.quiet:
        print(f"torrent : {args.torrent_file}")
        print(f"file    : {args.downloaded_file}")
        print(f"pieces  : {total} × {piece_length} bytes (piece length)")
        print(f"check   : first {pieces_to_check} piece(s) ({args.percent}% of total, integer division)")

    piece_id = 0
    try:
        with open(args.downloaded_file, "rb") as f:
            while piece_id < pieces_to_check:
                piece_bytes = f.read(piece_length)
                if len(piece_bytes) == 0:
                    raise ValueError("unexpected EOF: file shorter than expected for checked pieces")

                file_piece_hash = binascii.hexlify(hashlib.sha1(piece_bytes).digest()).decode()
                expected = piece_hashes[piece_id]
                if file_piece_hash != expected:
                    print(
                        f"error: piece #{piece_id}: got {file_piece_hash}, expected {expected}",
                        file=sys.stderr,
                    )
                    return 1
                if not args.quiet:
                    print(f"  ok  #{piece_id}")
                piece_id += 1
    except OSError as e:
        print(f"error: cannot read output file: {e}", file=sys.stderr)
        return 2

    if piece_id != pieces_to_check:
        print(
            f"error: checked {piece_id} pieces, expected {pieces_to_check}",
            file=sys.stderr,
        )
        return 1

    if not args.quiet:
        print(f"result: all {pieces_to_check} piece hashes match")
    else:
        print(f"OK {pieces_to_check}/{total} pieces")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
