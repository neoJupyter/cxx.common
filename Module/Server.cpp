#include "NetUtils.h"
#include <kls/Format.h>
#include <kls/coroutine/Operation.h>

using namespace njp;
using namespace kls;
using namespace njp::detail;
using namespace kls::coroutine;

namespace {
    class Server: public IServer {
        struct Endpoint {
            ValueAsync<> executor;
            std::unique_ptr<phttp::ServerEndpoint> endpoint;
        };
    public:
        Server(io::Peer peer, int backlog);
        ValueAsync<> run() override;
        ValueAsync<> close() override;
        void handles_impl(std::string_view method, std::string_view resource, FnClosure fn) override;
    private:
        std::mutex m_lock{};
        std::atomic_bool m_active{true};
        std::unique_ptr<phttp::Host> m_host;
        std::unordered_map<void *, Endpoint> m_running{};
        std::unordered_map<std::string, std::function<ValueAsync<temp_json>(const temp_json &)>> m_handlers;
        ValueAsync<> serve_endpoint(phttp::ServerEndpoint *ep);
        ValueAsync<> serve_endpoint_inner(phttp::ServerEndpoint &ep);
        static phttp::Response create_success_response(phttp::Headers &&headers, phttp::Block &&result_block);
        static phttp::Response create_error_response(phttp::Headers &&headers, int code, std::string_view message);
    };

    Server::Server(io::Peer peer, int backlog) : m_host(phttp::listen_tcp(peer, backlog)) {}

    ValueAsync<> Server::run() {
        std::unique_lock lock{m_lock};
        while (m_active) {
            try {
                lock.unlock();
                auto peer = phttp::ServerEndpoint::create(co_await m_host->accept());
                lock.lock();
                if (m_active) {
                    auto peer_ptr = peer.get();
                    m_running[peer_ptr] = Endpoint{.executor = serve_endpoint(peer_ptr), .endpoint = std::move(peer)};
                } else try { co_await peer->close(); } catch (...) {} // server is closing
            }
            catch (...) { if (m_active) throw; }
        }
    }

    ValueAsync<> Server::close() {
        std::unordered_map<void *, Endpoint> table{};
        {
            std::lock_guard lk{m_lock};
            m_active = false;
            table = std::move(m_running); // grab the table for cleanup
            co_await m_host->close(); // this should cause any running accept function to fall immediately
        }
        std::vector<ValueAsync<>> async_final{};
        async_final.reserve(table.size());
        for (auto &&[_, e]: table) {
            // try to close everything nicely, but we do not care if it broke
            async_final.push_back([](Endpoint ep) -> ValueAsync<> {
                co_await Redispatch{};
                try { co_await ep.endpoint->close(); } catch (...) {}
                try { co_await std::move(ep.executor); } catch (...) {}
            }(std::move(e)));
        }
        co_await await_all(std::move(async_final)); // join all calls so no resource is leaked
    }

    void Server::handles_impl(std::string_view method, std::string_view resource, IServer::FnClosure fn) {
        m_handlers[format("{}{}", method, resource)] = fn;
    }

    ValueAsync<> Server::serve_endpoint(phttp::ServerEndpoint *ep) {
        co_await Redispatch{};
        try { co_await serve_endpoint_inner(*ep); } catch (...) {}
        std::unique_ptr<phttp::ServerEndpoint> to_close;
        {
            std::lock_guard lk{m_lock};
            if (m_active) { // if active flag is cancelled then close() is responsible for cleanup
                // move out the endpoint for async closing and erase the running record
                auto it = m_running.find(ep);
                to_close = std::move(it->second.endpoint);
                m_running.erase(it);
            }
        }
        if (to_close) co_await to_close->close();
    }

    ValueAsync<> Server::serve_endpoint_inner(phttp::ServerEndpoint &ep) {
        co_await ep.run([this](phttp::Request request) -> ValueAsync<phttp::Response> {
            auto &&[line, headers, body] = request;
            auto handler_it = m_handlers.find(format("{}{}", line.verb(), line.resource()));
            if (handler_it == m_handlers.end()) {
                co_return create_error_response(std::move(headers), 404, "Not Found");
            }
            try {
                const auto request_json = json_from_block<temp_json>(body);
                const auto response_json = co_await handler_it->second(request_json);
                co_return create_success_response(std::move(headers), json_to_block(response_json));
            }
            catch (ServerUserError &) {
                co_return create_error_response(std::move(headers), 400, "Bad Request: User Error");
            }
            catch (...) {
                co_return create_error_response(std::move(headers), 500, "Internal Server Error");
            }
        });
    }

    phttp::Response Server::create_success_response(phttp::Headers &&headers, phttp::Block &&result_block) {
        return phttp::Response{
                .line = phttp::ResponseLine(200, "OK", temp::resource()),
                .headers = std::move(headers), .body = std::move(result_block)
        };
    }

    phttp::Response Server::create_error_response(phttp::Headers &&headers, int code, std::string_view message) {
        return phttp::Response{
                .line = phttp::ResponseLine(code, message, temp::resource()),
                .headers = std::move(headers), .body = phttp::Block(0, temp::resource())
        };
    }
}

std::shared_ptr<IServer> IServer::create(io::Peer peer, int backlog) {
    return std::make_shared<Server>(peer, backlog);
}
