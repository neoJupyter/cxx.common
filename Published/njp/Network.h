#pragma once

#include "Json.h"
#include <functional>
#include <kls/io/IP.h>
#include <kls/coroutine/Async.h>

namespace njp {
    class ClientRemoteError : public std::exception {
    public:
        ClientRemoteError(
                std::string_view method, std::string_view path, std::string_view request,
                int code, std::string_view message, std::string_view response
        );
        [[nodiscard]] const char *what() const noexcept override;
        [[nodiscard]] auto method() const noexcept { return m_method; }
        [[nodiscard]] auto path() const noexcept { return m_path; }
        [[nodiscard]] auto request() const noexcept { return m_request; }
        [[nodiscard]] auto code() const noexcept { return m_code; };
        [[nodiscard]] auto message() const noexcept { return m_message; }
        [[nodiscard]] auto response() const noexcept { return m_response; }
    private:
        int m_code;
        std::string m_what_data;
        std::string_view m_method, m_path, m_request, m_message, m_response;
    };

    struct ServerUserError : std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    class ServerUserCodeError : std::runtime_error {
    public:
        ServerUserCodeError(int code, std::string_view response);
        [[nodiscard]] auto code() const noexcept { return m_code; };
        [[nodiscard]] auto response() const noexcept { return m_response; }
    private:
        int m_code;
        std::string m_response;
    };

    struct IClient: kls::PmrBase {
        virtual kls::coroutine::ValueAsync<temp_json> exec(std::string_view m, std::string_view p, temp_json r) = 0;
        virtual kls::coroutine::ValueAsync<> close() = 0;
        static kls::coroutine::ValueAsync<std::shared_ptr<IClient>> create(kls::io::Peer peer);
    };

    struct IServer: kls::PmrBase {
    protected:
        using FnClosure = std::function<kls::coroutine::ValueAsync<temp_json>(temp_json)>;
        virtual void handles_impl(std::string_view method, std::string_view resource, FnClosure fn) = 0;
    public:
        template<class Fn>
        requires requires(std::decay_t<Fn> fn, temp_json arg) {
            { fn(arg) } -> std::same_as<kls::coroutine::ValueAsync<temp_json>>;
        }
        void handles(std::string_view method, std::string_view resource, Fn fn) { handles_impl(method, resource, fn); }
        virtual kls::coroutine::ValueAsync<> run() = 0;
        virtual kls::coroutine::ValueAsync<> close() = 0;
        static std::shared_ptr<IServer> create(kls::io::Peer peer, int backlog);
    };
}