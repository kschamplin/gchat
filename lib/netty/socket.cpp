

#include "netty.hpp"
#include <arpa/inet.h>
#include <assert.h>
#include <fcntl.h>
#include <iostream>
#include <system_error>
#include <unistd.h>
namespace Netty {

void Socket::open() {
    if (fd != -1) {
        return;
    }
    if (!info) {
        return; // addrinfo is missing.
    }
    fd = ::socket(info->ai_family, info->ai_socktype, info->ai_protocol);

    if (fd == -1) {
        throw std::system_error(errno, std::generic_category(),
                                "socket() failed");
    }
}

void Socket::setaddrinfo(addrinfo_p new_info) { info = move(new_info); }

void Socket::bind() {
    int result = ::bind(fd, info->ai_addr, info->ai_addrlen);

    if (result == -1) {
        throw std::system_error(errno, std::generic_category(),
                                "bind() failed");
    }
}

void Socket::listen() {
    int result = ::listen(fd, 20); // TODO: set a different backlog?

    if (result == -1) {
        throw std::system_error(errno, std::generic_category(),
                                "listen() failed");
    }
}

Socket Socket::accept() {
    addrinfo_p new_info = make_addrinfo(false);
    int result = ::accept(fd, new_info->ai_addr, &new_info->ai_addrlen);
    if (result == -1) {
        throw std::system_error(errno, std::generic_category(),
                                "accept() failed");
    }
    return Socket(result, move(new_info));
}

void Socket::connect() {
    // TODO: traverse the linked list for more results if connection fails.
    int result = ::connect(fd, info->ai_addr, info->ai_addrlen);
    if (result == -1) {
        throw std::system_error(errno, std::generic_category(),
                                "connect() failed");
    }
}

std::vector<std::uint8_t> Socket::recv(int size) {
    auto buf = std::vector<std::uint8_t>(size);
    int bytes = ::recv(fd, buf.data(), buf.size(), 0);
    if (bytes == -1) {
        throw std::system_error(errno, std::generic_category(),
                                "recv() failed");
    }
    buf.resize(size);
    return buf;
}

std::vector<std::uint8_t> Socket::recv_all() {
    assert((fcntl(fd, F_GETFL) & O_NONBLOCK) != 0);
    constexpr int block_size = 512;
    std::vector<std::uint8_t> result{};

    std::uint8_t data[block_size];
    while (1) {
        int bytes = ::recv(fd, data, block_size, 0);
        if (bytes == 0)
            break; // this means it's closed.
        if (bytes == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            throw std::system_error(errno, std::generic_category(),
                                    "recv() failed");
        }
        // bytes is how many bytes we recieved, add it to the result.
        result.insert(result.end(), data, &data[bytes]);
    }
    return result;
};

int Socket::send(std::vector<std::uint8_t> const &buf) {
    // this function is a bit harder, since we want to send all the data
    // sometimes the send() call only sends part of it.

    int len = buf.size();
    int sent = 0;
    while (len > 0) {
        int n = ::send(fd, buf.data() + sent, len, 0);
        if (n == -1) {
            throw std::system_error(errno, std::generic_category(),
                                    "send() failed:");
        }
        sent += n;
        len -= n;
    }

    return sent;
}

void Socket::setsockopt(int optname, int value) {
    int result = ::setsockopt(fd, SOL_SOCKET, optname, &value, sizeof(value));
    if (result == -1) {
        throw std::system_error(errno, std::generic_category(),
                                "setsockopt() failed:");
    }
}

} // namespace Netty
