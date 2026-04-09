#include "../../tools/ssinet.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <shellapi.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

using ssilang::net::Error;
using ssilang::net::MapEntry;
using ssilang::net::TcpServer;
using ssilang::net::TcpSocket;
using ssilang::net::Value;

namespace {

constexpr UINT WM_APP_RUN_TASK = WM_APP + 1;
constexpr wchar_t kWindowClassName[] = L"ssiLangWinguiWindow";
constexpr UINT_PTR kInternalRenderTimerId = 0x7ffffffeu;
constexpr UINT kInternalRenderTimerIntervalMs = 1;

struct UiTask {
    std::function<Value()> run;
    Value result = Value::null();
    std::string error;
    std::mutex mutex;
    std::condition_variable cv;
    bool done = false;
};

struct ProfileBucket {
    double total_ms = 0.0;
    double max_ms = 0.0;
    std::uint64_t calls = 0;
};

class HelperState;

struct WindowState {
    HelperState* owner = nullptr;
    int id = 0;
    HWND hwnd = nullptr;
    Value scene = Value::list();
    bool tracking_mouse = false;
    bool render_dirty = false;
    bool render_timer_armed = false;
};

Value make_ok(std::initializer_list<MapEntry> entries = {}) {
    Value out = Value::map();
    auto& items = out.as_map().items;
    items.emplace_back("ok", Value::boolean(true));
    for (const auto& entry : entries) {
        items.push_back(entry);
    }
    return out;
}

Value make_error(const std::string& message) {
    return Value::map({
        {"ok", Value::boolean(false)},
        {"error", Value::string(message)}
    });
}

void map_put(Value& map_value, std::string key, Value value) {
    map_value.as_map().items.emplace_back(std::move(key), std::move(value));
}

const Value* find_field(const Value& value, const char* key) {
    return value.find(key);
}

std::string require_string(const Value& value, const char* key) {
    const Value* field = find_field(value, key);
    if (field == nullptr || !field->is_string()) {
        throw Error(std::string("missing or invalid string field: ") + key);
    }
    return field->as_string();
}

std::string get_string(const Value& value, const char* key, const std::string& fallback = {}) {
    const Value* field = find_field(value, key);
    if (field == nullptr || field->is_null()) {
        return fallback;
    }
    if (!field->is_string()) {
        throw Error(std::string("invalid string field: ") + key);
    }
    return field->as_string();
}

bool get_bool(const Value& value, const char* key, bool fallback = false) {
    const Value* field = find_field(value, key);
    if (field == nullptr || field->is_null()) {
        return fallback;
    }
    if (!field->is_bool()) {
        throw Error(std::string("invalid bool field: ") + key);
    }
    return field->as_bool();
}

int get_int(const Value& value, const char* key, int fallback = 0) {
    const Value* field = find_field(value, key);
    if (field == nullptr || field->is_null()) {
        return fallback;
    }
    if (!field->is_number()) {
        throw Error(std::string("invalid number field: ") + key);
    }
    return static_cast<int>(field->as_number());
}

std::wstring widen(const std::string& text) {
    if (text.empty()) {
        return {};
    }
    const int needed = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
    if (needed <= 0) {
        throw Error("utf8 to utf16 conversion failed");
    }
    std::wstring out(static_cast<std::size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), out.data(), needed);
    return out;
}

std::string narrow(const std::wstring& text) {
    if (text.empty()) {
        return {};
    }
    const int needed = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (needed <= 0) {
        throw Error("utf16 to utf8 conversion failed");
    }
    std::string out(static_cast<std::size_t>(needed), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), out.data(), needed, nullptr, nullptr);
    return out;
}

COLORREF color_from_value(const Value* field, COLORREF fallback) {
    if (field == nullptr || field->is_null()) {
        return fallback;
    }
    if (field->is_number()) {
        const std::uint32_t raw = static_cast<std::uint32_t>(field->as_number());
        return RGB((raw >> 16) & 0xFFu, (raw >> 8) & 0xFFu, raw & 0xFFu);
    }
    if (field->is_map()) {
        const int r = get_int(*field, "r", 0);
        const int g = get_int(*field, "g", 0);
        const int b = get_int(*field, "b", 0);
        return RGB(r, g, b);
    }
    throw Error("invalid color value");
}

