# wsproxy

This program enables port forwarding on top of WebSocket similiar to ssh local port forwarding.

## Build

Make sure the `WebSocket++` and `boost` libraries are pre-installed. Or you can install with the following commands on Ubuntu.
```shell script
sudo apt install libwebsocketpp-dev libboost-all-dev
```

Then build with CMake.

```shell script
mkdir build && cd build
cmake ..
make all
```

## Server

Run `wspserver` on your server with listening port and forwarding port.

```shell script
./wspserver 1234 4321  # listen on 1234 and forward to 4321
```

## Client

Run `wsplisten` with server addr:port and the listening port.

```shell script
./wspclient 8.8.8.8:1234 1080  # listen on 1080 and the server is listening on 8.8.8.8:1234
```

Now any TCP data to localhost:1080 will be forwarded to 8.8.8.8:4321

## TODO

1. enable the client to assign the forwarding port.
2. enable encryption by using wss.
