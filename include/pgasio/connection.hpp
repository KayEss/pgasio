/*
    Copyright 2017, Kirit Sælensminde. http://www.kirit.com/pgasio/
*/


#pragma once


#include <pgasio/network.hpp>

#include <boost/asio/local/stream_protocol.hpp>


namespace pgasio {


    template<typename S>
    class connection;


    /// Perform a handshake that assumes to authentication or connection
    /// options are needed.
    template<typename S> inline
    connection<S> handshake(
        S socket,
        const char *user, const char *database,
        boost::asio::yield_context &yield
    ) {
        command cmd{0}; // The initial connect doesn't use a packet type char
        cmd.int32(0x0003'0000);
        cmd.c_str("user");
        cmd.c_str(user);
        if ( database != nullptr && database[0] != 0 ) {
            cmd.c_str("database");
            cmd.c_str(database);
        }
        cmd.int8(0);
        cmd.send(socket, yield);
        std::unordered_map<std::string, std::string> settings;
        int32_t process_id{}, secret{};
        while ( socket.is_open() ) {
            auto header = packet_header(socket, yield);
            auto body = header.packet_body(socket, yield);
            decoder decode = byte_view(body);
            switch ( header.type ) {
            case 'K':
                process_id = decode.read_int32();
                secret = decode.read_int32();
                break;
            case 'R':
                break; // Do nothing, the authentication was succesful
            case 'S': {
                    auto name = decode.read_string();
                    auto value = decode.read_string();
                    settings.insert(std::make_pair(std::move(name), std::move(value)));
                }
                break;
            case 'Z':
                return connection<S>(std::move(socket), std::move(settings), process_id, secret);
            default:
                throw std::runtime_error(
                    std::string("Unknown connection packet type: ") + header.type);
            }
        }
        throw std::runtime_error("The Postgres connection closed during the initial handshake");
    }


    /// The connection to the database
    template<typename S>
    class connection {
        friend connection<S> handshake<>(S, const char *, const char *, boost::asio::yield_context &);

        connection(S s, std::unordered_map<std::string, std::string> set, int32_t pid, int32_t sec)
        : socket(std::move(s)), settings(std::move(set)), process_id(pid), secret(sec) {
        }

    public:
        S socket;
        const std::unordered_map<std::string, std::string> settings;
        const int32_t process_id;
        const int32_t secret;
    };


    /// Return a unix domain socket for the given location
    template<typename L> inline
    auto unix_domain_socket(
        boost::asio::io_service &ios, L loc, boost::asio::yield_context &yield
    ) {
        boost::asio::local::stream_protocol::socket socket{ios};
        boost::asio::local::stream_protocol::endpoint ep(loc);
        socket.async_connect(ep, yield);
        return socket;
    }


}

