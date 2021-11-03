

# Making Certificates Using openssl

I'm not sure if having a self-signed root certificate will work with gemini, 
but it seems to be the only way to make rustls happy.

First generate a root certificate:

```
openssl genrsa -out ca.key 2048
openssl req -new -x509 -days 365 -key ca.key \
    -subj "/C=<Country>/ST=<State>/L=<Locality>/O=<Organization>/CN=<Org.> Root CA" -out ca.crt
```

Next create the server certificate:

```
openssl req -newkey rsa:2048 -nodes -keyout server.key \
    -subj "/C=<Country>/ST=<State>/L=<Locality>/O=<Organization>/CN=<domain>" -out server.csr
openssl x509 -req -extfile <(printf "subjectAltName=DNS:<domain1>,DNS:domain2") \
    -days 365 -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out server.crt
```
