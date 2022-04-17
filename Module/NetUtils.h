#pragma once

#include "njp/Network.h"
#include <kls/phttp/Protocol.h>

namespace njp::detail {
    template<class Json>
    Json json_from_block(const kls::phttp::Block& block) {
        const auto range = kls::static_span_cast<char>(block.content());
        return Json::parse(std::string_view(range.begin(), range.end()));
    }

    template<class Json>
    kls::phttp::Block json_to_block(const Json &json) {
        auto content = json.dump();
        auto block = kls::phttp::Block(int32_t(content.size()), kls::temp::resource());
        kls::copy(kls::Span<>{kls::Span<char>{content}}, block.content());
        return block;
    }
}
