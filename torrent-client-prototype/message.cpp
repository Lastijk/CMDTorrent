#include <netinet/in.h>
#include <stdexcept>
#include "message.h"
#include "byte_tools.h"

Message Message::Parse(const std::string &messageString) {
    if(messageString.empty()) return Init(MessageId::KeepAlive, "");
    Message parsed;
    std::string pd(messageString.begin() + 1, messageString.end());
    parsed.payload = pd;
    parsed.messageLength = parsed.payload.size() + 1;
    parsed.id = static_cast<MessageId>(messageString[0]);
    return parsed;
}

Message Message::Init(MessageId id, const std::string &payload) {
    if(id == MessageId::KeepAlive) return Message{id, 0, ""};
    return Message{id, 1 + payload.size(), payload};
}

std::string Message::ToString() const {
    std::string stroka;
    uint32_t len = static_cast<uint32_t>(messageLength);
    for (int i = 3; i >= 0; --i) stroka.push_back(static_cast<char>((len >> (i * 8)) & 0xFF));
    stroka.push_back(static_cast<char>(id));
    stroka.append(payload);
    return stroka;
}