const Value* font_source(const Value& value) {
    const Value* font = value.find("font");
    if (font != nullptr && font->is_map()) {
        return font;
    }
    return &value;
}

HFONT create_font_from_value(const Value& value) {
    const Value* src = font_source(value);
    const std::wstring face = widen(get_string(*src, "face", "Segoe UI"));
    const int size = std::max(1, get_int(*src, "size", 16));
    const int weight = get_int(*src, "weight", FW_NORMAL);
    const BOOL italic = get_bool(*src, "italic", false) ? TRUE : FALSE;
    const BOOL underline = get_bool(*src, "underline", false) ? TRUE : FALSE;
    return CreateFontW(
        -size,
        0,
        0,
        0,
        weight,
        italic,
        underline,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        face.c_str()
        );
}

Value rect_to_value(const RECT& rc) {
    return Value::map({
        {"left", Value::number(rc.left)},
        {"top", Value::number(rc.top)},
        {"right", Value::number(rc.right)},
        {"bottom", Value::number(rc.bottom)},
        {"width", Value::number(rc.right - rc.left)},
        {"height", Value::number(rc.bottom - rc.top)}
    });
}

Value mouse_event_base(const WindowState& window, const char* type, int x, int y, WPARAM wparam) {
    return Value::map({
        {"type", Value::string(type)},
        {"window_id", Value::number(window.id)},
        {"x", Value::number(x)},
        {"y", Value::number(y)},
        {"left", Value::boolean((wparam & MK_LBUTTON) != 0)},
        {"right", Value::boolean((wparam & MK_RBUTTON) != 0)},
        {"middle", Value::boolean((wparam & MK_MBUTTON) != 0)},
        {"shift", Value::boolean((wparam & MK_SHIFT) != 0)},
        {"ctrl", Value::boolean((wparam & MK_CONTROL) != 0)},
        {"alt", Value::boolean((GetKeyState(VK_MENU) & 0x8000) != 0)}
    });
}

void fill_rect_solid(HDC dc, int x, int y, int w, int h, COLORREF color) {
    RECT rc{x, y, x + w, y + h};
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(dc, &rc, brush);
    DeleteObject(brush);
}

void render_scene(WindowState& window, HDC target) {
    RECT client{};
    GetClientRect(window.hwnd, &client);
    const int width = std::max(1, static_cast<int>(client.right - client.left));
    const int height = std::max(1, static_cast<int>(client.bottom - client.top));

    HDC mem = CreateCompatibleDC(target);
    HBITMAP bitmap = CreateCompatibleBitmap(target, width, height);
    HGDIOBJ old_bitmap = SelectObject(mem, bitmap);

    fill_rect_solid(mem, 0, 0, width, height, RGB(255, 255, 255));

    if (window.scene.is_list()) {
        for (const Value& op : window.scene.as_list().items) {
            if (!op.is_map()) {
                continue;
            }

            const std::string kind = get_string(op, "kind", "");
            if (kind == "clear") {
                fill_rect_solid(mem, 0, 0, width, height, color_from_value(op.find("color"), RGB(255, 255, 255)));
                continue;
            }

            if (kind == "fill_rect") {
                fill_rect_solid(
                    mem,
                    get_int(op, "x", 0),
                    get_int(op, "y", 0),
                    get_int(op, "w", 0),
                    get_int(op, "h", 0),
                    color_from_value(op.find("color"), RGB(0, 0, 0))
                    );
                continue;
            }

            if (kind == "rect") {
                HPEN pen = CreatePen(PS_SOLID, std::max(1, get_int(op, "thickness", 1)), color_from_value(op.find("color"), RGB(0, 0, 0)));
                HGDIOBJ old_pen = SelectObject(mem, pen);
                HGDIOBJ old_brush = SelectObject(mem, GetStockObject(HOLLOW_BRUSH));
                Rectangle(
                    mem,
                    get_int(op, "x", 0),
                    get_int(op, "y", 0),
                    get_int(op, "x", 0) + get_int(op, "w", 0),
                    get_int(op, "y", 0) + get_int(op, "h", 0)
                    );
                SelectObject(mem, old_brush);
                SelectObject(mem, old_pen);
                DeleteObject(pen);
                continue;
            }

            if (kind == "line") {
                HPEN pen = CreatePen(PS_SOLID, std::max(1, get_int(op, "thickness", 1)), color_from_value(op.find("color"), RGB(0, 0, 0)));
                HGDIOBJ old_pen = SelectObject(mem, pen);
                MoveToEx(mem, get_int(op, "x1", 0), get_int(op, "y1", 0), nullptr);
                LineTo(mem, get_int(op, "x2", 0), get_int(op, "y2", 0));
                SelectObject(mem, old_pen);
                DeleteObject(pen);
                continue;
            }

            if (kind == "text") {
                const std::wstring text = widen(get_string(op, "text", ""));
                HFONT font = create_font_from_value(op);
                HGDIOBJ old_font = SelectObject(mem, font);
                const int old_mode = SetBkMode(mem, TRANSPARENT);
                const COLORREF old_color = SetTextColor(mem, color_from_value(op.find("color"), RGB(0, 0, 0)));
                TextOutW(mem, get_int(op, "x", 0), get_int(op, "y", 0), text.c_str(), static_cast<int>(text.size()));
                SetTextColor(mem, old_color);
                SetBkMode(mem, old_mode);
                SelectObject(mem, old_font);
                DeleteObject(font);
                continue;
            }
        }
    }

    BitBlt(target, 0, 0, width, height, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old_bitmap);
    DeleteObject(bitmap);
    DeleteDC(mem);
}

