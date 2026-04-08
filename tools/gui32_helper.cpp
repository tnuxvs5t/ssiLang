#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <windowsx.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

struct Fail : std::runtime_error {
    using std::runtime_error::runtime_error;
};

[[noreturn]] void fail(const std::string& msg) {
    throw Fail(msg);
}

struct SValue {
    enum Type { NUM, STR, BOOL, NUL, LIST, MAP } type = NUL;
    double num = 0;
    std::string str;
    std::vector<SValue> list;
    std::vector<std::pair<std::string, SValue>> map;

    static SValue Num(double v) {
        SValue out;
        out.type = NUM;
        out.num = v;
        return out;
    }

    static SValue Str(std::string v) {
        SValue out;
        out.type = STR;
        out.str = std::move(v);
        return out;
    }

    static SValue Bool(bool v) {
        SValue out;
        out.type = BOOL;
        out.num = v ? 1.0 : 0.0;
        return out;
    }

    static SValue Null() {
        return SValue();
    }

    static SValue List(std::vector<SValue> v = {}) {
        SValue out;
        out.type = LIST;
        out.list = std::move(v);
        return out;
    }

    static SValue Map(std::vector<std::pair<std::string, SValue>> v = {}) {
        SValue out;
        out.type = MAP;
        out.map = std::move(v);
        return out;
    }
};

void write_u32_le(std::string& out, uint32_t v) {
    out.push_back(static_cast<char>(v & 0xFF));
    out.push_back(static_cast<char>((v >> 8) & 0xFF));
    out.push_back(static_cast<char>((v >> 16) & 0xFF));
    out.push_back(static_cast<char>((v >> 24) & 0xFF));
}

uint32_t read_u32_le(const char* in) {
    return static_cast<uint32_t>(static_cast<unsigned char>(in[0])) |
           (static_cast<uint32_t>(static_cast<unsigned char>(in[1])) << 8) |
           (static_cast<uint32_t>(static_cast<unsigned char>(in[2])) << 16) |
           (static_cast<uint32_t>(static_cast<unsigned char>(in[3])) << 24);
}

std::string serialize_inner(const SValue& v) {
    std::ostringstream out;
    switch (v.type) {
    case SValue::NUM:
        out << "N" << v.num << "\n";
        break;
    case SValue::STR:
        out << "S" << v.str.size() << ":" << v.str;
        break;
    case SValue::BOOL:
        out << "B" << (v.num != 0 ? "1" : "0") << "\n";
        break;
    case SValue::NUL:
        out << "Z\n";
        break;
    case SValue::LIST:
        out << "L" << v.list.size() << "\n";
        for (const auto& item : v.list) out << serialize_inner(item);
        break;
    case SValue::MAP:
        out << "M" << v.map.size() << "\n";
        for (const auto& entry : v.map) {
            out << "S" << entry.first.size() << ":" << entry.first;
            out << serialize_inner(entry.second);
        }
        break;
    }
    return out.str();
}

std::string serialize(const SValue& v) {
    return "V1\n" + serialize_inner(v);
}

SValue deserialize_inner(const std::string& data, size_t& pos) {
    if (pos >= data.size()) return SValue::Null();
    char tag = data[pos++];
    switch (tag) {
    case 'N': {
        size_t end = data.find('\n', pos);
        if (end == std::string::npos) fail("bad serialized number");
        double num = std::stod(data.substr(pos, end - pos));
        pos = end + 1;
        return SValue::Num(num);
    }
    case 'S': {
        size_t colon = data.find(':', pos);
        if (colon == std::string::npos) fail("bad serialized string");
        size_t len = static_cast<size_t>(std::stoul(data.substr(pos, colon - pos)));
        pos = colon + 1;
        if (pos + len > data.size()) fail("truncated serialized string");
        std::string text = data.substr(pos, len);
        pos += len;
        return SValue::Str(text);
    }
    case 'B': {
        if (pos + 1 >= data.size()) fail("bad serialized bool");
        bool value = data[pos] == '1';
        pos += 2;
        return SValue::Bool(value);
    }
    case 'Z':
        if (pos >= data.size()) fail("bad serialized null");
        pos += 1;
        return SValue::Null();
    case 'L': {
        size_t end = data.find('\n', pos);
        if (end == std::string::npos) fail("bad serialized list");
        size_t count = static_cast<size_t>(std::stoul(data.substr(pos, end - pos)));
        pos = end + 1;
        std::vector<SValue> items;
        items.reserve(count);
        for (size_t i = 0; i < count; ++i) items.push_back(deserialize_inner(data, pos));
        return SValue::List(std::move(items));
    }
    case 'M': {
        size_t end = data.find('\n', pos);
        if (end == std::string::npos) fail("bad serialized map");
        size_t count = static_cast<size_t>(std::stoul(data.substr(pos, end - pos)));
        pos = end + 1;
        std::vector<std::pair<std::string, SValue>> items;
        items.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            SValue key = deserialize_inner(data, pos);
            if (key.type != SValue::STR) fail("map key must be string");
            items.push_back({key.str, deserialize_inner(data, pos)});
        }
        return SValue::Map(std::move(items));
    }
    default:
        fail("unknown serialized tag");
    }
}

SValue deserialize(const std::string& data) {
    size_t pos = data.rfind("V1\n", 0) == 0 ? 3 : 0;
    return deserialize_inner(data, pos);
}

const SValue* map_get(const SValue& map, const std::string& key) {
    if (map.type != SValue::MAP) return nullptr;
    for (const auto& entry : map.map) if (entry.first == key) return &entry.second;
    return nullptr;
}

std::string need_string(const SValue& map, const std::string& key) {
    const SValue* value = map_get(map, key);
    if (!value || value->type != SValue::STR) fail("missing string field: " + key);
    return value->str;
}

double need_number(const SValue& map, const std::string& key) {
    const SValue* value = map_get(map, key);
    if (!value || value->type != SValue::NUM) fail("missing number field: " + key);
    return value->num;
}

std::string opt_string(const SValue& map, const std::string& key, const std::string& fallback = "") {
    const SValue* value = map_get(map, key);
    if (!value || value->type == SValue::NUL) return fallback;
    if (value->type != SValue::STR) fail("field must be string: " + key);
    return value->str;
}

