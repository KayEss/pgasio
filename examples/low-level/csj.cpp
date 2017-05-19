/*
    Copyright 2017, Kirit Sælensminde. http://www.kirit.com/pgasio/
*/


#include <cstdlib>
#include <iostream>
#include <string>

#include <f5/threading/channel.hpp>
#include <f5/threading/reactor.hpp>
#include <f5/threading/sync.hpp>

#include <pgasio/exec.hpp>


int main(int argc, char *argv[]) {
    /// The parameters we need to use. This is all legacy C stuff :(
    const char *user = std::getenv("LOGNAME");
    const char *database = nullptr;
    const char *path = "/var/run/postgresql/.s.PGSQL.5432";
    const char *sql = nullptr;

    /// Go through the command line and pull the details out
    for ( std::size_t a{1}; a < argc; ++a ) {
        using namespace std::string_literals;
        auto read_opt = [&](char opt) {
            if ( ++a >= argc ) throw std::runtime_error("Missing option after -"s + opt);
            return argv[a];
        };
        if ( argv[a] == "-d"s ) {
            database = read_opt('d');
        } else if ( argv[a] == "-h"s ) {
            path = read_opt('h');
        } else if ( argv[a] == "-U"s ) {
            user = read_opt('U');
        } else if ( argv[a][0] == '-' ) {
            std::cerr << "Unknown command line option: "s + argv[a][0] << std::endl;
            return 2;
        } else if ( sql ) {
            std::cerr << "Extra SQL command ignored\n" << argv[a] << std::endl;
        } else {
            sql = argv[a];
        }
    }
    if ( not sql ) {
        std::cerr << "MIssing SQL statement" << std::endl;
        return 1;
    }

    /// Set up the reactor thread pool and the channels we'll need
    f5::boost_asio::reactor_pool reactor{[]() {
        std::cerr << "An error occured\n";
        auto ep = std::current_exception();
        if ( ep ) {
            try {
                std::rethrow_exception(ep);
            } catch ( std::exception &e ) {
                std::cerr << e.what() << std::endl;
            }
        }
        std::exit(2);
        return false;
    }};
    f5::boost_asio::channel<pgasio::record_block> blocks{reactor.get_io_service(), reactor.size()};
    f5::boost_asio::channel<std::string> csj{reactor.get_io_service(), reactor.size()};

    /// Database conversation coroutine
    boost::asio::spawn(reactor.get_io_service(), [&](auto yield) {
        auto cnx = pgasio::handshake(
            pgasio::unix_domain_socket(reactor.get_io_service(), path, yield),
            user, database, yield);
        auto results = pgasio::exec(cnx, sql, yield);
        auto records = results.recordset(yield);
    });

    /// Workers for converting the raw data into CSJ
    for ( std::size_t t{}; t < reactor.size(); ++t ) {
        boost::asio::spawn(reactor.get_io_service(), [&](auto yield) {
        });
    }

    /// Write the CSJ blocks out to stdout in the right order
    f5::sync s;
    boost::asio::spawn(reactor.get_io_service(), s([&](auto yield) {
        while ( true ) {
            auto chunk = csj.consume(yield);
            if ( chunk.empty() ) return;
            std::cout << chunk;
        }
    }));
    s.wait();

    return 0;
}