void arm_render_timer(WindowState& window) {
    if (window.render_timer_armed || window.hwnd == nullptr || !IsWindow(window.hwnd)) {
        return;
    }
    if (SetTimer(window.hwnd, kInternalRenderTimerId, kInternalRenderTimerIntervalMs, nullptr) == 0) {
        throw Error("SetTimer failed for internal render timer");
    }
    window.render_timer_armed = true;
}

void request_render(WindowState& window) {
    window.render_dirty = true;
    arm_render_timer(window);
}

void disarm_render_timer(WindowState& window) {
    if (!window.render_timer_armed || window.hwnd == nullptr || !IsWindow(window.hwnd)) {
        window.render_timer_armed = false;
        return;
    }
    KillTimer(window.hwnd, kInternalRenderTimerId);
    window.render_timer_armed = false;
}

class HelperState {
public:
    explicit HelperState(TcpSocket channel)
        : channel_(std::move(channel)),
        ui_thread_id_(GetCurrentThreadId()) {
        ensure_window_class();
        MSG msg{};
        PeekMessageW(&msg, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
    }

    int run() {
        command_thread_ = std::thread([this]() { command_loop(); });

        MSG msg{};
        while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
            if (msg.message == WM_APP_RUN_TASK) {
                auto* task = reinterpret_cast<UiTask*>(msg.wParam);
                try {
                    task->result = task->run();
                } catch (const std::exception& e) {
                    task->error = e.what();
                }
                {
                    std::lock_guard<std::mutex> lock(task->mutex);
                    task->done = true;
                }
                task->cv.notify_one();
                continue;
            }

            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        stopping_ = true;
        if (command_thread_.joinable()) {
            command_thread_.join();
        }
        return 0;
    }

    void enqueue_event(Value event) {
        {
            std::lock_guard<std::mutex> lock(event_mutex_);
            events_.push(std::move(event));
        }
        event_cv_.notify_one();
    }

    LRESULT handle_window_message(WindowState& window, UINT message, WPARAM wparam, LPARAM lparam) {
        switch (message) {
        case WM_CLOSE:
            enqueue_event(Value::map({
                {"type", Value::string("close")},
                {"window_id", Value::number(window.id)}
            }));
            return 0;
        case WM_DESTROY:
            enqueue_event(Value::map({
                {"type", Value::string("destroy")},
                {"window_id", Value::number(window.id)}
            }));
            return 0;
        case WM_SIZE:
            enqueue_event(Value::map({
                {"type", Value::string("resize")},
                {"window_id", Value::number(window.id)},
                {"width", Value::number(LOWORD(lparam))},
                {"height", Value::number(HIWORD(lparam))}
            }));
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            RECT client{};
            GetClientRect(window.hwnd, &client);
            enqueue_event(Value::map({
                {"type", Value::string("paint")},
                {"window_id", Value::number(window.id)},
                {"width", Value::number(client.right - client.left)},
                {"height", Value::number(client.bottom - client.top)}
            }));
            PAINTSTRUCT ps{};
            BeginPaint(window.hwnd, &ps);
            EndPaint(window.hwnd, &ps);
            request_render(window);
            return 0;
        }
        case WM_MOUSEMOVE: {
            if (!window.tracking_mouse) {
                TRACKMOUSEEVENT tme{};
                tme.cbSize = sizeof(tme);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = window.hwnd;
                TrackMouseEvent(&tme);
                window.tracking_mouse = true;
            }
            enqueue_event(mouse_event_base(window, "mouse_move", GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam), wparam));
            return 0;
        }
        case WM_MOUSELEAVE:
            window.tracking_mouse = false;
            enqueue_event(Value::map({
                {"type", Value::string("mouse_leave")},
                {"window_id", Value::number(window.id)}
            }));
            return 0;
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN: {
            SetCapture(window.hwnd);
            Value event = mouse_event_base(window, "mouse_down", GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam), wparam);
            map_put(event, "button", Value::string(message == WM_LBUTTONDOWN ? "left" : message == WM_RBUTTONDOWN ? "right" : "middle"));
            enqueue_event(std::move(event));
            return 0;
        }
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP: {
            if ((wparam & (MK_LBUTTON | MK_RBUTTON | MK_MBUTTON)) == 0) {
                ReleaseCapture();
            }
            Value event = mouse_event_base(window, "mouse_up", GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam), wparam);
            map_put(event, "button", Value::string(message == WM_LBUTTONUP ? "left" : message == WM_RBUTTONUP ? "right" : "middle"));
            enqueue_event(std::move(event));
            return 0;
        }
        case WM_MOUSEWHEEL: {
            POINT pt{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            ScreenToClient(window.hwnd, &pt);
            Value event = mouse_event_base(window, "mouse_wheel", pt.x, pt.y, GET_KEYSTATE_WPARAM(wparam));
            map_put(event, "delta", Value::number(GET_WHEEL_DELTA_WPARAM(wparam)));
            enqueue_event(std::move(event));
            return 0;
        }
        case WM_TIMER:
            if (static_cast<UINT_PTR>(wparam) == kInternalRenderTimerId) {
                disarm_render_timer(window);
                if (!window.render_dirty || window.hwnd == nullptr || !IsWindow(window.hwnd) || !IsWindowVisible(window.hwnd)) {
                    return 0;
                }
                HDC dc = GetDC(window.hwnd);
                if (dc != nullptr) {
                    const auto started = std::chrono::steady_clock::now();
                    render_scene(window, dc);
                    ReleaseDC(window.hwnd, dc);
                    window.render_dirty = false;
                    note_present();
                    note_profile("render_scene", started);
                } else {
                    arm_render_timer(window);
                }
                return 0;
            }
            enqueue_event(Value::map({
                {"type", Value::string("timer")},
                {"window_id", Value::number(window.id)},
                {"timer_id", Value::number(static_cast<double>(static_cast<std::uint64_t>(wparam)))}
            }));
            return 0;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            enqueue_event(Value::map({
                {"type", Value::string("key_down")},
                {"window_id", Value::number(window.id)},
                {"key", Value::number(static_cast<int>(wparam))},
                {"repeat", Value::boolean((lparam & 0x40000000) != 0)},
                {"shift", Value::boolean((GetKeyState(VK_SHIFT) & 0x8000) != 0)},
                {"ctrl", Value::boolean((GetKeyState(VK_CONTROL) & 0x8000) != 0)},
                {"alt", Value::boolean((GetKeyState(VK_MENU) & 0x8000) != 0)}
            }));
            return 0;
        case WM_KEYUP:
        case WM_SYSKEYUP:
            enqueue_event(Value::map({
                {"type", Value::string("key_up")},
                {"window_id", Value::number(window.id)},
                {"key", Value::number(static_cast<int>(wparam))},
                {"shift", Value::boolean((GetKeyState(VK_SHIFT) & 0x8000) != 0)},
                {"ctrl", Value::boolean((GetKeyState(VK_CONTROL) & 0x8000) != 0)},
                {"alt", Value::boolean((GetKeyState(VK_MENU) & 0x8000) != 0)}
            }));
            return 0;
        case WM_CHAR: {
            std::wstring wtext(1, static_cast<wchar_t>(wparam));
            enqueue_event(Value::map({
                {"type", Value::string("char")},
                {"window_id", Value::number(window.id)},
                {"text", Value::string(narrow(wtext))},
                {"code", Value::number(static_cast<int>(wparam))}
            }));
            return 0;
        }
        default:
            return DefWindowProcW(window.hwnd, message, wparam, lparam);
        }
    }