bool opt_bool(const SValue& map, const std::string& key, bool fallback = false) {
    const SValue* value = map_get(map, key);
    if (!value || value->type == SValue::NUL) return fallback;
    if (value->type != SValue::BOOL) fail("field must be bool: " + key);
    return value->num != 0;
}

int opt_int(const SValue& map, const std::string& key, int fallback = 0) {
    const SValue* value = map_get(map, key);
    if (!value || value->type == SValue::NUL) return fallback;
    if (value->type != SValue::NUM) fail("field must be number: " + key);
    return static_cast<int>(value->num);
}

COLORREF color_from_number(double num) {
    uint32_t v = static_cast<uint32_t>(num);
    return RGB((v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF);
}

COLORREF opt_color(const SValue& map, const std::string& key, COLORREF fallback) {
    const SValue* value = map_get(map, key);
    if (!value || value->type == SValue::NUL) return fallback;
    if (value->type != SValue::NUM) fail("field must be number color: " + key);
    return color_from_number(value->num);
}

const SValue& need_list(const SValue& map, const std::string& key) {
    const SValue* value = map_get(map, key);
    if (!value || value->type != SValue::LIST) fail("missing list field: " + key);
    return *value;
}

std::vector<std::string> string_list(const SValue& value) {
    if (value.type != SValue::LIST) fail("expected list");
    std::vector<std::string> out;
    out.reserve(value.list.size());
    for (const auto& item : value.list) {
        if (item.type != SValue::STR) fail("list items must be string");
        out.push_back(item.str);
    }
    return out;
}

std::wstring utf8_to_wide(const std::string& text) {
    if (text.empty()) return std::wstring();
    int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
    if (size <= 0) fail("utf8_to_wide failed");
    std::wstring out(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), &out[0], size);
    return out;
}

std::string wide_to_utf8(const std::wstring& text) {
    if (text.empty()) return std::string();
    int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) fail("wide_to_utf8 failed");
    std::string out(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), &out[0], size, nullptr, nullptr);
    return out;
}

std::string socket_error_text() {
    return std::to_string(WSAGetLastError());
}

struct WindowState {
    int id = 0;
    HWND hwnd = nullptr;
    std::vector<int> controls;
    HMENU menu = nullptr;
    std::vector<int> menu_commands;
};

struct ControlState {
    int id = 0;
    int window_id = 0;
    int command_id = 0;
    HWND hwnd = nullptr;
    std::string kind;
    std::string token;
};

struct MenuState {
    int window_id = 0;
    std::string token;
};

struct TimerState {
    int id = 0;
    int window_id = 0;
    UINT_PTR native_id = 0;
    std::string token;
};

struct CanvasState {
    int control_id = 0;
    int window_id = 0;
    HWND hwnd = nullptr;
    HDC mem_dc = nullptr;
    HBITMAP bitmap = nullptr;
    HGDIOBJ old_bitmap = nullptr;
    int width = 0;
    int height = 0;
    COLORREF back_color = RGB(255, 255, 255);
    bool tracking_mouse = false;
};

class App;
App* g_app = nullptr;

class App {
public:
    App() = default;

    ~App() {
        destroy_all_windows();
        close_client();
        if (listen_socket_ != INVALID_SOCKET) closesocket(listen_socket_);
    }

    void run(uint16_t port) {
        open_listener(port);
        register_window_class();
        while (running_) {
            pump_messages();
            poll_network();
        }
    }

