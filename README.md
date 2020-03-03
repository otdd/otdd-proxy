# Otdd Istio Proxy

The Otdd Istio Proxy extends the istio proxy( envoy ) with two filters: Otdd Redirector and Otdd Recorder

Otdd Redirector:

- *Request Redirecting*. The otdd redirector takes effect on the redirector pod and redirect the requests to the recorder pod at a specified interval( defautls to 1s).

- *Protocol Support*. Currently supports http protocol.

Otdd Recorder:

- *Test Case Recording*. The otdd recorder records and groups the inbound request/response and its cooresponding outbound requests/responses and send them to otdd server through mixer.

Please see [otdd.io](https://otdd.io)
to learn about the overall Otdd project and how to get in touch with us.
