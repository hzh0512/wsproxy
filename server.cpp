#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <iostream>
#include <unordered_map>
#include "Connection.hpp"

typedef websocketpp::server<websocketpp::config::asio> server;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;
using websocketpp::connection_hdl;

// pull out the type of messages sent by our config
typedef server::message_ptr message_ptr;

std::map<connection_hdl, Connection*, std::owner_less<connection_hdl>> ws_to_conn;
std::unordered_map<Connection*, connection_hdl> conn_to_ws;
Client client;
server wsserver;
char* forward_port;


void on_open(server* s, connection_hdl hdl) {
    websocketpp::lib::error_code ec;
    auto conn = client.connect("127.0.0.1", forward_port);
    if (!conn) {
        s->close(hdl, websocketpp::close::status::normal, "server port not open", ec);
        if (ec) {
            std::cout << "Close failed because: " << ec.message() << std::endl;
        }
    } else {
        std::cout << "Connected to 127.0.0.1:" << forward_port << std::endl;
        ws_to_conn[hdl] = conn;
        conn_to_ws[conn] = hdl;
    }
}

void on_message(server* s, connection_hdl hdl, message_ptr msg) {
    auto m = msg->get_payload();
    auto connection = ws_to_conn[hdl];
    connection->send_raw(m.c_str(), m.size());
}

void on_fail(server* s, connection_hdl hdl) {
    server::connection_ptr con = s->get_con_from_hdl(hdl);
    std::cout << con->get_response_header("Client") << " failed because "
              << con->get_ec().message();
    if (ws_to_conn.find(hdl) == ws_to_conn.end()) {
        return;
    }
    auto conn = ws_to_conn[hdl];
    ws_to_conn.erase(hdl);
    conn_to_ws.erase(conn);
    conn->close();
}

void on_close(server* s, connection_hdl hdl) {
    server::connection_ptr con = s->get_con_from_hdl(hdl);
    std::cout << con->get_response_header("Client") << " closed because "
              << con->get_remote_close_reason() << ", close code: "
              << con->get_remote_close_code() << " ("
              << websocketpp::close::status::get_string(con->get_remote_close_code());
    if (ws_to_conn.find(hdl) == ws_to_conn.end()) {
        return;
    }
    auto conn = ws_to_conn[hdl];
    ws_to_conn.erase(hdl);
    conn_to_ws.erase(conn);
    conn->close();
}

void on_local(Connection *connection, Connection::Event evt) {
    websocketpp::lib::error_code ec;
    if (evt == Connection::OnRecv) {
        std::vector<uint8_t> data = connection->recv_buffer;
        connection->recv_buffer.clear();
        wsserver.send(conn_to_ws[connection], data.data(), data.size(),
                      websocketpp::frame::opcode::binary, ec);
        if (ec) {
            std::cout << "Send failed because: " << ec.message() << std::endl;
        }
    } else if (evt == Connection::OnClose) {
        if (conn_to_ws.find(connection) == conn_to_ws.end()) {
            return;
        }
        auto handle = conn_to_ws[connection];
        ws_to_conn.erase(handle);
        conn_to_ws.erase(connection);
        wsserver.close(handle, websocketpp::close::status::normal, "bye", ec);
        if (ec) {
            std::cout << "Close failed because: " << ec.message() << std::endl;
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << argv[0] << " listen_port forward_port" << std::endl;
        return -1;
    }

    uint32_t listen_port = strtoul(argv[1], nullptr, 0);
    forward_port = argv[2];

    wsserver.set_access_channels(websocketpp::log::alevel::all);
    wsserver.clear_access_channels(websocketpp::log::alevel::frame_payload);
    wsserver.init_asio();
    wsserver.start_perpetual();
    wsserver.set_message_handler(bind(&on_message, &wsserver, ::_1, ::_2));
    wsserver.set_fail_handler(bind(&on_fail, &wsserver, ::_1));
    wsserver.set_close_handler(bind(&on_close, &wsserver, ::_1));
    wsserver.set_open_handler(bind(&on_open, &wsserver, ::_1));

    wsserver.listen(listen_port);
    wsserver.start_accept();

    std::cout << "listening on port " << listen_port << std::endl;

    try {
        while (true) {
            client.poll(on_local, 0);
            wsserver.poll();
            usleep(1000);
        }
    } catch (...) {
        wsserver.stop_perpetual();
        std::cout << "Bye!" << std::endl;
    }
}
