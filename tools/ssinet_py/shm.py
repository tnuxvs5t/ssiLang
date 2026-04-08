from __future__ import annotations

import mmap
import os
import struct
import sys
import time
from typing import Optional

from .codec import decode_payload, encode_payload
from .value import Error, NetValue


if sys.platform == "win32":
    import ctypes
    from ctypes import wintypes

    _kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    _kernel32.CreateMutexW.argtypes = [wintypes.LPVOID, wintypes.BOOL, wintypes.LPCWSTR]
    _kernel32.CreateMutexW.restype = wintypes.HANDLE
    _kernel32.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
    _kernel32.WaitForSingleObject.restype = wintypes.DWORD
    _kernel32.ReleaseMutex.argtypes = [wintypes.HANDLE]
    _kernel32.ReleaseMutex.restype = wintypes.BOOL
    _kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
    _kernel32.CloseHandle.restype = wintypes.BOOL
    WAIT_OBJECT_0 = 0x00000000
    WAIT_ABANDONED = 0x00000080
    INFINITE = 0xFFFFFFFF
else:
    import ctypes
    import ctypes.util

    _libc_name = ctypes.util.find_library("c")
    if _libc_name is None:
        raise RuntimeError("Could not locate libc for shm support")
    _libc = ctypes.CDLL(_libc_name, use_errno=True)

    _libc.shm_open.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_int]
    _libc.shm_open.restype = ctypes.c_int
    _libc.ftruncate.argtypes = [ctypes.c_int, ctypes.c_longlong]
    _libc.ftruncate.restype = ctypes.c_int
    _libc.close.argtypes = [ctypes.c_int]
    _libc.close.restype = ctypes.c_int
    _libc.shm_unlink.argtypes = [ctypes.c_char_p]
    _libc.shm_unlink.restype = ctypes.c_int

    O_CREAT = os.O_CREAT
    O_EXCL = os.O_EXCL
    O_RDWR = os.O_RDWR


class SharedMemory:
    def __init__(
        self,
        mapping: mmap.mmap,
        name: str,
        size: int,
        owner: bool = False,
        posix_fd: Optional[int] = None,
        win_mutex: Optional[int] = None,
    ):
        self._mapping = mapping
        self._name = name
        self._size = size
        self._owner = owner
        self._posix_fd = posix_fd
        self._win_mutex = win_mutex

    def _lock_addr(self) -> int:
        return ctypes.addressof(ctypes.c_char.from_buffer(self._mapping))

    def _lock(self) -> None:
        if sys.platform == "win32":
            if self._win_mutex is None:
                raise Error("Windows shm mutex not initialized")
            result = _kernel32.WaitForSingleObject(self._win_mutex, INFINITE)
            if result not in (WAIT_OBJECT_0, WAIT_ABANDONED):
                raise Error(f"WaitForSingleObject failed (err={ctypes.get_last_error()})")
            return

        while True:
            self._mapping.seek(0)
            current = struct.unpack("<I", self._mapping.read(4))[0]
            if current == 0:
                self._mapping.seek(0)
                self._mapping.write(struct.pack("<I", 1))
                self._mapping.flush()
                self._mapping.seek(0)
                if struct.unpack("<I", self._mapping.read(4))[0] == 1:
                    return
            time.sleep(0)

    def _unlock(self) -> None:
        if sys.platform == "win32":
            if self._win_mutex is None:
                raise Error("Windows shm mutex not initialized")
            if not _kernel32.ReleaseMutex(self._win_mutex):
                raise Error(f"ReleaseMutex failed (err={ctypes.get_last_error()})")
            return

        self._mapping.seek(0)
        self._mapping.write(struct.pack("<I", 0))
        self._mapping.flush()

    @classmethod
    def open(cls, name: str, size: int = 4096) -> "SharedMemory":
        if not name:
            raise Error("shm name must not be empty")
        if size < 8:
            raise Error("shm size must be at least 8 bytes")

        if sys.platform == "win32":
            mapping = mmap.mmap(-1, size, tagname=f"Local\\sl_shm_{name}", access=mmap.ACCESS_WRITE)
            mutex = _kernel32.CreateMutexW(None, False, f"Local\\sl_shm_lock_{name}")
            if not mutex:
                mapping.close()
                raise Error(f"CreateMutexW failed (err={ctypes.get_last_error()})")
            return cls(mapping, name=name, size=size, owner=False, win_mutex=int(mutex))

        shm_name = f"/sl_shm_{name}".encode("utf-8")
        owner = False
        fd = _libc.shm_open(shm_name, O_CREAT | O_EXCL | O_RDWR, 0o666)
        if fd >= 0:
            owner = True
            if _libc.ftruncate(fd, size) != 0:
                err = ctypes.get_errno()
                _libc.close(fd)
                _libc.shm_unlink(shm_name)
                raise Error(f"ftruncate failed ({os.strerror(err)})")
        else:
            err = ctypes.get_errno()
            if err != getattr(os, "EEXIST", 17):
                raise Error(f"shm_open failed ({os.strerror(err)})")
            fd = _libc.shm_open(shm_name, O_RDWR, 0o666)
            if fd < 0:
                err = ctypes.get_errno()
                raise Error(f"shm_open failed ({os.strerror(err)})")

        mapping = mmap.mmap(fd, size, access=mmap.ACCESS_WRITE)
        return cls(mapping, name=name, size=size, owner=owner, posix_fd=fd)

    def is_open(self) -> bool:
        return self._mapping is not None

    def size(self) -> int:
        return self._size

    def name(self) -> str:
        return self._name

    def owner(self) -> bool:
        return self._owner

    def store_payload(self, payload: bytes | str) -> None:
        data = payload.encode("utf-8") if isinstance(payload, str) else bytes(payload)
        if len(data) + 8 > self._size:
            raise Error("shm data too large")
        self._lock()
        try:
            self._mapping.seek(4)
            self._mapping.write(struct.pack("<I", len(data)))
            self._mapping.write(data)
            self._mapping.flush()
        finally:
            self._unlock()

    def load_payload(self) -> Optional[bytes]:
        self._lock()
        try:
            self._mapping.seek(4)
            size = struct.unpack("<I", self._mapping.read(4))[0]
            if size == 0:
                return None
            return self._mapping.read(size)
        finally:
            self._unlock()

    def store_net(self, value: NetValue) -> None:
        self.store_payload(encode_payload(value))

    def load_net(self) -> Optional[NetValue]:
        payload = self.load_payload()
        if payload is None:
            return None
        return decode_payload(payload)

    def clear(self) -> None:
        self._lock()
        try:
            self._mapping.seek(4)
            self._mapping.write(struct.pack("<I", 0))
            self._mapping.flush()
        finally:
            self._unlock()

    def close(self) -> None:
        if self._mapping is None:
            return

        mapping = self._mapping
        self._mapping = None  # type: ignore[assignment]
        mapping.close()

        if sys.platform == "win32" and self._win_mutex is not None:
            handle = self._win_mutex
            self._win_mutex = None
            _kernel32.CloseHandle(handle)

        if sys.platform != "win32" and self._posix_fd is not None:
            fd = self._posix_fd
            self._posix_fd = None
            _libc.close(fd)
            if self._owner:
                _libc.shm_unlink(f"/sl_shm_{self._name}".encode("utf-8"))

    def __enter__(self) -> "SharedMemory":
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        self.close()
