# GemCaps

![GemCaps Logo](gemcaps_logo.png)

> [Read me in gmi][gmi]

[gmi]: README.gmi

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

> Note: the process may be different on windows, but you would still need cmake to build gemcaps.

## Configuration

All configuration files are written in yaml. You can see an example of a
working configuration in the [example folder][examples]. The first
configuration file you will need to make is the main config file.

[examples]: example/

```
files/
 > test/
    > test.gmi
 > index.gmi
handlers/
 > files.yml
servers/
 > main.yml
conf.yml
cert.pem
key.pem
```

## Config

```yml
$schema: https://json-schema.org/draft/2020-12/schema
title: Gemcaps Config
description: configure general settings for gemcaps
type: object
properties:
  scriptRunners:
    description: A map of file extensions to program executables
    type: object
    additionalProperties:
      description: A list of arguments that can execute a file
      type: array
      items:
        type: string 
```

### conf.yml

```yml
scriptRunners:
  py: python3
  jar: ["java", "-jar"]
```

In the conf.yml file, it defines two file extensions that can be executed:

* py files can be executed by running `python3 <file>`
* jar files can be executed by running `java -jar <file>`


### Servers

Server configurations define individual servers that will be run in a single instance of gemcaps.

Below is a schema for server configs

#### Server Config Schema

```yml
$schema: https://json-schema.org/draft/2020-12/schema
title: Server Config
description: configure individual servers
type: object
properties:
  name:
    description: The name of the server. This is used by handlers to define which server they respond to
    type: string
  host:
    description: The hostname that the server will listen on. (only IPv4 is supported currently)
    default: 0.0.0.0
    type: string
  port:
    description: The port that the server will listen on.
    default: 1965
    type: number
  cert:
    description: The certificate file to use for this server.
    type: string
  key:
    description: The certificate key to use for this server.
    type: string
required:
- cert
- key
- name
```

#### main.yml

```yml
name: main
cert: ../cert.pem
key: ../key.pem
```

In this config, it defines a server by the name `main` that listens on `0.0.0.0:1965` using the certificates in the parent directory.

### Handlers

Handlers are a bit more complex because they allow for extensibility based on the type of handler being defined.

Below is the base schema for all handlers, then after that will be the schema for file handlers.

#### Base Handler Config Schema

```yml
$schema: https://json-schema.org/draft/2020-12/schema
title: Base handler config
description: base configuration for handlers
type: object
properties:
  server:
    description: The server to attach this handler to
    type: string
  handler:
    description: The handler that will be used for this configuration
    type: string
 required:
 - server
 - handler
```

#### File Handler Config Schema

```yml
$schema: https://json-schema.org/draft/2020-12/schema
title: File handler config
description: configuration for file handlers
type: object
properties:
  server:
    description: The server to attach this handler to
    type: string
  handler:
    description: The handler that will be used for this configuration
    type: string
    enum:
    - filehandler
  host:
    description: The hostname that the handler will accept i.e. 'localhost'
    type: string
  folder:
    description: The folder that contains the files to serve
    type: string
  base:
    description: The base path where the files start
    type: string
  readDirs:
    description: Whether to read the contents of directories if no index file is present
    type: boolean
    default: true
  rules:
    description: a list of regex patterns that a file would need to match in order for a client to view the file.
    type: array
    items:
      description: A regex string
      type: string
  cgiFiletypes:
    description: a list of file types that may be executed as a CGI script
    type: array
    items:
      description: A file extension
      type: string
  cgiLang:
    description: The language that cgi scripts should use
    default: en_US.UTF-8
    type: string
  cgiVars:
    description: The environmental variables to add to all CGI scripts
    type: object
    additionalProperties:
      type: string
required:
- server
- handler
- folder
```

#### files.yml

```yml
server: main
handler: filehandler
folder: ../files
cgiFiletypes:
- py
cgiVars:
  foobar: Cheese!
```

In files.yml, this defines a handler that will serve files from the files directory to any host.

It also configures .py files to be considered cgi files. It will attempt to execute any .py file as a cgi script.
