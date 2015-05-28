# draw-server
Collaborative drawing site powered by websockets

# About
This is a simple websocket server written in C that 
can handle multiple users drawing on the same canvas.

## Requirements
- an HTTP server
- libssl
- libcrypto
- libpthread

## Compiling
It's pretty simple for now...
```
$ make
```

## Usage
Place index.html at the root of your HTTP server, then run:

```
./draw-server 8808

```
The default port can be changed in index.html, and the default hostname
is localhost so that you can try it out before configuring your network.

