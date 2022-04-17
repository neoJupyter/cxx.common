#include "NetUtils.h"

using namespace njp;
using namespace kls;
using namespace njp::detail;
using namespace kls::coroutine;

namespace {
    class Client: public IClient {
    public:
        explicit Client(std::unique_ptr<kls::phttp::Endpoint> endpoint);
        coroutine::ValueAsync<temp_json> exec(std::string_view m, std::string_view p, temp_json request) override;
        coroutine::ValueAsync<> close() override;
    private:
        std::unique_ptr<kls::phttp::ClientEndpoint> m_endpoint;
    };
    
    Client::Client(std::unique_ptr<kls::phttp::Endpoint> endpoint) :
            m_endpoint(phttp::ClientEndpoint::create(std::move(endpoint))) {}

    ValueAsync<temp_json> Client::exec(std::string_view m, std::string_view p, temp_json request) {
        temp::string dump = request.dump();
        auto request_phttp = phttp::Request{
                .line = phttp::RequestLine(m, p, temp::resource()),
                .headers = phttp::Headers(temp::resource()),
                .body = json_to_block(request)
        };
        auto&&[line, _, body] = co_await m_endpoint->exec(std::move(request_phttp));
        const auto range = static_span_cast<char>(body.content());
        if (line.code() == 200) co_return temp_json::parse(std::string_view(range.begin(), range.end()));
        throw ClientRemoteError(m, p, dump, line.code(), line.message(), {range.begin(), range.end()});
    }

    ValueAsync<> Client::close() { return m_endpoint->close(); }
}

kls::coroutine::ValueAsync<std::shared_ptr<IClient>> IClient::create(kls::io::Peer peer) {
    co_return std::make_shared<Client>(co_await phttp::connect_tcp(peer));
}
