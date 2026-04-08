from __future__ import annotations

import os
import socket
import struct
import sys
from dataclasses import dataclass
from typing import Optional, Tuple

from .codec import decode_payload, encode_payload
from .value import Error, NetValue


def _parse_endpoint(endpoint: str) -> Tuple[str, int]:
    host, sep, port_text = endpoint.rpartition(":")
    if not sep:
        raise Error(f"Invalid endpoint: {endpoint}")
    try:
        port = int(port_text)
    except ValueError as exc:
        raise Error(f"Invalid endpoint: {endpoint}") from exc
    if port < 1 or port > 65535:
        raise Error(f"Port out of range: {port}")
    return host, port


def _recv_exact_socket(sock: socket.socket, size: int) -> bytes:
    chunks = bytearray()
    while len(chunks) < size:
        chunk = sock.recv(size - len(chunks))
        if not chunk:
            raise Error("socket recv failed")
        chunks.extend(chunk)
    return bytes(chunks)


def _send_all_fd(fd: int, data: bytes) -> None:
    view = memoryview(data)
    offset = 0
    while offset < len(view):
        written = os.write(fd, view[offset:])
        if written <= 0:
            raise Error("pipe write failed")
        offset += written


def _recv_exact_fd(fd: int, size: int) -> bytes:
    chunks = bytearray()
    while len(chunks) < size:
        chunk = os.read(fd, size - len(chunks))
        if not chunk:
            raise Error("pipe read failed")
        chunks.extend(chunk)
    return bytes(chunks)


if sys.platform == "win32":
    import ctypes
    from ctypes import wintypes

    _kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)

    INVALID_HANDLE_VALUE = ctypes.c_void_p(-1).value
    ERROR_PIPE_CONNECTED = 535
    GENERIC_READ = 0x80000000
    GENERIC_WRITE = 0x40000000
    OPEN_EXISTING = 3
    PIPE_ACCESS_DUPLEX = 0x00000003
    PIPE_TYPE_BYTE = 0x00000000
    PIPE_READMODE_BYTE = 0x00000000
    PIPE_WAIT = 0x00000000

    _kernel32.CreateNamedPipeW.argtypes = [
        wintypes.LPCWSTR,
        wintypes.DWORD,
        wintypes.DWORD,
        wintypes.DWORD,
        wintypes.DWORD,
        wintypes.DWORD,
        wintypes.DWORD,
        wintypes.LPVOID,
    ]
    _kernel32.CreateNamedPipeW.restype = wintypes.HANDLE

    _kernel32.CreateFileW.argtypes = [
        wintypes.LPCWSTR,
        wintypes.DWORD,
        wintypes.DWORD,
        wintypes.LPVOID,
        wintypes.DWORD,
        wintypes.DWORD,
        wintypes.HANDLE,
    ]
    _kernel32.CreateFileW.restype = wintypes.HANDLE

    _kernel32.ConnectNamedPipe.argtypes = [wintypes.HANDLE, wintypes.LPVOID]
    _kernel32.ConnectNamedPipe.restype = wintypes.BOOL

    _kernel32.ReadFile.argtypes = [
        wintypes.HANDLE,
        wintypes.LPVOID,
        wintypes.DWORD,
        ctypes.POINTER(wintypes.DWORD),
        wintypes.LPVOID,
    ]
    _kernel32.ReadFile.restype = wintypes.BOOL

    _kernel32.WriteFile.argtypes = [
        wintypes.HANDLE,
        wintypes.LPCVOID,
        wintypes.DWORD,
        ctypes.POINTER(wintypes.DWORD),
        wintypes.LPVOID,
    ]
    _kernel32.WriteFile.restype = wintypes.BOOL

    _kernel32.FlushFileBuffers.argtypes = [wintypes.HANDLE]
    _kernel32.FlushFileBuffers.restype = wintypes.BOOL

    _kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
    _kernel32.CloseHandle.restype = wintypes.BOOL

    def _send_all_handle(handle: int, data: bytes) -> None:
        view = memoryview(data)
        offset = 0
        while offset < len(view):
            written = wintypes.DWORD()
            ok = _kernel32.WriteFile(handle, view[offset:].tobytes(), len(view) - offset, ctypes.byref(written), None)
            if not ok or written.value == 0:
                raise Error(f"pipe write failed (err={ctypes.get_last_error()})")
            offset += int(written.value)

    def _recv_exact_handle(handle: int, size: int) -> bytes:
        out = bytearray(size)
        offset = 0
        while offset < size:
            got = wintypes.DWORD()
            ok = _kernel32.ReadFile(handle, (ctypes.c_char * (size - offset)).from_buffer(out, offset), size - offset, ctypes.byref(got), None)
            if not ok or got.value == 0:
                raise Error(f"pipe read failed (err={ctypes.get_last_error()})")
            offset += int(got.value)
        return bytes(out)


