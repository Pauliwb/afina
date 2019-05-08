#ifndef AFINA_NETWORK_ST_NONBLOCKING_CONNECTION_H
#define AFINA_NETWORK_ST_NONBLOCKING_CONNECTION_H

#include <cstring>
#include <iostream>
#include <list>
#include <sys/epoll.h>

// X_X
#include "protocol/Parser.h"
#include <afina/Storage.h>
#include <afina/execute/Command.h>
// X_X

namespace Afina {
namespace Network {
namespace STnonblock {

class Connection {
public:
    // X_X
    Connection(int s, std::shared_ptr<spdlog::logger> log, std::shared_ptr<Afina::Storage> ps)
        : _socket(s), _logger(log), pStorage(ps) {
        std::memset(&_event, 0, sizeof(struct epoll_event));
        _event.data.ptr = this;
    }
    // X_X

    inline bool isAlive() const { return true; }

    void Start();

protected:
    void OnError();
    void OnClose();
    void DoRead();
    void DoWrite();

private:
    friend class ServerImpl;

    int _socket;
    struct epoll_event _event;

    // X_X
    std::shared_ptr<spdlog::logger> _logger;
    std::shared_ptr<Afina::Storage> pStorage;

    bool _live;

    std::list<std::string> _write_buffers;
    std::size_t _write_offset;
    char _read_buffer[4096];
    int readed_bytes;

    Protocol::Parser parser;
    std::size_t arg_remains;
    std::string argument_for_command;
    std::unique_ptr<Execute::Command> command_to_execute;
    // X_X

};

} // namespace STnonblock
} // namespace Network
} // namespace Afina

#endif // AFINA_NETWORK_ST_NONBLOCKING_CONNECTION_H
