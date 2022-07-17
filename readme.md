# GemCaps

GemCaps will be a fully featured gemini webserver. You can configure multiple
hostnames with different endpoints. These endpoints can serve files, CGI
scripts, and proxies. This gives you enough power to build whatever you need
for your capsules.

## Configuration

Each capsule configuration file is tied to a single port. 
If you want multiple applications running from the same port, then you could
create a base application that redirects domains to the corresponding internal
port.

main.toml

```
listen = 1965
error_log = "logs/foobar.error.log"

[application]
domain_names = ["foobar.com"]
redirect = "gemini://localhost:1970"

[application]
domain_names = ["cheese.foobar.com"]
redirect = "gemini://localhost:1980"

```

foobar.toml

```
listen = 1970
certificate = "certs/foobar.crt"
certificate_key = "certs/foobar.key"
error_log = "logs/foobar.error.log"
access_log = "logs/foobar.access.log"
pid_file = "logs/foobar.pid"
worker_processes = 5
domain_names = ["foobar.com"]

[application]
rule = "^/app/"
proxy = "gemini://localhost:1971"
substitution = "s|^/app/||g" # sed inspired substitution

[application]
rule = "\.py$"
cgi_root = "/var/www/gemini"
substitution = "s|^|/foobar|g"

[application]
file_root = "/var/www/gemini"
substitution = "s|^|/foobar|g"

```

With a given capsule, you can have several types of configurations: redirect,
proxy, cgi, and file.

### Redirect

A redirect will redirect all communications to another gemini url without
setting up the tls connection. This is done by reading the requested domain
name in the tls header. This can be useful if you want to redirect all traffic
for one domain to another capsule.

### Proxy

A proxy is similar to redirect, however a tls connection is made with the
client and a new connection is made with the 3rd party. This can be useful if
you only want to redirect some traffic to a 3rd party based on the path.

### CGI

CGI is meant for use with dynamic capsules where code is executed on the server
and the result is sent back to the client.

### Files

You can send files from the filesystem back to the client.

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
