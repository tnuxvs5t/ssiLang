from __future__ import annotations

from typing import Union

from .value import Error, NetValue, ensure_supported, is_number, normalize_number, number_to_token


BytesLike = Union[bytes, bytearray, memoryview]


def _as_bytes(payload: Union[str, BytesLike]) -> bytes:
    if isinstance(payload, str):
        return payload.encode("utf-8")
    if isinstance(payload, memoryview):
        return payload.tobytes()
    return bytes(payload)


def _encode_inner(value: NetValue) -> bytes:
    if value is None:
        return b"Z\n"
    if isinstance(value, bool):
        return b"B1\n" if value else b"B0\n"
    if is_number(value):
        return b"N" + number_to_token(value).encode("ascii") + b"\n"
    if isinstance(value, str):
        raw = value.encode("utf-8")
        return b"S" + str(len(raw)).encode("ascii") + b":" + raw
    if isinstance(value, list):
        out = [b"L", str(len(value)).encode("ascii"), b"\n"]
        for item in value:
            out.append(_encode_inner(item))
        return b"".join(out)
    if isinstance(value, dict):
        out = [b"M", str(len(value)).encode("ascii"), b"\n"]
        for key, item in value.items():
            if not isinstance(key, str):
                raise Error("ssiLang %net maps require string keys")
            raw_key = key.encode("utf-8")
            out.extend((b"S", str(len(raw_key)).encode("ascii"), b":", raw_key, _encode_inner(item)))
        return b"".join(out)
    raise Error(f"Unsupported %net value type: {type(value)!r}")


def encode_payload(value: NetValue) -> bytes:
    ensure_supported(value)
    return b"V1\n" + _encode_inner(value)


class _Reader:
    def __init__(self, payload: Union[str, BytesLike]):
        self.data = _as_bytes(payload)
        self.pos = 0

    def eof(self) -> bool:
        return self.pos >= len(self.data)

    def read_byte(self) -> int:
        if self.eof():
            raise Error("Unexpected end of %net payload")
        out = self.data[self.pos]
        self.pos += 1
        return out

    def read_until(self, needle: int) -> bytes:
        idx = self.data.find(bytes((needle,)), self.pos)
        if idx < 0:
            raise Error("Malformed %net payload")
        out = self.data[self.pos:idx]
        self.pos = idx + 1
        return out

    def read_exact(self, size: int) -> bytes:
        if self.pos + size > len(self.data):
            raise Error("Truncated %net payload")
        out = self.data[self.pos:self.pos + size]
        self.pos += size
        return out


def _decode_inner(reader: _Reader) -> NetValue:
    tag = reader.read_byte()

    if tag == ord("N"):
        token = reader.read_until(ord("\n")).decode("ascii")
        try:
            return normalize_number(float(token))
        except ValueError as exc:
            raise Error("Malformed number in %net payload") from exc

    if tag == ord("S"):
        length_token = reader.read_until(ord(":")).decode("ascii")
        try:
            size = int(length_token)
        except ValueError as exc:
            raise Error("Malformed string length in %net payload") from exc
        return reader.read_exact(size).decode("utf-8")

    if tag == ord("B"):
        value = reader.read_byte()
        nl = reader.read_byte()
        if nl != ord("\n") or value not in (ord("0"), ord("1")):
            raise Error("Malformed bool in %net payload")
        return value == ord("1")

    if tag == ord("Z"):
        nl = reader.read_byte()
        if nl != ord("\n"):
            raise Error("Malformed null in %net payload")
        return None

    if tag == ord("L"):
        count_token = reader.read_until(ord("\n")).decode("ascii")
        try:
            count = int(count_token)
        except ValueError as exc:
            raise Error("Malformed list length in %net payload") from exc
        out = []
        for _ in range(count):
            out.append(_decode_inner(reader))
        return out

    if tag == ord("M"):
        count_token = reader.read_until(ord("\n")).decode("ascii")
        try:
            count = int(count_token)
        except ValueError as exc:
            raise Error("Malformed map length in %net payload") from exc
        out = {}
        for _ in range(count):
            key = _decode_inner(reader)
            if not isinstance(key, str):
                raise Error("Corrupted %net payload: map key is not a string")
            out[key] = _decode_inner(reader)
        return out

    raise Error(f"Unknown %net tag: {chr(tag)!r}")


def decode_payload(payload: Union[str, BytesLike]) -> NetValue:
    reader = _Reader(payload)
    if reader.data.startswith(b"V"):
        header = reader.read_until(ord("\n"))
        if header != b"V1":
            raise Error("Unsupported %net version header")
    value = _decode_inner(reader)
    if not reader.eof():
        raise Error("Trailing bytes after %net payload")
    return value