    LRESULT on_window_message(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
        auto canvas_it = canvas_hwnds_.find(hwnd);
        if (canvas_it != canvas_hwnds_.end()) {
            return on_canvas_message(canvas_it->second, hwnd, message, wparam, lparam);
        }

        int window_id = static_cast<int>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

        switch (message) {
        case WM_COMMAND: {
            int command_id = LOWORD(wparam);
            int notify_code = HIWORD(wparam);

            auto menu_it = command_to_menu_.find(command_id);
            if (menu_it != command_to_menu_.end()) {
                send_event({
                    {"event", SValue::Str("menu")},
                    {"window", SValue::Num(menu_it->second.window_id)},
                    {"token", SValue::Str(menu_it->second.token)}
                });
                return 0;
            }

            auto it = command_to_control_.find(command_id);
            if (it != command_to_control_.end()) {
                auto control = need_control(it->second);
                if (control->kind == "button" || control->kind == "checkbox") {
                    if (notify_code == BN_CLICKED) {
                        std::vector<std::pair<std::string, SValue>> extra{
                            {"window", SValue::Num(control->window_id)},
                            {"control", SValue::Num(control->id)},
                            {"token", SValue::Str(control->token)}
                        };
                        if (control->kind == "checkbox") {
                            extra.push_back({"checked", SValue::Bool(SendMessageW(control->hwnd, BM_GETCHECK, 0, 0) == BST_CHECKED)});
                        }
                        send_event("click", std::move(extra));
                        return 0;
                    }
                }
                if (control->kind == "listbox" && notify_code == LBN_SELCHANGE) {
                    send_event("select", {
                        {"window", SValue::Num(control->window_id)},
                        {"control", SValue::Num(control->id)},
                        {"token", SValue::Str(control->token)},
                        {"selected", SValue::Num((double)SendMessageW(control->hwnd, LB_GETCURSEL, 0, 0))}
                    });
                    return 0;
                }
                if ((control->kind == "input" || control->kind == "textarea") && notify_code == EN_CHANGE) {
                    send_event("change", {
                        {"window", SValue::Num(control->window_id)},
                        {"control", SValue::Num(control->id)},
                        {"token", SValue::Str(control->token)}
                    });
                    return 0;
                }
                if ((notify_code == EN_SETFOCUS) || (notify_code == BN_SETFOCUS) || (notify_code == LBN_SETFOCUS)) {
                    send_event("focus", {
                        {"window", SValue::Num(control->window_id)},
                        {"control", SValue::Num(control->id)},
                        {"token", SValue::Str(control->token)}
                    });
                    return 0;
                }
                if ((notify_code == EN_KILLFOCUS) || (notify_code == BN_KILLFOCUS) || (notify_code == LBN_KILLFOCUS)) {
                    send_event("blur", {
                        {"window", SValue::Num(control->window_id)},
                        {"control", SValue::Num(control->id)},
                        {"token", SValue::Str(control->token)}
                    });
                    return 0;
                }
            }
            break;
        }
        case WM_TIMER: {
            int timer_id = static_cast<int>(wparam);
            auto it = timers_.find(timer_id);
            if (it != timers_.end()) {
                send_event("timer", {
                    {"window", SValue::Num(it->second.window_id)},
                    {"timer", SValue::Num(it->second.id)},
                    {"token", SValue::Str(it->second.token)}
                });
                return 0;
            }
            break;
        }
        case WM_SETFOCUS:
            if (window_id != 0) {
                send_event("window_focus", {{"window", SValue::Num(window_id)}});
            }
            return 0;
        case WM_KILLFOCUS:
            if (window_id != 0) {
                send_event("window_blur", {{"window", SValue::Num(window_id)}});
            }
            return 0;
        case WM_SIZE:
            if (window_id != 0) {
                send_event("resize", {
                    {"window", SValue::Num(window_id)},
                    {"width", SValue::Num((double)LOWORD(lparam))},
                    {"height", SValue::Num((double)HIWORD(lparam))}
                });
            }
            break;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            if (window_id != 0) {
                send_event("key_down", {
                    {"window", SValue::Num(window_id)},
                    {"key", SValue::Num((double)wparam)},
                    {"repeat", SValue::Bool((lparam & 0x40000000) != 0)}
                });
            }
            break;
        case WM_KEYUP:
        case WM_SYSKEYUP:
            if (window_id != 0) {
                send_event("key_up", {
                    {"window", SValue::Num(window_id)},
                    {"key", SValue::Num((double)wparam)}
                });
            }
            break;
        case WM_CHAR:
            if (window_id != 0) {
                wchar_t wc = static_cast<wchar_t>(wparam);
                send_event("char", {
                    {"window", SValue::Num(window_id)},
                    {"key", SValue::Num((double)wparam)},
                    {"text", SValue::Str(wide_to_utf8(std::wstring(1, wc)))}
                });
            }
            break;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            handle_window_destroy(hwnd);
            return 0;
        default:
            break;
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }

private:
    static constexpr const wchar_t* kWindowClass = L"ssiLangGui32Window";
    static constexpr const wchar_t* kCanvasClass = L"ssiLangGui32Canvas";

    SOCKET listen_socket_ = INVALID_SOCKET;
    SOCKET client_socket_ = INVALID_SOCKET;
    bool running_ = true;
    bool shutdown_after_response_ = false;
    std::string input_buffer_;
    HFONT default_font_ = nullptr;

    int next_window_id_ = 1;
    int next_control_id_ = 1;
    int next_command_id_ = 1000;
    int next_timer_id_ = 1;

    std::unordered_map<int, std::shared_ptr<WindowState>> windows_;
    std::unordered_map<int, std::shared_ptr<ControlState>> controls_;
    std::unordered_map<int, int> command_to_control_;
    std::unordered_map<int, MenuState> command_to_menu_;
    std::unordered_map<int, TimerState> timers_;
    std::unordered_map<int, CanvasState> canvases_;
    std::unordered_map<HWND, int> canvas_hwnds_;

    LRESULT on_canvas_message(int control_id, HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
        auto it = canvases_.find(control_id);
        if (it == canvases_.end()) return DefWindowProcW(hwnd, message, wparam, lparam);
        auto& canvas = it->second;
        auto control = need_control(control_id);

        switch (message) {
        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC hdc = BeginPaint(hwnd, &ps);
            BitBlt(hdc, 0, 0, canvas.width, canvas.height, canvas.mem_dc, 0, 0, SRCCOPY);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_MOUSEMOVE: {
            if (!canvas.tracking_mouse) {
                TRACKMOUSEEVENT tme{};
                tme.cbSize = sizeof(tme);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = hwnd;
                TrackMouseEvent(&tme);
                canvas.tracking_mouse = true;
                send_event("mouse_enter", {
                    {"window", SValue::Num(canvas.window_id)},
                    {"control", SValue::Num(control_id)},
                    {"token", SValue::Str(control->token)}
                });
            }
            send_event("mouse_move", {
                {"window", SValue::Num(canvas.window_id)},
                {"control", SValue::Num(control_id)},
                {"token", SValue::Str(control->token)},
                {"x", SValue::Num((double)GET_X_LPARAM(lparam))},
                {"y", SValue::Num((double)GET_Y_LPARAM(lparam))}
            });
            return 0;
        }
        case WM_MOUSELEAVE:
            canvas.tracking_mouse = false;
            send_event("mouse_leave", {
                {"window", SValue::Num(canvas.window_id)},
                {"control", SValue::Num(control_id)},
                {"token", SValue::Str(control->token)}
            });
            return 0;
        case WM_LBUTTONDOWN:
            SetFocus(hwnd);
            send_event("mouse_down", {
                {"window", SValue::Num(canvas.window_id)},
                {"control", SValue::Num(control_id)},
                {"token", SValue::Str(control->token)},
                {"x", SValue::Num((double)GET_X_LPARAM(lparam))},
                {"y", SValue::Num((double)GET_Y_LPARAM(lparam))},
                {"button", SValue::Str("left")}
            });
            send_event("canvas_click", {
                {"window", SValue::Num(canvas.window_id)},
                {"control", SValue::Num(control_id)},
                {"token", SValue::Str(control->token)},
                {"x", SValue::Num((double)GET_X_LPARAM(lparam))},
                {"y", SValue::Num((double)GET_Y_LPARAM(lparam))}
            });
            return 0;
        case WM_LBUTTONUP:
            send_event("mouse_up", {
                {"window", SValue::Num(canvas.window_id)},
                {"control", SValue::Num(control_id)},
                {"token", SValue::Str(control->token)},
                {"x", SValue::Num((double)GET_X_LPARAM(lparam))},
                {"y", SValue::Num((double)GET_Y_LPARAM(lparam))},
                {"button", SValue::Str("left")}
            });
            return 0;
        case WM_RBUTTONDOWN:
            SetFocus(hwnd);
            send_event("mouse_down", {
                {"window", SValue::Num(canvas.window_id)},
                {"control", SValue::Num(control_id)},
                {"token", SValue::Str(control->token)},
                {"x", SValue::Num((double)GET_X_LPARAM(lparam))},
                {"y", SValue::Num((double)GET_Y_LPARAM(lparam))},
                {"button", SValue::Str("right")}
            });
            return 0;
        case WM_RBUTTONUP:
            send_event("mouse_up", {
                {"window", SValue::Num(canvas.window_id)},
                {"control", SValue::Num(control_id)},
                {"token", SValue::Str(control->token)},
                {"x", SValue::Num((double)GET_X_LPARAM(lparam))},
                {"y", SValue::Num((double)GET_Y_LPARAM(lparam))},
                {"button", SValue::Str("right")}
            });
            return 0;
        case WM_SETFOCUS:
            send_event("focus", {
                {"window", SValue::Num(canvas.window_id)},
                {"control", SValue::Num(control_id)},
                {"token", SValue::Str(control->token)}
            });
            return 0;
        case WM_KILLFOCUS:
            send_event("blur", {
                {"window", SValue::Num(canvas.window_id)},
                {"control", SValue::Num(control_id)},
                {"token", SValue::Str(control->token)}
            });
            return 0;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            send_event("key_down", {
                {"window", SValue::Num(canvas.window_id)},
                {"control", SValue::Num(control_id)},
                {"token", SValue::Str(control->token)},
                {"key", SValue::Num((double)wparam)},
                {"repeat", SValue::Bool((lparam & 0x40000000) != 0)}
            });
            return 0;
        case WM_KEYUP:
        case WM_SYSKEYUP:
            send_event("key_up", {
                {"window", SValue::Num(canvas.window_id)},
                {"control", SValue::Num(control_id)},
                {"token", SValue::Str(control->token)},
                {"key", SValue::Num((double)wparam)}
            });
            return 0;
        case WM_CHAR: {
            wchar_t wc = static_cast<wchar_t>(wparam);
            send_event("char", {
                {"window", SValue::Num(canvas.window_id)},
                {"control", SValue::Num(control_id)},
                {"token", SValue::Str(control->token)},
                {"key", SValue::Num((double)wparam)},
                {"text", SValue::Str(wide_to_utf8(std::wstring(1, wc)))}
            });
            return 0;
        }
        default:
            return DefWindowProcW(hwnd, message, wparam, lparam);
        }
    }

    void open_listener(uint16_t port) {
        listen_socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listen_socket_ == INVALID_SOCKET) fail("socket() failed: " + socket_error_text());

        u_long nonblocking = 1;
        ioctlsocket(listen_socket_, FIONBIO, &nonblocking);

        int yes = 1;
        setsockopt(listen_socket_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof(yes));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = htons(port);

        if (bind(listen_socket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
            fail("bind() failed on 127.0.0.1:" + std::to_string(port) + " err=" + socket_error_text());
        }
        if (listen(listen_socket_, 1) != 0) fail("listen() failed err=" + socket_error_text());
    }

    void register_window_class() {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.style = 0; // Intentionally avoid CS_DBLCLKS.
        wc.lpfnWndProc = &App::window_proc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = kWindowClass;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        if (!RegisterClassExW(&wc)) fail("RegisterClassExW failed");

        WNDCLASSEXW canvas_wc{};
        canvas_wc.cbSize = sizeof(canvas_wc);
        canvas_wc.style = 0; // Intentionally avoid CS_DBLCLKS.
        canvas_wc.lpfnWndProc = &App::window_proc;
        canvas_wc.hInstance = GetModuleHandleW(nullptr);
        canvas_wc.lpszClassName = kCanvasClass;
        canvas_wc.hCursor = LoadCursor(nullptr, IDC_CROSS);
        canvas_wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        if (!RegisterClassExW(&canvas_wc)) fail("RegisterClassExW canvas failed");

        default_font_ = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    }

    void pump_messages() {
        MSG msg{};
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    void poll_network() {
        fd_set read_set;
        FD_ZERO(&read_set);
        SOCKET max_fd = 0;
        if (listen_socket_ != INVALID_SOCKET) {
            FD_SET(listen_socket_, &read_set);
            max_fd = std::max(max_fd, listen_socket_);
        }
        if (client_socket_ != INVALID_SOCKET) {
            FD_SET(client_socket_, &read_set);
            max_fd = std::max(max_fd, client_socket_);
        }

        timeval tv{};
        tv.tv_usec = 10000;
        int rc = select(static_cast<int>(max_fd + 1), &read_set, nullptr, nullptr, &tv);
        if (rc == SOCKET_ERROR) return;

        if (listen_socket_ != INVALID_SOCKET && FD_ISSET(listen_socket_, &read_set)) accept_client();
        if (client_socket_ != INVALID_SOCKET && FD_ISSET(client_socket_, &read_set)) read_client();
    }

    void accept_client() {
        sockaddr_in client_addr{};
        int len = sizeof(client_addr);
        SOCKET sock = accept(listen_socket_, reinterpret_cast<sockaddr*>(&client_addr), &len);
        if (sock == INVALID_SOCKET) return;

        close_client();
        destroy_all_windows();

        u_long nonblocking = 1;
        ioctlsocket(sock, FIONBIO, &nonblocking);
        client_socket_ = sock;
        input_buffer_.clear();
        shutdown_after_response_ = false;
    }

    void read_client() {
        char buf[4096];
        while (true) {
            int n = recv(client_socket_, buf, sizeof(buf), 0);
            if (n > 0) {
                input_buffer_.append(buf, buf + n);
                process_frames();
                continue;
            }
            if (n == 0) {
                close_client();
                destroy_all_windows();
                return;
            }
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK) return;
            close_client();
            destroy_all_windows();
            return;
        }
    }

    void process_frames() {
        while (input_buffer_.size() >= 4) {
            uint32_t len = read_u32_le(input_buffer_.data());
            if (input_buffer_.size() < 4u + len) return;

            std::string payload = input_buffer_.substr(4, len);
            input_buffer_.erase(0, 4u + len);

            try {
                handle_request(deserialize(payload));
            } catch (const std::exception& e) {
                send_error_response(0, e.what());
            }
        }
    }

    void send_frame(const SValue& value) {
        if (client_socket_ == INVALID_SOCKET) return;

        std::string payload = serialize(value);
        std::string frame;
        frame.reserve(4 + payload.size());
        write_u32_le(frame, static_cast<uint32_t>(payload.size()));
        frame += payload;

        size_t sent = 0;
        while (sent < frame.size()) {
            int n = send(client_socket_, frame.data() + sent, static_cast<int>(frame.size() - sent), 0);
            if (n == SOCKET_ERROR) {
                int err = WSAGetLastError();
                if (err == WSAEWOULDBLOCK) {
                    Sleep(1);
                    continue;
                }
                close_client();
                return;
            }
            sent += static_cast<size_t>(n);
        }
    }

    void send_response(int req_id, std::vector<std::pair<std::string, SValue>> extra = {}) {
        std::vector<std::pair<std::string, SValue>> out;
        out.push_back({"kind", SValue::Str("resp")});
        out.push_back({"req_id", SValue::Num(req_id)});
        out.push_back({"ok", SValue::Bool(true)});
        for (auto& entry : extra) out.push_back(std::move(entry));
        send_frame(SValue::Map(std::move(out)));
        if (shutdown_after_response_) running_ = false;
    }

    void send_error_response(int req_id, const std::string& message) {
        send_frame(SValue::Map({
            {"kind", SValue::Str("resp")},
            {"req_id", SValue::Num(req_id)},
            {"ok", SValue::Bool(false)},
            {"error", SValue::Str(message)}
        }));
    }

    void send_event(std::vector<std::pair<std::string, SValue>> extra) {
        std::vector<std::pair<std::string, SValue>> out;
        out.push_back({"kind", SValue::Str("event")});
        for (auto& entry : extra) out.push_back(std::move(entry));
        send_frame(SValue::Map(std::move(out)));
    }

    void send_event(const std::string& event_name, std::vector<std::pair<std::string, SValue>> extra) {
        extra.insert(extra.begin(), {"event", SValue::Str(event_name)});
        send_event(std::move(extra));
    }

    void send_close_event(int window_id) {
        send_event({
            {"event", SValue::Str("close")},
            {"window", SValue::Num(window_id)}
        });
    }

    void handle_request(const SValue& msg) {
        if (msg.type != SValue::MAP) fail("request must be a map");
        if (need_string(msg, "kind") != "req") fail("unexpected message kind");

        int req_id = static_cast<int>(need_number(msg, "req_id"));
        std::string op = need_string(msg, "op");

        try {
            if (op == "ping") {
                send_response(req_id, {{"value", SValue::Str("pong")}});
                return;
            }
            if (op == "create_window") {
                send_response(req_id, {{"window", SValue::Num(create_window(msg))}});
                return;
            }
            if (op == "show") {
                auto window = need_window(static_cast<int>(need_number(msg, "window")));
                ShowWindow(window->hwnd, SW_SHOW);
                UpdateWindow(window->hwnd);
                send_response(req_id);
                return;
            }
            if (op == "hide") {
                ShowWindow(need_window(static_cast<int>(need_number(msg, "window")))->hwnd, SW_HIDE);
                send_response(req_id);
                return;
            }
            if (op == "close") {
                DestroyWindow(need_window(static_cast<int>(need_number(msg, "window")))->hwnd);
                send_response(req_id);
                return;
            }
            if (op == "set_title") {
                set_text(need_window(static_cast<int>(need_number(msg, "window")))->hwnd, need_string(msg, "text"));
                send_response(req_id);
                return;
            }
            if (op == "set_bounds") {
                auto window = need_window(static_cast<int>(need_number(msg, "window")));
                int x = opt_int(msg, "x", CW_USEDEFAULT);
                int y = opt_int(msg, "y", CW_USEDEFAULT);
                int width = opt_int(msg, "width", 640);
                int height = opt_int(msg, "height", 480);
                MoveWindow(window->hwnd, x, y, width, height, TRUE);
                send_response(req_id);
                return;
            }
            if (op == "get_title") {
                send_response(req_id, {{"text", SValue::Str(get_text(need_window(static_cast<int>(need_number(msg, "window")))->hwnd))}});
                return;
            }
            if (op == "label") {
                send_response(req_id, {{"control", SValue::Num(create_control(msg, "label"))}});
                return;
            }
            if (op == "button") {
                send_response(req_id, {{"control", SValue::Num(create_control(msg, "button"))}});
                return;
            }
            if (op == "input") {
                send_response(req_id, {{"control", SValue::Num(create_control(msg, "input"))}});
                return;
            }
            if (op == "textarea") {
                send_response(req_id, {{"control", SValue::Num(create_control(msg, "textarea"))}});
                return;
            }
            if (op == "checkbox") {
                send_response(req_id, {{"control", SValue::Num(create_control(msg, "checkbox"))}});
                return;
            }
            if (op == "listbox") {
                send_response(req_id, {{"control", SValue::Num(create_control(msg, "listbox"))}});
                return;
            }
            if (op == "groupbox") {
                send_response(req_id, {{"control", SValue::Num(create_control(msg, "groupbox"))}});
                return;
            }
            if (op == "canvas") {
                send_response(req_id, {{"control", SValue::Num(create_control(msg, "canvas"))}});
                return;
            }
            if (op == "set_text") {
                set_text(need_control(static_cast<int>(need_number(msg, "control")))->hwnd, need_string(msg, "text"));
                send_response(req_id);
                return;
            }
            if (op == "get_text") {
                send_response(req_id, {{"text", SValue::Str(get_text(need_control(static_cast<int>(need_number(msg, "control")))->hwnd))}});
                return;
            }
            if (op == "set_enabled") {
                auto control = need_control(static_cast<int>(need_number(msg, "control")));
                EnableWindow(control->hwnd, opt_bool(msg, "enabled", true));
                send_response(req_id);
                return;
            }
            if (op == "focus") {
                SetFocus(need_control(static_cast<int>(need_number(msg, "control")))->hwnd);
                send_response(req_id);
                return;
            }
            if (op == "set_checked") {
                auto control = need_control(static_cast<int>(need_number(msg, "control")));
                SendMessageW(control->hwnd, BM_SETCHECK, opt_bool(msg, "checked", false) ? BST_CHECKED : BST_UNCHECKED, 0);
                send_response(req_id);
                return;
            }
            if (op == "get_checked") {
                auto control = need_control(static_cast<int>(need_number(msg, "control")));
                send_response(req_id, {{"checked", SValue::Bool(SendMessageW(control->hwnd, BM_GETCHECK, 0, 0) == BST_CHECKED)}});
                return;
            }
            if (op == "list_set_items") {
                auto control = need_control(static_cast<int>(need_number(msg, "control")));
                list_set_items(control->hwnd, string_list(need_list(msg, "items")));
                send_response(req_id);
                return;
            }
            if (op == "list_get_items") {
                auto control = need_control(static_cast<int>(need_number(msg, "control")));
                send_response(req_id, {{"items", SValue::List(list_items(control->hwnd))}});
                return;
            }
            if (op == "list_add_item") {
                auto control = need_control(static_cast<int>(need_number(msg, "control")));
                SendMessageW(control->hwnd, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(utf8_to_wide(need_string(msg, "text")).c_str()));
                send_response(req_id);
                return;
            }
            if (op == "list_set_selected") {
                auto control = need_control(static_cast<int>(need_number(msg, "control")));
                SendMessageW(control->hwnd, LB_SETCURSEL, static_cast<WPARAM>(opt_int(msg, "selected", -1)), 0);
                send_response(req_id);
                return;
            }
            if (op == "list_get_selected") {
                auto control = need_control(static_cast<int>(need_number(msg, "control")));
                send_response(req_id, {{"selected", SValue::Num((double)SendMessageW(control->hwnd, LB_GETCURSEL, 0, 0))}});
                return;
            }
            if (op == "set_menu") {
                set_menu(need_window(static_cast<int>(need_number(msg, "window"))), need_list(msg, "items"));
                send_response(req_id);
                return;
            }
            if (op == "set_timer") {
                send_response(req_id, {{"timer", SValue::Num(set_timer(msg))}});
                return;
            }
            if (op == "kill_timer") {
                kill_timer(static_cast<int>(need_number(msg, "timer")));
                send_response(req_id);
                return;
            }
            if (op == "canvas_clear") {
                canvas_clear(static_cast<int>(need_number(msg, "control")), opt_color(msg, "color", RGB(255, 255, 255)));
                send_response(req_id);
                return;
            }
            if (op == "canvas_line") {
                canvas_line(msg);
                send_response(req_id);
                return;
            }
            if (op == "canvas_rect") {
                canvas_rect(msg);
                send_response(req_id);
                return;
            }
            if (op == "canvas_text") {
                canvas_text(msg);
                send_response(req_id);
                return;
            }
            if (op == "message") {
                show_message(msg);
                send_response(req_id);
                return;
            }
            if (op == "shutdown") {
                destroy_all_windows();
                shutdown_after_response_ = true;
                send_response(req_id);
                return;
            }
            fail("unknown op: " + op);
        } catch (const std::exception& e) {
            send_error_response(req_id, e.what());
        }
    }

    std::shared_ptr<WindowState> need_window(int id) {
        auto it = windows_.find(id);
        if (it == windows_.end()) fail("unknown window id");
        return it->second;
    }

    std::shared_ptr<ControlState> need_control(int id) {
        auto it = controls_.find(id);
        if (it == controls_.end()) fail("unknown control id");
        return it->second;
    }

    int create_window(const SValue& msg) {
        int width = static_cast<int>(need_number(msg, "width"));
        int height = static_cast<int>(need_number(msg, "height"));
        if (width < 120 || height < 80) fail("window size too small");
        int x = opt_int(msg, "x", CW_USEDEFAULT);
        int y = opt_int(msg, "y", CW_USEDEFAULT);
        bool resizable = opt_bool(msg, "resizable", true);
        bool visible = opt_bool(msg, "visible", false);

        int id = next_window_id_++;
        auto window = std::make_shared<WindowState>();
        window->id = id;

        DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
        if (resizable) style |= WS_THICKFRAME | WS_MAXIMIZEBOX;
        if (visible) style |= WS_VISIBLE;

        HWND hwnd = CreateWindowExW(
            0,
            kWindowClass,
            utf8_to_wide(need_string(msg, "title")).c_str(),
            style,
            x,
            y,
            width,
            height,
            nullptr,
            nullptr,
            GetModuleHandleW(nullptr),
            nullptr
        );
        if (!hwnd) fail("CreateWindowExW failed");

        SetWindowLongPtrW(hwnd, GWLP_USERDATA, static_cast<LONG_PTR>(id));
        window->hwnd = hwnd;
        windows_[id] = window;
        return id;
    }

    int create_control(const SValue& msg, const std::string& kind) {
        auto window = need_window(static_cast<int>(need_number(msg, "window")));
        int x = static_cast<int>(need_number(msg, "x"));
        int y = static_cast<int>(need_number(msg, "y"));
        int width = static_cast<int>(need_number(msg, "width"));
        int height = static_cast<int>(need_number(msg, "height"));
        std::wstring text = utf8_to_wide(need_string(msg, "text"));

        DWORD style = WS_CHILD | WS_VISIBLE;
        DWORD ex_style = 0;
        const wchar_t* class_name = L"STATIC";
        int command_id = 0;
        std::string token = opt_string(msg, "token");
        bool readonly = opt_bool(msg, "readonly", false);

        if (kind == "button") {
            class_name = L"BUTTON";
            style |= WS_TABSTOP | BS_PUSHBUTTON;
            command_id = next_command_id_++;
        } else if (kind == "checkbox") {
            class_name = L"BUTTON";
            style |= WS_TABSTOP | BS_AUTOCHECKBOX;
            command_id = next_command_id_++;
        } else if (kind == "input") {
            class_name = L"EDIT";
            ex_style = WS_EX_CLIENTEDGE;
            style |= ES_AUTOHSCROLL | WS_TABSTOP;
        } else if (kind == "textarea") {
            class_name = L"EDIT";
            ex_style = WS_EX_CLIENTEDGE;
            style |= ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | WS_TABSTOP;
        } else if (kind == "listbox") {
            class_name = L"LISTBOX";
            style |= LBS_NOTIFY | WS_VSCROLL | WS_BORDER | WS_TABSTOP;
            command_id = next_command_id_++;
        } else if (kind == "groupbox") {
            class_name = L"BUTTON";
            style |= BS_GROUPBOX;
        } else if (kind == "canvas") {
            class_name = kCanvasClass;
            style |= WS_BORDER;
        }

        HWND hwnd = CreateWindowExW(
            ex_style,
            class_name,
            text.c_str(),
            style,
            x,
            y,
            width,
            height,
            window->hwnd,
            command_id == 0 ? nullptr : reinterpret_cast<HMENU>(static_cast<INT_PTR>(command_id)),
            GetModuleHandleW(nullptr),
            nullptr
        );
        if (!hwnd) fail("CreateWindowExW control failed");
        if (default_font_) SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(default_font_), TRUE);
        if ((kind == "input" || kind == "textarea") && readonly) {
            SendMessageW(hwnd, EM_SETREADONLY, TRUE, 0);
        }
        if (kind == "checkbox" && opt_bool(msg, "checked", false)) {
            SendMessageW(hwnd, BM_SETCHECK, BST_CHECKED, 0);
        }
        if (kind == "listbox") {
            const SValue* items = map_get(msg, "items");
            if (items && items->type == SValue::LIST) {
                list_set_items(hwnd, string_list(*items));
            }
            SendMessageW(hwnd, LB_SETCURSEL, static_cast<WPARAM>(opt_int(msg, "selected", -1)), 0);
        }

        int id = next_control_id_++;
        auto control = std::make_shared<ControlState>();
        control->id = id;
        control->window_id = window->id;
        control->command_id = command_id;
        control->hwnd = hwnd;
        control->kind = kind;
        control->token = token;

        controls_[id] = control;
        window->controls.push_back(id);
        if (command_id != 0) command_to_control_[command_id] = id;
        if (kind == "canvas") {
            HDC screen_dc = GetDC(nullptr);
            HDC mem_dc = CreateCompatibleDC(screen_dc);
            HBITMAP bitmap = CreateCompatibleBitmap(screen_dc, width, height);
            ReleaseDC(nullptr, screen_dc);
            if (!mem_dc || !bitmap) fail("Create canvas surface failed");
            HGDIOBJ old_bitmap = SelectObject(mem_dc, bitmap);
            canvases_[id] = CanvasState{id, window->id, hwnd, mem_dc, bitmap, old_bitmap, width, height, opt_color(msg, "color", RGB(255, 255, 255))};
            canvas_hwnds_[hwnd] = id;
            canvas_clear(id, opt_color(msg, "color", RGB(255, 255, 255)));
        }
        return id;
    }

