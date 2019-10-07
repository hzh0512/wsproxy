#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <iostream>
#include <unordered_map>
#include "Connection.hpp"

typedef websocketpp::client<websocketpp::config::asio_client> client;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;
using websocketpp::connection_hdl;

// pull out the type of messages sent by our config
typedef websocketpp::config::asio_client::message_type::ptr message_ptr;

std::string server_uri;  // format: ws://localhost:9002
uint32_t server_port;
std::map<connection_hdl, Connection*, std::owner_less<connection_hdl>> ws_to_conn;
std::unordered_map<Connection*, connection_hdl> conn_to_ws;
client wsclient;

void on_message(client* c, connection_hdl hdl, message_ptr msg) {
    auto m = msg->get_payload();
    auto connection = ws_to_conn[hdl];
    if (msg->get_opcode() == websocketpp::frame::opcode::binary) {
        connection->send_raw(m.c_str(), m.size());
    } else {
        std::cout << "unknown opcode: " << msg->get_opcode() << std::endl;
    }
}

void on_fail(client* c, connection_hdl hdl) {
    client::connection_ptr con = c->get_con_from_hdl(hdl);
    std::cout << con->get_response_header("Server") << " failed because "
              << con->get_ec().message();
    if (ws_to_conn.find(hdl) == ws_to_conn.end()) {
        return;
    }
    auto conn = ws_to_conn[hdl];
    ws_to_conn.erase(hdl);
    conn_to_ws.erase(conn);
    conn->close();
}

void on_close(client* c, connection_hdl hdl) {
    client::connection_ptr con = c->get_con_from_hdl(hdl);
    std::cout << con->get_response_header("Server") << " closed because "
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
    if (evt == Connection::OnOpen) {
        client::connection_ptr con = wsclient.get_connection(server_uri, ec);
        if (ec) {
            std::cout << "Connect failed because: " << ec.message() << std::endl;
            connection->close();
            return;
        }
        std::cout << "Connected to " << server_uri << std::endl;
        wsclient.connect(con);
        auto handle = con->get_handle();
        ws_to_conn[handle] = connection;
        conn_to_ws[connection] = handle;
    } else if (evt == Connection::OnRecv) {
        std::vector<uint8_t> data = connection->recv_buffer;
        connection->recv_buffer.clear();
        wsclient.send(conn_to_ws[connection], data.data(), data.size(),
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
        wsclient.close(handle, websocketpp::close::status::normal, "bye", ec);
        if (ec) {
            std::cout << "Close failed because: " << ec.message() << std::endl;
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << argv[0] << " server_addr:server_port local_port" << std::endl;
        return -1;
    }

    server_uri = std::string("ws://") + argv[1];

    Server server(argv[2]);

    wsclient.set_access_channels(websocketpp::log::alevel::all);
    wsclient.clear_access_channels(websocketpp::log::alevel::frame_payload);
    wsclient.init_asio();
    wsclient.start_perpetual();
    wsclient.set_message_handler(bind(&on_message, &wsclient, ::_1, ::_2));
    wsclient.set_fail_handler(bind(&on_fail, &wsclient, ::_1));
    wsclient.set_close_handler(bind(&on_close, &wsclient, ::_1));

    try {
        while (true) {
            server.poll(on_local, 0);
            wsclient.poll();
            usleep(1000);
        }
    } catch (...) {
        wsclient.stop_perpetual();
        std::cout << "Bye!" << std::endl;
    }
}