class TcpSocket:
    def __init__(self, sock: socket.socket):
        self._sock = sock

    @classmethod
    def connect(cls, endpoint: str, timeout_seconds: int = 3) -> "TcpSocket":
        if timeout_seconds < 0:
            raise Error("timeout_seconds must be non-negative")
        host, port = _parse_endpoint(endpoint)
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(timeout_seconds)
        try:
            sock.connect((host, port))
        except OSError as exc:
            sock.close()
            raise Error(f"connect failed: {endpoint} ({exc})") from exc
        sock.settimeout(None)
        return cls(sock)

    def is_open(self) -> bool:
        return self._sock.fileno() >= 0

    def send_raw(self, data: bytes | str) -> None:
        payload = data.encode("utf-8") if isinstance(data, str) else bytes(data)
        self._sock.sendall(payload)

    def recv_raw_exact(self, size: int) -> bytes:
        if size < 0:
            raise Error("recv_raw_exact size must be non-negative")
        return _recv_exact_socket(self._sock, size)

    def send_frame(self, payload: bytes | str) -> None:
        data = payload.encode("utf-8") if isinstance(payload, str) else bytes(payload)
        self._sock.sendall(struct.pack("<I", len(data)) + data)

    def recv_frame(self) -> bytes:
        header = _recv_exact_socket(self._sock, 4)
        size = struct.unpack("<I", header)[0]
        return _recv_exact_socket(self._sock, size)

    def send_net(self, value: NetValue) -> None:
        self.send_frame(encode_payload(value))

    def recv_net(self) -> NetValue:
        return decode_payload(self.recv_frame())

    def peer_addr(self) -> str:
        host, port = self._sock.getpeername()
        return f"{host}:{port}"

    def set_timeout_seconds(self, seconds: int) -> None:
        if seconds < 0:
            raise Error("tcp.set_timeout requires non-negative seconds")
        self._sock.settimeout(seconds)

    def close(self) -> None:
        try:
            self._sock.close()
        except OSError:
            pass

    def __enter__(self) -> "TcpSocket":
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        self.close()


class TcpServer:
    def __init__(self, sock: socket.socket):
        self._sock = sock

    @classmethod
    def listen(cls, endpoint: str, backlog: int = 16) -> "TcpServer":
        if backlog < 0:
            raise Error("tcp_listen backlog must be non-negative")
        host, port = _parse_endpoint(endpoint)
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        try:
            sock.bind((host or "0.0.0.0", port))
            sock.listen(backlog)
        except OSError as exc:
            sock.close()
            raise Error(f"listen failed: {endpoint} ({exc})") from exc
        return cls(sock)

    def accept(self) -> TcpSocket:
        client, _ = self._sock.accept()
        return TcpSocket(client)

    def addr(self) -> str:
        host, port = self._sock.getsockname()
        return f"{host}:{port}"

    def close(self) -> None:
        try:
            self._sock.close()
        except OSError:
            pass

    def __enter__(self) -> "TcpServer":
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        self.close()


@dataclass
class _PipeBackend:
    kind: str
    read_fd: Optional[int] = None
    write_fd: Optional[int] = None
    handle: Optional[int] = None


