## Creating Certificates

You can use the provided config file (`example/req.conf`) as a template for your certificate. After you have modified the conf file to your needs, run the following command.

```sh
openssl req -x509 -nodes -days 365 -newkey rsa:2048 -keyout server.key -out server.crt -config req.conf -extensions 'v3_req'
```

### Why do I need to use SAN certificates?

You need Subject Alt Names (SAN) to allow for different certificates for different servers. This could be useful if you wanted to expose multiple servers behind a WAN using proxies.

## Running the example server

> If you do not have cargo installed, you can install it at [https://www.rust-lang.org/](rust-lang.org).

Now you can run the example server with the following command.

```sh
cargo run example
```

## Installation

To install gemcaps, you can run:

```sh
cargo install --path .
```

This will install gemcaps into `$HOME/.cargo/bin/gemcaps`.

Now you can copy the example configuration to your real configuration folder (for this, I will assume it is in `/etc/gemcaps/`). You can now run your server with:

```sh
gemcaps /etc/gemcaps
```
