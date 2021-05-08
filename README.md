# GemCaps

![GemCaps Logo](gemcaps_logo.png)

GemCaps is a Gemini capsule server for the [gemini protocol][gemini]. It is
programmed in C++ using [libuv][libuv] and [wolfssl][wolfssl]. It is supported
on all linux/mac/windows (Any platform libuv supports). GemCaps runs
asynchronously which allows for more effecient use of the processor by having
fewer context switches and still allows for multiple simultaneous requests.

[gemini]: https://gemini.circumlunar.space
[libuv]: https://libuv.org/
[wolfssl]: https://www.wolfssl.com/

## Plans

My goal for GemCaps is to act as a highly effecient and configurable server.
It will support reverse proxies, CGI, and my own custom Server Gateway
Protocol. I want GemCaps to do the heavy lifting while being a gateway to other
servers. That way developers can spend less time working with SSL and serving
files, and more time on the complex parts of a dynamic server.

While I believe that CGI is great, it has its own downfalls. Most noteably, each
request spins up a new process. I plan to create a custom protocol that can link
a forward facing protocol (GemCaps) and a long lived process that can handle
incomming requests. I am calling this protocl GSGI (Gemini Server Gateway Interface)
It will be tailored specifically for gemini servers.

In this first version of GemCaps, it can only process file requests. I have
spent a lot of effort learning about how to write asynchronous programs with
libuv which has been very fullfilling. One of the most difficult parts of this
type of programming has proven to be dealing with memory leaks. While I have
stamped out most of them, I fear that there may be more that have gone under
the radar. Because of this, I hope to find and record as many issues as I can
find on the [issues][issues] page.

[issues]: https://github.com/ttocsneb/gemcaps/issues

## Installation

### Dependencies

I have tried my best to make GemCaps as painless to install as possible. All of
its dependencies have been included in the repository as submodules. So
collecting the dependencies should be as easy as:

```sh
git submodule init
git submodule update
```

### Building

GemCaps is built with cmake. The process of building is just as most other cmake projects:

```sh
mkdir build
cd build
cmake ..
make gemcaps
```

Your executable will be located at `bin/gemcaps`.

## Configuration

All configuration files are written in yaml. You can see an example of a
working configuration in the [example folder][examples]. The first
configuration file you will need to make is the main config file.

[examples]: example/

### Main Configuration

conf.yml

```yml
cert: path/to/cert.pem # required
key: path/to/key.pem # required
capsules: path/to/capsules/configuration/folder
listen: 0.0.0.0 # The default listening ip
port: 1965 # The default listening port
timeout: 10 # the time (seconds) before timing out a connection (a timeout of 0 means no timeout)
cacheSize: 50000 # the maximum size (kb) for the cache. (a size of 0 means no limit)
```

> Note: all relative paths in a configuration file are relative to the
> configuration file.
>
> In `/foo/bar/conf.yml`, the path `cheese` would point to `/foo/bar/cheese`

#### cert/key

This is a required field.

The path to the tls certificate and key file.

#### capsules

This is a required field.

The path to the capsule configurations.

#### listen

The host to listen on.

By default, this is `0.0.0.0`

#### port

The port to listen on.

By default, this is `1965`

#### timeout

The time (in seconds) before closing a connection due to innactivity

A value of `0`, means to not timeout any connection.

The default value is `5` seconds.

#### cacheSize

The maximum size (in kb) of the cache in memory.

The default value is `50000` kb (50 mb).

## Capsule Configurations

The capsule configuration files are a bit more involved than the main config
file. Each file can override some of the main settings to allow for listening
on multiple ports or hosts. When a different port or host is used, you can also
optionally specify custom certificates.

Each capsule can also have handlers. Each handler will process requests
differently. For now, only the `file` handler is available.

capsule.yml

```yml
host: '.*'
port: 1965
cert: path/to/cert.pem
key: path/to/key.pem
handlers:
- type: file
  path: '/'
  rules:
  - '.*\.gmi$' 
  root: '/foo/bar/www'
  allowedDirs:
  - '/foo/bar/www' 
  cacheTime: 1
  readDirs: true
```

#### host

This is a regex that the capsule will listen for. This should not be confused
with the host the server listens on. This is the host that the client requests.

#### port

You may listen on a different port than the default port. Please keep in mind
that if there are two capsules listening on the same port, they will be
listening on the same 'server'.

#### cert/key

You can also give a custom certificate file to use. Just remember that the
first capsule that is created with the given port will use their certificates.

If a capsule tries to load a certificate in a 'server' that is already created,
then a warning will be printed out to the console.

### Handlers

#### type

the type of handler to create. A list of valid types are as follows:

- file

#### path

The path to accept requests in.

#### rules

This can be used if you don't want to accept all paths inside the `path`
folder. It is a list of regexes where only one needs to match against a request
to be accepted.

### File Handler

- type: `file`

#### root

The root option is required and gives the location to read files from.

#### allowedDirs

The allowed directories on the filesystem that the server can read from. When
finding a file, `root` is added to the path in question then expands `../` and
symlinks. If the resulting path is not contained within any of the allowedDirs
options, then an error is sent to the client.

#### cacheTime

The time to cache a file in seconds

#### readDirs

If there is no `index\..*` file in a directory, then the contents of the 
directory will be sent to the client. To disable this, you can set `readDirs`
to false.