    void list_set_items(HWND hwnd, const std::vector<std::string>& items) {
        SendMessageW(hwnd, LB_RESETCONTENT, 0, 0);
        for (const auto& item : items) {
            std::wstring wide = utf8_to_wide(item);
            SendMessageW(hwnd, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wide.c_str()));
        }
    }

    std::vector<SValue> list_items(HWND hwnd) {
        int count = static_cast<int>(SendMessageW(hwnd, LB_GETCOUNT, 0, 0));
        if (count < 0) return {};
        std::vector<SValue> out;
        out.reserve(count);
        for (int i = 0; i < count; ++i) {
            int len = static_cast<int>(SendMessageW(hwnd, LB_GETTEXTLEN, i, 0));
            std::wstring buf(static_cast<size_t>(len) + 1, L'\0');
            SendMessageW(hwnd, LB_GETTEXT, i, reinterpret_cast<LPARAM>(buf.data()));
            if (!buf.empty() && buf.back() == L'\0') buf.pop_back();
            out.push_back(SValue::Str(wide_to_utf8(buf)));
        }
        return out;
    }

    HMENU build_menu_recursive(int window_id, const SValue& items, bool popup, std::vector<int>& command_ids) {
        if (items.type != SValue::LIST) fail("menu items must be list");
        HMENU menu = popup ? CreatePopupMenu() : CreateMenu();
        if (!menu) fail("CreateMenu failed");

        for (const auto& item : items.list) {
            if (item.type != SValue::MAP) fail("menu item must be map");
            if (opt_bool(item, "sep", false)) {
                AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
                continue;
            }

            std::wstring text = utf8_to_wide(need_string(item, "text"));
            const SValue* children = map_get(item, "children");
            if (children && children->type == SValue::LIST) {
                HMENU sub = build_menu_recursive(window_id, *children, true, command_ids);
                AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(sub), text.c_str());
            } else {
                int command_id = next_command_id_++;
                command_ids.push_back(command_id);
                command_to_menu_[command_id] = MenuState{window_id, opt_string(item, "token", wide_to_utf8(text))};
                AppendMenuW(menu, MF_STRING, command_id, text.c_str());
            }
        }

        return menu;
    }

    void clear_menu(std::shared_ptr<WindowState> window) {
        for (int command_id : window->menu_commands) {
            command_to_menu_.erase(command_id);
        }
        window->menu_commands.clear();
        if (window->menu) {
            SetMenu(window->hwnd, nullptr);
            DestroyMenu(window->menu);
            window->menu = nullptr;
        }
    }

    void set_menu(std::shared_ptr<WindowState> window, const SValue& items) {
        clear_menu(window);
        window->menu = build_menu_recursive(window->id, items, false, window->menu_commands);
        SetMenu(window->hwnd, window->menu);
        DrawMenuBar(window->hwnd);
    }

    double set_timer(const SValue& msg) {
        auto window = need_window(static_cast<int>(need_number(msg, "window")));
        int ms = static_cast<int>(need_number(msg, "ms"));
        if (ms < 1) fail("timer ms must be positive");
        int timer_id = next_timer_id_++;
        if (!SetTimer(window->hwnd, static_cast<UINT_PTR>(timer_id), static_cast<UINT>(ms), nullptr)) {
            fail("SetTimer failed");
        }
        timers_[timer_id] = TimerState{timer_id, window->id, static_cast<UINT_PTR>(timer_id), opt_string(msg, "token")};
        return static_cast<double>(timer_id);
    }

    void kill_timer(int timer_id) {
        auto it = timers_.find(timer_id);
        if (it == timers_.end()) return;
        auto win_it = windows_.find(it->second.window_id);
        if (win_it != windows_.end() && IsWindow(win_it->second->hwnd)) {
            KillTimer(win_it->second->hwnd, it->second.native_id);
        }
        timers_.erase(it);
    }

    CanvasState& need_canvas(int control_id) {
        auto it = canvases_.find(control_id);
        if (it == canvases_.end()) fail("unknown canvas control");
        return it->second;
    }

    void refresh_canvas(CanvasState& canvas) {
        InvalidateRect(canvas.hwnd, nullptr, FALSE);
        UpdateWindow(canvas.hwnd);
    }

    void canvas_clear(int control_id, COLORREF color) {
        auto& canvas = need_canvas(control_id);
        canvas.back_color = color;
        HBRUSH brush = CreateSolidBrush(color);
        RECT rc{0, 0, canvas.width, canvas.height};
        FillRect(canvas.mem_dc, &rc, brush);
        DeleteObject(brush);
        refresh_canvas(canvas);
    }

    void canvas_line(const SValue& msg) {
        auto& canvas = need_canvas(static_cast<int>(need_number(msg, "control")));
        HPEN pen = CreatePen(PS_SOLID, std::max(1, opt_int(msg, "line_width", 1)), opt_color(msg, "color", RGB(0, 0, 0)));
        HGDIOBJ old_pen = SelectObject(canvas.mem_dc, pen);
        MoveToEx(canvas.mem_dc, opt_int(msg, "x1"), opt_int(msg, "y1"), nullptr);
        LineTo(canvas.mem_dc, opt_int(msg, "x2"), opt_int(msg, "y2"));
        SelectObject(canvas.mem_dc, old_pen);
        DeleteObject(pen);
        refresh_canvas(canvas);
    }

    void canvas_rect(const SValue& msg) {
        auto& canvas = need_canvas(static_cast<int>(need_number(msg, "control")));
        COLORREF line_color = opt_color(msg, "line_color", RGB(0, 0, 0));
        COLORREF fill_color = opt_color(msg, "fill_color", RGB(255, 255, 255));
        bool fill = opt_bool(msg, "fill", true);
        HPEN pen = CreatePen(PS_SOLID, std::max(1, opt_int(msg, "line_width", 1)), line_color);
        HBRUSH brush = fill ? CreateSolidBrush(fill_color) : reinterpret_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
        HGDIOBJ old_pen = SelectObject(canvas.mem_dc, pen);
        HGDIOBJ old_brush = SelectObject(canvas.mem_dc, brush);
        Rectangle(
            canvas.mem_dc,
            opt_int(msg, "x"),
            opt_int(msg, "y"),
            opt_int(msg, "x") + opt_int(msg, "width"),
            opt_int(msg, "y") + opt_int(msg, "height")
        );
        SelectObject(canvas.mem_dc, old_pen);
        SelectObject(canvas.mem_dc, old_brush);
        DeleteObject(pen);
        if (fill) DeleteObject(brush);
        refresh_canvas(canvas);
    }

    void canvas_text(const SValue& msg) {
        auto& canvas = need_canvas(static_cast<int>(need_number(msg, "control")));
        COLORREF color = opt_color(msg, "color", RGB(0, 0, 0));
        SetBkMode(canvas.mem_dc, TRANSPARENT);
        SetTextColor(canvas.mem_dc, color);
        std::wstring text = utf8_to_wide(need_string(msg, "text"));
        TextOutW(canvas.mem_dc, opt_int(msg, "x"), opt_int(msg, "y"), text.c_str(), static_cast<int>(text.size()));
        refresh_canvas(canvas);
    }

    void set_text(HWND hwnd, const std::string& text) {
        std::wstring wide = utf8_to_wide(text);
        if (!SetWindowTextW(hwnd, wide.c_str())) fail("SetWindowTextW failed");
    }

    std::string get_text(HWND hwnd) {
        int len = GetWindowTextLengthW(hwnd);
        std::wstring buf(static_cast<size_t>(len) + 1, L'\0');
        GetWindowTextW(hwnd, buf.data(), len + 1);
        if (!buf.empty() && buf.back() == L'\0') buf.pop_back();
        return wide_to_utf8(buf);
    }

    void show_message(const SValue& msg) {
        HWND owner = nullptr;
        const SValue* window_value = map_get(msg, "window");
        if (window_value && window_value->type == SValue::NUM) {
            owner = need_window(static_cast<int>(window_value->num))->hwnd;
        }
        MessageBoxW(
            owner,
            utf8_to_wide(need_string(msg, "text")).c_str(),
            utf8_to_wide(opt_string(msg, "title", "gui32")).c_str(),
            MB_OK | MB_ICONINFORMATION
        );
    }

    void handle_window_destroy(HWND hwnd) {
        int window_id = static_cast<int>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        auto it = windows_.find(window_id);
        if (it == windows_.end()) return;

        clear_menu(it->second);

        std::vector<int> kill_ids;
        for (const auto& entry : timers_) {
            if (entry.second.window_id == window_id) kill_ids.push_back(entry.first);
        }
        for (int timer_id : kill_ids) kill_timer(timer_id);

        for (int control_id : it->second->controls) {
            auto ctl_it = controls_.find(control_id);
            if (ctl_it != controls_.end()) {
                if (ctl_it->second->command_id != 0) command_to_control_.erase(ctl_it->second->command_id);
                auto canvas_it = canvases_.find(control_id);
                if (canvas_it != canvases_.end()) {
                    canvas_hwnds_.erase(canvas_it->second.hwnd);
                    SelectObject(canvas_it->second.mem_dc, canvas_it->second.old_bitmap);
                    DeleteObject(canvas_it->second.bitmap);
                    DeleteDC(canvas_it->second.mem_dc);
                    canvases_.erase(canvas_it);
                }
                controls_.erase(ctl_it);
            }
        }

        windows_.erase(it);
        send_close_event(window_id);
    }

    void destroy_all_windows() {
        std::vector<HWND> hwnds;
        hwnds.reserve(windows_.size());
        for (const auto& entry : windows_) if (entry.second->hwnd) hwnds.push_back(entry.second->hwnd);
        for (HWND hwnd : hwnds) if (IsWindow(hwnd)) DestroyWindow(hwnd);
        windows_.clear();
        controls_.clear();
        command_to_control_.clear();
        command_to_menu_.clear();
        timers_.clear();
        canvases_.clear();
        canvas_hwnds_.clear();
    }

    void close_client() {
        if (client_socket_ != INVALID_SOCKET) {
            closesocket(client_socket_);
            client_socket_ = INVALID_SOCKET;
        }
        input_buffer_.clear();
    }

    static LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
        if (!g_app) return DefWindowProcW(hwnd, message, wparam, lparam);
        return g_app->on_window_message(hwnd, message, wparam, lparam);
    }

};

} // namespace

int main(int argc, char** argv) {
    uint16_t port = 32123;
    if (argc > 1) port = static_cast<uint16_t>(std::stoi(argv[1]));

    WSADATA wd{};
    if (WSAStartup(MAKEWORD(2, 2), &wd) != 0) {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }

    int exit_code = 0;
    try {
        App app;
        g_app = &app;
        app.run(port);
        g_app = nullptr;
    } catch (const std::exception& e) {
        g_app = nullptr;
        std::cerr << e.what() << "\n";
        exit_code = 1;
    }

    WSACleanup();
    return exit_code;
}
