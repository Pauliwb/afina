#include "Connection.h"

#include <cassert>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <list> 

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/uio.h>

#include <spdlog/logger.h>
#include <afina/Storage.h>
#include <afina/execute/Command.h>
#include <afina/logging/Service.h>
#include "protocol/Parser.h"

namespace Afina {
namespace Network {
namespace STnonblock {


static constexpr int EVENT_READ = EPOLLIN | EPOLLRDHUP | EPOLLERR | EPOLLHUP | EPOLLET;
static constexpr int EVENT_READ_WRITE = EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLERR | EPOLLHUP | EPOLLET; 

// See Connection.h
void Connection::Start() {
    _logger->info("Start on descriptor {}", _socket);
    _live = true;

    _event.data.ptr = this;
    _event.events = EVENT_READ;

    readed_bytes = 0;
    _write_offset = 0;
    arg_remains = 0;

    command_to_execute = nullptr;
    parser = Protocol::Parser{};
    argument_for_command.clear();
    _write_buffers.clear();
}

// See Connection.h     
void Connection::OnError() {
    _live = false;

}

// See Connection.h
void Connection::OnClose() {
    _live = false;

}

// See Connection.h
void Connection::DoRead() {
    _logger->info("DoRead on descriptor {}", _socket)

    try {
        int readed_bytes_ = -1;

        while ((readed_bytes_ = read(_socket, _read_buffer + readed_bytes, sizeof(_read_buffer) - readed_bytes)) > 0) {

            _logger->debug("Got {} bytes from socket", readed_bytes_);

            readed_bytes += readed_bytes_;

            // Single block of data readed from the socket could trigger inside actions a multiple times,
            // for example:
            // - read#0: [<command1 start>]
            // - read#1: [<command1 end> <argument> <command2> <argument for command 2> <command3> ... ]
            while (readed_bytes > 0) {
                _logger->debug("Process {} bytes", readed_bytes);

                // There is no command yet
                if (!command_to_execute) {
                    std::size_t parsed = 0;
                    if (parser.Parse(_read_buffer, readed_bytes, parsed)) {
                        // There is no command to be launched, continue to parse input stream
                        // Here we are, current chunk finished some command, process it
                        _logger->debug("Found new command: {} in {} bytes", parser.Name(), parsed);
                        command_to_execute = parser.Build(arg_remains);
                        if (arg_remains > 0) {
                            arg_remains += 2;
                        }
                    }

                    // Parsed might fails to consume any bytes from input stream. In real life that could happens,
                    // for example, because we are working with UTF-16 chars and only 1 byte left in stream
                    if (parsed == 0) {
                        break;
                    } else {
                        std::memmove(_read_buffer, _read_buffer + parsed, readed_bytes - parsed);
                        readed_bytes -= parsed;
                    }
                }

                // There is command, but we still wait for argument to arrive...
                if (command_to_execute && arg_remains > 0) {
                    _logger->debug("Fill argument: {} bytes of {}", readed_bytes, arg_remains);
                    // There is some parsed command, and now we are reading argument
                    std::size_t to_read = std::min(arg_remains, std::size_t(readed_bytes));
                    argument_for_command.append(_read_buffer, to_read);

                    std::memmove(_read_buffer, _read_buffer + to_read, readed_bytes - to_read);
                    arg_remains -= to_read;
                    readed_bytes -= to_read;
                }

                // Thre is command & argument - RUN!
                if (command_to_execute && arg_remains == 0) {
                    _logger->debug("Start command execution");

                    _logger->debug("arguments {}", argument_for_command);

                    

                    std::string result;
                    command_to_execute->Execute(*pStorage, argument_for_command, result);
                    result += "\r\n";
                    _write_buffers.push_back(result);
                    if (_write_buffers.size() == 1) {
                        _event.events = EVENT_READ_WRITE;
                    }


                    // Prepare for the next command
                    command_to_execute.reset();
                    argument_for_command.resize(0);
                    parser.Reset();
                }
            } // while (readed_bytes)
        }

        if (readed_bytes == 0) {
            _logger->debug("Readed 0 bytes in DoRead");
        } else {
            throw std::runtime_error(std::string(strerror(errno)));
        }

    } catch (std::runtime_error &ex) {
        _logger->error("Failed to process connection on descriptor {}: {}", _socket, ex.what());
    }
}

// See Connection.h
void Connection::DoWrite() {
    _logger->info("DoWrite on descriptor {}\n", _socket);

    if (_write_buffers.empty()) { 
        _event.events = EVENT_READ;
        return;
    }

    const int max_size = 64;
    int written_bytes = 1;
    
    while ((written_bytes > 0) && (!_write_buffers.empty())){

        int cur_size = std::min(static_cast<int>(_write_buffers.size()), max_size);

        struct iovec iov[cur_size];
        {
            auto it = _write_buffers.begin();
            for (int i = 0; i < cur_size; ++i, ++it) {
                iov[i].iov_base = const_cast<char *>(it->c_str());
                iov[i].iov_len = it->size();
            }
        }
        iov[0].iov_base = static_cast<char *>(iov[0].iov_base) + _write_offset;
        iov[0].iov_len -= _write_offset;

        written_bytes = writev(_socket, iov, cur_size);

        auto it_delete_end = _write_buffers.begin();
        if (written_bytes > 0){
            _write_offset += written_bytes;
            int responses = 0;
            while ((responses < cur_size) && (_write_offset >= iov[responses].iov_len)) {
                _write_offset -= iov[responses].iov_len;
                it_delete_end++;
                responses++;
            }
        }
        _write_buffers.erase(_write_buffers.begin(), it_delete_end);
    }

    if (_write_buffers.empty()) {
        _event.events = EVENT_READ;
    }

}

} // namespace STnonblock
} // namespace Network
} // namespace Afina
