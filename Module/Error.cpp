#include "NetUtils.h"
#include "njp/Network.h"
#include <kls/Format.h>

using namespace njp;
using namespace kls;
using namespace njp::detail;
using namespace kls::coroutine;

static constexpr auto exception_lines = std::array<std::string_view, 7>{
        "Exception throw from remote endpoint:",
        "\nWith Method: ",
        "\nWith Path: ",
        "\nWith Request:\n",
        "\nGot Code: ",
        "\nGot Message: "
        "\nGet Response:\n",
};

constexpr std::array<size_t, 7> get_exception_lines_size() {
    std::array<size_t, 7> ret{0};
    ret[0] = exception_lines[0].size();
    for (int i = 1; i < 7; ++i) ret[i] = exception_lines[i].size() + ret[i - 1];
    return ret;
}

static constexpr auto exception_lines_size = get_exception_lines_size();

std::string_view append_get_second_unsafe(std::string &op, std::string_view first, std::string_view second) {
    const auto begin = (op.append(first), op.end());
    const auto end = (op.append(second), op.end());
    return {begin, end};
}

ClientRemoteError::ClientRemoteError(
        std::string_view method, std::string_view path, std::string_view request,
        int code, std::string_view message, std::string_view response
) {
    auto code_str = std::to_string(code);
    std::array<size_t, 7> args_size{
            0, method.size(), path.size(), request.size(),
            code_str.size(), message.size(), response.size()
    };
    for (int i = 1; i < 7; ++i) args_size[i] += args_size[i - 1];
    auto result = std::string();
    result.reserve(exception_lines_size[6] + args_size[6] + 16);
    result.append(exception_lines[0]);
    m_method = append_get_second_unsafe(result, exception_lines[1], method);
    m_path = append_get_second_unsafe(result, exception_lines[2], path);
    m_request = append_get_second_unsafe(result, exception_lines[3], request);
    append_get_second_unsafe(result, exception_lines[4], code_str);
    m_message = append_get_second_unsafe(result, exception_lines[5], message);
    m_response = append_get_second_unsafe(result, exception_lines[6], response);
    m_what_data = std::move(result);
    m_code = code;
}

const char *ClientRemoteError::what() const noexcept { return m_what_data.c_str(); }

ServerUserCodeError::ServerUserCodeError(int code, std::string_view response) :
        runtime_error(format("Request handling failed\nWith HttpCode: {}\nWith Response: {}", code, response)),
        m_code(code), m_response(response) {}