private:
    static LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
        if (message == WM_NCCREATE) {
            auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
            auto* window = reinterpret_cast<WindowState*>(create->lpCreateParams);
            window->hwnd = hwnd;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
            return TRUE;
        }

        auto* window = reinterpret_cast<WindowState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (window != nullptr && window->owner != nullptr) {
            return window->owner->handle_window_message(*window, message, wparam, lparam);
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }

    void ensure_window_class() {
        static ATOM klass = 0;
        if (klass != 0) {
            return;
        }

        WNDCLASSW wc{};
        wc.lpfnWndProc = &HelperState::window_proc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = kWindowClassName;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        klass = RegisterClassW(&wc);
        if (klass == 0) {
            throw Error("RegisterClassW failed");
        }
    }

    WindowState& require_window(int id) {
        auto it = windows_.find(id);
        if (it == windows_.end()) {
            throw Error("unknown window id");
        }
        return *it->second;
    }

    Value invoke_on_ui(std::function<Value()> fn) {
        if (GetCurrentThreadId() == ui_thread_id_) {
            return fn();
        }

        UiTask task;
        task.run = std::move(fn);
        if (!PostThreadMessageW(ui_thread_id_, WM_APP_RUN_TASK, reinterpret_cast<WPARAM>(&task), 0)) {
            throw Error("PostThreadMessageW failed");
        }

        std::unique_lock<std::mutex> lock(task.mutex);
        task.cv.wait(lock, [&task]() { return task.done; });
        if (!task.error.empty()) {
            throw Error(task.error);
        }
        return task.result;
    }

    Value create_window_ui(const Value& request) {
        const std::wstring title = widen(get_string(request, "title", "wingui"));
        const int client_width = std::max(1, get_int(request, "width", 800));
        const int client_height = std::max(1, get_int(request, "height", 600));
        const bool resizable = get_bool(request, "resizable", true);
        const bool visible = get_bool(request, "visible", true);

        DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
        if (resizable) {
            style |= WS_THICKFRAME | WS_MAXIMIZEBOX;
        }

        RECT rc{0, 0, client_width, client_height};
        AdjustWindowRectEx(&rc, style, FALSE, 0);

        auto window = std::make_unique<WindowState>();
        window->owner = this;
        window->id = next_window_id_++;
        window->scene = Value::list();

        HWND hwnd = CreateWindowExW(
            0,
            kWindowClassName,
            title.c_str(),
            style,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            rc.right - rc.left,
            rc.bottom - rc.top,
            nullptr,
            nullptr,
            GetModuleHandleW(nullptr),
            window.get()
            );
        if (hwnd == nullptr) {
            throw Error("CreateWindowExW failed");
        }

        const int id = window->id;
        windows_.emplace(id, std::move(window));

        if (visible) {
            ShowWindow(hwnd, SW_SHOW);
            UpdateWindow(hwnd);
        } else {
            ShowWindow(hwnd, SW_HIDE);
        }

        RECT client{};
        GetClientRect(hwnd, &client);
        return make_ok({
            {"id", Value::number(id)},
            {"visible", Value::boolean(visible)},
            {"client", rect_to_value(client)}
        });
    }

    Value show_window_ui(const Value& request) {
        WindowState& window = require_window(get_int(request, "window_id", -1));
        const bool visible = get_bool(request, "visible", true);
        ShowWindow(window.hwnd, visible ? SW_SHOW : SW_HIDE);
        if (visible) {
            UpdateWindow(window.hwnd);
        }
        return make_ok();
    }

    Value set_title_ui(const Value& request) {
        WindowState& window = require_window(get_int(request, "window_id", -1));
        const std::wstring title = widen(require_string(request, "title"));
        SetWindowTextW(window.hwnd, title.c_str());
        return make_ok();
    }

    Value destroy_window_ui(const Value& request) {
        const int id = get_int(request, "window_id", -1);
        auto it = windows_.find(id);
        if (it == windows_.end()) {
            return make_ok();
        }

        HWND hwnd = it->second->hwnd;
        disarm_render_timer(*it->second);
        if (IsWindow(hwnd)) {
            DestroyWindow(hwnd);
        }
        windows_.erase(it);
        return make_ok();
    }

    Value present_ui(const Value& request) {
        WindowState& window = require_window(get_int(request, "window_id", -1));
        const Value* ops = request.find("ops");
        if (ops == nullptr || !ops->is_list()) {
            throw Error("present requires ops list");
        }
        window.scene = *ops;
        request_render(window);
        return make_ok();
    }

    Value stats_direct() {
        Value out = make_ok();
        {
            std::lock_guard<std::mutex> lock(event_mutex_);
            map_put(out, "queue_len", Value::number(static_cast<double>(events_.size())));
        }
        map_put(out, "fps", Value::number(current_fps()));
        Value profile = Value::map();
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            for (const auto& entry : profile_buckets_) {
                const ProfileBucket& bucket = entry.second;
                Value bucket_value = Value::map();
                map_put(bucket_value, "calls", Value::number(static_cast<double>(bucket.calls)));
                map_put(bucket_value, "total_ms", Value::number(bucket.total_ms));
                map_put(bucket_value, "max_ms", Value::number(bucket.max_ms));
                map_put(bucket_value, "avg_ms", Value::number(bucket.calls == 0 ? 0.0 : bucket.total_ms / static_cast<double>(bucket.calls)));
                map_put(profile, entry.first, std::move(bucket_value));
            }
        }
        map_put(out, "profile", std::move(profile));
        return out;
    }

    Value clipboard_get_ui() {
        if (!OpenClipboard(nullptr)) {
            throw Error("OpenClipboard failed");
        }

        std::string text;
        HANDLE handle = GetClipboardData(CF_UNICODETEXT);
        if (handle != nullptr) {
            LPCWSTR raw = static_cast<LPCWSTR>(GlobalLock(handle));
            if (raw != nullptr) {
                text = narrow(std::wstring(raw));
                GlobalUnlock(handle);
            }
        }

        CloseClipboard();
        return make_ok({
            {"text", Value::string(text)}
        });
    }

    Value clipboard_set_ui(const Value& request) {
        const std::wstring text = widen(get_string(request, "text", ""));
        if (!OpenClipboard(nullptr)) {
            throw Error("OpenClipboard failed");
        }

        if (!EmptyClipboard()) {
            CloseClipboard();
            throw Error("EmptyClipboard failed");
        }

        const std::size_t bytes = (text.size() + 1) * sizeof(wchar_t);
        HGLOBAL handle = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (handle == nullptr) {
            CloseClipboard();
            throw Error("GlobalAlloc failed");
        }

        void* raw = GlobalLock(handle);
        if (raw == nullptr) {
            GlobalFree(handle);
            CloseClipboard();
            throw Error("GlobalLock failed");
        }

        std::memcpy(raw, text.c_str(), bytes);
        GlobalUnlock(handle);

        if (SetClipboardData(CF_UNICODETEXT, handle) == nullptr) {
            GlobalFree(handle);
            CloseClipboard();
            throw Error("SetClipboardData failed");
        }

        CloseClipboard();
        return make_ok();
    }

    Value invalidate_ui(const Value& request) {
        WindowState& window = require_window(get_int(request, "window_id", -1));
        request_render(window);
        return make_ok();
    }

    Value set_timer_ui(const Value& request) {
        WindowState& window = require_window(get_int(request, "window_id", -1));
        const UINT_PTR timer_id = static_cast<UINT_PTR>(std::max(1, get_int(request, "timer_id", 1)));
        if (timer_id == kInternalRenderTimerId) {
            throw Error("timer_id is reserved by wingui internal renderer");
        }
        const UINT interval_ms = static_cast<UINT>(std::max(1, get_int(request, "interval_ms", 16)));
        if (SetTimer(window.hwnd, timer_id, interval_ms, nullptr) == 0) {
            throw Error("SetTimer failed");
        }
        return make_ok({
            {"timer_id", Value::number(static_cast<double>(timer_id))},
            {"interval_ms", Value::number(interval_ms)}
        });
    }

    Value kill_timer_ui(const Value& request) {
        WindowState& window = require_window(get_int(request, "window_id", -1));
        const UINT_PTR timer_id = static_cast<UINT_PTR>(std::max(1, get_int(request, "timer_id", 1)));
        if (timer_id == kInternalRenderTimerId) {
            throw Error("timer_id is reserved by wingui internal renderer");
        }
        KillTimer(window.hwnd, timer_id);
        return make_ok({
            {"timer_id", Value::number(static_cast<double>(timer_id))}
        });
    }

    Value client_rect_ui(const Value& request) {
        WindowState& window = require_window(get_int(request, "window_id", -1));
        RECT rc{};
        GetClientRect(window.hwnd, &rc);
        return make_ok({
            {"rect", rect_to_value(rc)}
        });
    }

    Value stop_ui() {
        std::vector<int> ids;
        ids.reserve(windows_.size());
        for (const auto& entry : windows_) {
            ids.push_back(entry.first);
        }
        for (int id : ids) {
            Value req = Value::map({
                {"window_id", Value::number(id)}
            });
            destroy_window_ui(req);
        }
        PostQuitMessage(0);
        return make_ok();
    }

    Value measure_text_direct(const Value& request) {
        const auto started = std::chrono::steady_clock::now();
        const std::wstring text = widen(get_string(request, "text", ""));
        HDC dc = GetDC(nullptr);
        HFONT font = create_font_from_value(request);
        HGDIOBJ old_font = SelectObject(dc, font);

        SIZE size{};
        GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &size);
        TEXTMETRICW metrics{};
        GetTextMetricsW(dc, &metrics);

        SelectObject(dc, old_font);
        DeleteObject(font);
        ReleaseDC(nullptr, dc);

        note_profile("measure_text", started);

        return make_ok({
            {"width", Value::number(size.cx)},
            {"height", Value::number(size.cy)},
            {"ascent", Value::number(metrics.tmAscent)},
            {"descent", Value::number(metrics.tmDescent)},
            {"line_height", Value::number(metrics.tmHeight)}
        });
    }

    Value poll_event_direct(const Value& request) {
        const auto started = std::chrono::steady_clock::now();
        const int timeout_ms = std::max(0, get_int(request, "timeout_ms", 0));
        std::unique_lock<std::mutex> lock(event_mutex_);
        if (events_.empty() && timeout_ms > 0) {
            event_cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this]() { return !events_.empty() || stopping_.load(); });
        }
        if (events_.empty()) {
            note_profile("poll_event", started);
            return make_ok({
                {"event", Value::null()}
            });
        }
        Value event = std::move(events_.front());
        events_.pop();
        note_profile("poll_event", started);
        return make_ok({
            {"event", std::move(event)}
        });
    }

    Value handle_command(const Value& request) {
        if (!request.is_map()) {
            throw Error("request must be a map");
        }

        const std::string cmd = require_string(request, "cmd");

        if (cmd == "ping") {
            return make_ok({
                {"helper", Value::string("wingui")},
                {"protocol", Value::string("wingui/v1")},
                {"pid", Value::number(GetCurrentProcessId())}
            });
        }

        if (cmd == "screen_size") {
            return make_ok({
                {"width", Value::number(GetSystemMetrics(SM_CXSCREEN))},
                {"height", Value::number(GetSystemMetrics(SM_CYSCREEN))}
            });
        }

        if (cmd == "work_area") {
            RECT rc{};
            SystemParametersInfoW(SPI_GETWORKAREA, 0, &rc, 0);
            Value out = make_ok();
            map_put(out, "rect", rect_to_value(rc));
            return out;
        }

        if (cmd == "measure_text") {
            return measure_text_direct(request);
        }

        if (cmd == "poll_event") {
            return poll_event_direct(request);
        }

        if (cmd == "stats") {
            return stats_direct();
        }

        if (cmd == "clipboard_get") {
            return invoke_on_ui([this]() { return clipboard_get_ui(); });
        }

        if (cmd == "clipboard_set") {
            return invoke_on_ui([this, request]() { return clipboard_set_ui(request); });
        }

        if (cmd == "create_window") {
            return invoke_on_ui([this, request]() { return create_window_ui(request); });
        }

        if (cmd == "show_window") {
            return invoke_on_ui([this, request]() { return show_window_ui(request); });
        }

        if (cmd == "set_title") {
            return invoke_on_ui([this, request]() { return set_title_ui(request); });
        }

        if (cmd == "destroy_window") {
            return invoke_on_ui([this, request]() { return destroy_window_ui(request); });
        }

        if (cmd == "present") {
            return invoke_on_ui([this, request]() { return present_ui(request); });
        }

        if (cmd == "invalidate") {
            return invoke_on_ui([this, request]() { return invalidate_ui(request); });
        }

        if (cmd == "set_timer") {
            return invoke_on_ui([this, request]() { return set_timer_ui(request); });
        }

        if (cmd == "kill_timer") {
            return invoke_on_ui([this, request]() { return kill_timer_ui(request); });
        }

        if (cmd == "client_rect") {
            return invoke_on_ui([this, request]() { return client_rect_ui(request); });
        }

        if (cmd == "stop") {
            stopping_ = true;
            Value response = make_ok();
            invoke_on_ui([this]() { return stop_ui(); });
            return response;
        }

        throw Error("unknown cmd: " + cmd);
    }

    void command_loop() {
        try {
            while (!stopping_.load()) {
                Value request = channel_.recv_net();
                Value response = handle_command(request);
                channel_.send_net(response);
                if (request.is_map()) {
                    const Value* cmd = request.find("cmd");
                    if (cmd != nullptr && cmd->is_string() && cmd->as_string() == "stop") {
                        break;
                    }
                }
            }
        } catch (const std::exception& e) {
            if (!stopping_.load()) {
                try {
                    channel_.send_net(make_error(e.what()));
                } catch (...) {
                }
                stopping_ = true;
                PostThreadMessageW(ui_thread_id_, WM_QUIT, 0, 0);
            }
        }
    }

    void note_present() {
        const auto now = std::chrono::steady_clock::now();
        std::lock_guard<std::mutex> lock(stats_mutex_);
        frame_times_.push_back(now);
        const auto cutoff = now - std::chrono::seconds(1);
        while (!frame_times_.empty() && frame_times_.front() < cutoff) {
            frame_times_.pop_front();
        }
    }

    void note_profile(const std::string& name, std::chrono::steady_clock::time_point started) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started).count();
        std::lock_guard<std::mutex> lock(stats_mutex_);
        ProfileBucket& bucket = profile_buckets_[name];
        bucket.calls += 1;
        bucket.total_ms += static_cast<double>(elapsed) / 1000.0;
        bucket.max_ms = std::max(bucket.max_ms, static_cast<double>(elapsed) / 1000.0);
    }

    double current_fps() {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        if (frame_times_.size() <= 1) {
            return frame_times_.empty() ? 0.0 : 1.0;
        }
        const auto span = std::chrono::duration_cast<std::chrono::milliseconds>(frame_times_.back() - frame_times_.front()).count();
        if (span <= 0) {
            return static_cast<double>(frame_times_.size());
        }
        return static_cast<double>(frame_times_.size() - 1) * 1000.0 / static_cast<double>(span);
    }

    TcpSocket channel_;
    DWORD ui_thread_id_ = 0;
    std::atomic<bool> stopping_{false};
    std::thread command_thread_;
    std::mutex event_mutex_;
    std::condition_variable event_cv_;
    std::queue<Value> events_;
    std::mutex stats_mutex_;
    std::deque<std::chrono::steady_clock::time_point> frame_times_;
    std::unordered_map<std::string, ProfileBucket> profile_buckets_;
    int next_window_id_ = 1;
    std::unordered_map<int, std::unique_ptr<WindowState>> windows_;
};

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        return 2;
    }

    try {
        TcpServer server = TcpServer::listen(argv[1]);
        TcpSocket channel = server.accept();
        HelperState helper(std::move(channel));
        return helper.run();
    } catch (const std::exception&) {
        return 1;
    }
}
