# Bare Asynchronous TCP Client

A simple native addon for the Bare runtime. It lets you connect to any IP address over a raw TCP socket, send a custom payload (like an HTTP request), and get the server's response back as a JavaScript Uint8Array.

## How Data Flows

```text
[ JavaScript Layer ]                 [ C Binding ]                         [ Network ]
           │                               │                                  │
      1.   │ ─── Calls tcpCat() ─────────> │                                  │
           │                               │ ── Allocates request context     │
           │                               │ ── Initializes TCP socket        │
           │                               │ ── Starts async connect ───────> │
      2.   │ <── Returns Pending Promise ─ │                                  │
           │                               │                                  │
           │                               │ <── TCP connection established ─ │
           │                               │ ── Sends payload data ─────────> │
           │                               │                                  │
           │                               │ ┌─── Incoming packet stream ───> │
           │                               │ │                                │
           │                               │ │  (Loop repeats per chunk)      │
           │                               │ │ <─ Response data chunk ─────── │
           │                               │ └──> Appends chunk to heap       │
           │                               │                                  │
           │                               │ <── Detects server hangup (EOF) ─│
           │                               │ ── Creates JS Uint8Array View    │
      3.   │ <── Promise Resolves ──────── │                                  │
           │     (Passes Uint8Array)       │ ── Cleans up socket & context    │
           ▼                               ▼                                  ▼
```

## Getting Started

Follow these steps to compile the native addon and run the suite:

```
$ npm install
$ bare-make generate
$ bare-make build
$ bare-make install
```
To test:

```
$ npx bare test.js
```


## Example Code (example.js)

To execute `example.js`:

```
$ npx bare example.js
```

Output:

```text
Connecting to TCP Socket -> 1.1.1.1:80...

Data successfully received:

HTTP/1.1 301 Moved Permanently
Date: Fri, 29 May 2026 16:37:44 GMT
Content-Type: text/html
Content-Length: 167
Connection: close
Cache-Control: max-age=3600
Expires: Fri, 29 May 2026 17:37:44 GMT
Location: https://www.cloudflare.com/
Set-Cookie: __cf_bm=ufkN9BawAyNHLxzVfw5dTuhdUbV1S6_pF0JO1ttO0pw-1780072664-1.0.1.1-n8AxOgodSW4L8ErkKxF0wV_wrbpSQNIEMKBXKscmm3AFMcJG2.HxkZXHKBPjTN9vgi018kRVJdpar5ozhEf6LisACuGfYWFqg6L5vnVePYc; path=/; expires=Fri, 29-May-26 17:07:44 GMT; domain=.cloudflare.com; HttpOnly
Report-To: {"endpoints":[{"url":"https:\/\/a.nel.cloudflare.com\/report\/v4?s=LxwWNAwUm5Vo2O3tcU8b5tfBW6TU36eJxLIB%2FmY0lGh%2F%2B14XCPpmuCqPoDWMbkOz4uyyCCIOgdEkJohgNERt8XRumMYFQYWwta8lF%2BbHz96dDZZTtmLVoVtoAx8Fyp0U"}],"group":"cf-nel","max_age":604800}
NEL: {"success_fraction":0,"report_to":"cf-nel","max_age":604800}
Server: cloudflare
CF-RAY: a036ecec08e523e2-DEL
alt-svc: h3=":443"; ma=86400

<html>
<head><title>301 Moved Permanently</title></head>
<body>
<center><h1>301 Moved Permanently</h1></center>
<hr><center>cloudflare</center>
</body>
</html>
```