class PipeChannel:
    def __init__(self, backend: _PipeBackend, name: str = "", named: bool = False):
        self._backend = backend
        self._name = name
        self._named = named

    @classmethod
    def anonymous(cls) -> "PipeChannel":
        read_fd, write_fd = os.pipe()
        return cls(_PipeBackend(kind="fd", read_fd=read_fd, write_fd=write_fd))

    @classmethod
    def open_named(cls, name: str) -> "PipeChannel":
        if sys.platform == "win32":
            path = rf"\\.\pipe\{name}"
            handle = _kernel32.CreateNamedPipeW(
                path,
                PIPE_ACCESS_DUPLEX,
                PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                1,
                4096,
                4096,
                0,
                None,
            )
            if handle == INVALID_HANDLE_VALUE:
                handle = _kernel32.CreateFileW(path, GENERIC_READ | GENERIC_WRITE, 0, None, OPEN_EXISTING, 0, None)
                if handle == INVALID_HANDLE_VALUE:
                    raise Error(f"Cannot open pipe: {name} (err={ctypes.get_last_error()})")
            else:
                ok = _kernel32.ConnectNamedPipe(handle, None)
                err = ctypes.get_last_error()
                if not ok and err != ERROR_PIPE_CONNECTED:
                    _kernel32.CloseHandle(handle)
                    raise Error(f"ConnectNamedPipe failed (err={err})")
            return cls(_PipeBackend(kind="handle", handle=int(handle)), name=name, named=True)

        path = f"/tmp/sl_pipe_{name}"
        try:
            os.mkfifo(path, 0o666)
        except FileExistsError:
            pass
        try:
            fd = os.open(path, os.O_RDWR)
        except OSError as exc:
            raise Error(f"Cannot open pipe: {name} ({exc})") from exc
        return cls(_PipeBackend(kind="fd", read_fd=fd, write_fd=fd), name=name, named=True)

    def is_open(self) -> bool:
        if self._backend.kind == "fd":
            return self._backend.read_fd is not None and self._backend.write_fd is not None
        return self._backend.handle is not None

    def is_named(self) -> bool:
        return self._named

    def name(self) -> Optional[str]:
        return self._name if self._named else None

    def _write_all(self, data: bytes) -> None:
        if self._backend.kind == "fd":
            assert self._backend.write_fd is not None
            _send_all_fd(self._backend.write_fd, data)
        else:
            assert self._backend.handle is not None
            _send_all_handle(self._backend.handle, data)

    def _read_exact(self, size: int) -> bytes:
        if self._backend.kind == "fd":
            assert self._backend.read_fd is not None
            return _recv_exact_fd(self._backend.read_fd, size)
        assert self._backend.handle is not None
        return _recv_exact_handle(self._backend.handle, size)

    def send_frame(self, payload: bytes | str) -> None:
        data = payload.encode("utf-8") if isinstance(payload, str) else bytes(payload)
        self._write_all(struct.pack("<I", len(data)))
        self._write_all(data)

    def recv_frame(self) -> bytes:
        header = self._read_exact(4)
        size = struct.unpack("<I", header)[0]
        return self._read_exact(size)

    def send_net(self, value: NetValue) -> None:
        self.send_frame(encode_payload(value))

    def recv_net(self) -> NetValue:
        return decode_payload(self.recv_frame())

    def flush(self) -> None:
        if self._backend.kind == "fd":
            return
        if sys.platform == "win32" and self._backend.handle is not None:
            _kernel32.FlushFileBuffers(self._backend.handle)

    def close(self) -> None:
        if self._backend.kind == "fd":
            read_fd = self._backend.read_fd
            write_fd = self._backend.write_fd
            self._backend.read_fd = None
            self._backend.write_fd = None
            if read_fd is not None:
                try:
                    os.close(read_fd)
                except OSError:
                    pass
            if write_fd is not None and write_fd != read_fd:
                try:
                    os.close(write_fd)
                except OSError:
                    pass
            if self._named and sys.platform != "win32":
                try:
                    os.unlink(f"/tmp/sl_pipe_{self._name}")
                except OSError:
                    pass
            return

        handle = self._backend.handle
        self._backend.handle = None
        if handle is not None:
            _kernel32.CloseHandle(handle)

    def __enter__(self) -> "PipeChannel":
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        self.close()
