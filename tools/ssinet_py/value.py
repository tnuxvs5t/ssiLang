from __future__ import annotations

import math
from typing import Dict, List, TypeAlias, Union


class Error(RuntimeError):
    pass


Scalar: TypeAlias = Union[None, bool, int, float, str]
NetValue: TypeAlias = Union[Scalar, List["NetValue"], Dict[str, "NetValue"]]


def is_number(value: object) -> bool:
    return isinstance(value, (int, float)) and not isinstance(value, bool)


def ensure_supported(value: NetValue) -> None:
    if value is None or isinstance(value, (bool, str)):
        return
    if is_number(value):
        return
    if isinstance(value, list):
        for item in value:
            ensure_supported(item)
        return
    if isinstance(value, dict):
        for key, item in value.items():
            if not isinstance(key, str):
                raise Error("ssiLang %net maps require string keys")
            ensure_supported(item)
        return
    raise Error(f"Unsupported %net value type: {type(value)!r}")


def number_to_token(value: Union[int, float]) -> str:
    number = float(value)
    if math.isfinite(number) and number == int(number) and abs(number) < 1e15:
        return str(int(number))
    return format(number, ".15g")


def normalize_number(value: float) -> Union[int, float]:
    if math.isfinite(value) and value == int(value):
        return int(value)
    return value


def debug_string(value: NetValue) -> str:
    if value is None:
        return "null"
    if isinstance(value, bool):
        return "true" if value else "false"
    if is_number(value):
        return number_to_token(value)
    if isinstance(value, str):
        return value
    if isinstance(value, list):
        parts = []
        for item in value:
            if isinstance(item, str):
                parts.append(f"\"{item}\"")
            else:
                parts.append(debug_string(item))
        return "[" + ", ".join(parts) + "]"
    if isinstance(value, dict):
        parts = []
        for key, item in value.items():
            if isinstance(item, str):
                parts.append(f"{key}: \"{item}\"")
            else:
                parts.append(f"{key}: {debug_string(item)}")
        return "{" + ", ".join(parts) + "}"
    raise Error(f"Unsupported %net value type: {type(value)!r}")
