// ===============================================================
// HTTP/HTTPS Client Library for Akar Script
//
// Written entirely in Akar, using only native primitives:
//   - net.connect(host, port)     → TcpSocket
//   - net.ssl_connect(host, port) → SslSocket
//   - string_find, string_sub, string_split, string_trim, etc.
//
// Auto-loaded by the VM — no `include` needed.
// ===============================================================

// -----------------------------------------------------------------
// Hex helpers (for chunked transfer encoding)
// -----------------------------------------------------------------

fn hex_val(c) {
    if (c == "0") { return 0 }
    if (c == "1") { return 1 }
    if (c == "2") { return 2 }
    if (c == "3") { return 3 }
    if (c == "4") { return 4 }
    if (c == "5") { return 5 }
    if (c == "6") { return 6 }
    if (c == "7") { return 7 }
    if (c == "8") { return 8 }
    if (c == "9") { return 9 }
    if (c == "a" or c == "A") { return 10 }
    if (c == "b" or c == "B") { return 11 }
    if (c == "c" or c == "C") { return 12 }
    if (c == "d" or c == "D") { return 13 }
    if (c == "e" or c == "E") { return 14 }
    if (c == "f" or c == "F") { return 15 }
    return -1
}

fn parse_hex(s) {
    let result = 0
    let i = 0
    s = string_trim(s)
    while (i < len(s)) {
        let d = hex_val(s[i])
        if (d < 0) { break }
        result = result * 16 + d
        i = i + 1
    }
    return result
}

// -----------------------------------------------------------------
// Chunked Transfer-Encoding decoder
// -----------------------------------------------------------------

fn decode_chunked(data) {
    let result = ""
    let pos = 0
    let dlen = len(data)

    while (pos < dlen) {
        let remaining = string_sub(data, pos, dlen - pos)
        let line_end = string_find(remaining, "\r\n")
        let nl_len = 2
        if (line_end < 0) {
            line_end = string_find(remaining, "\n")
            nl_len = 1
        }
        if (line_end < 0) { break }

        let size_str = string_sub(data, pos, line_end)
        let chunk_size = parse_hex(size_str)
        if (chunk_size <= 0) { break }

        pos = pos + line_end + nl_len

        if (pos + chunk_size <= dlen) {
            result = result + string_sub(data, pos, chunk_size)
        }
        pos = pos + chunk_size

        // skip trailing \r\n after chunk data
        if (pos + 2 <= dlen and string_sub(data, pos, 2) == "\r\n") {
            pos = pos + 2
        } else if (pos < dlen and string_sub(data, pos, 1) == "\n") {
            pos = pos + 1
        }
    }
    return result
}

// -----------------------------------------------------------------
// URL parser
//   returns { scheme, host, port, path }
// -----------------------------------------------------------------

fn parse_url(url) {
    let scheme = "http"
    let host = ""
    let port = 80
    let path = "/"

    if (string_starts_with(url, "https://")) {
        scheme = "https"
        port = 443
        url = string_sub(url, 8, len(url) - 8)
    } else if (string_starts_with(url, "http://")) {
        url = string_sub(url, 7, len(url) - 7)
    }

    let slash = string_find(url, "/")
    let host_port = ""
    if (slash >= 0) {
        host_port = string_sub(url, 0, slash)
        path = string_sub(url, slash, len(url) - slash)
    } else {
        host_port = url
    }

    let colon = string_find(host_port, ":")
    if (colon >= 0) {
        host = string_sub(host_port, 0, colon)
        let port_str = string_sub(host_port, colon + 1, len(host_port) - colon - 1)
        let p = to_number(port_str)
        if (p != nil and p > 0) { port = p }
    } else {
        host = host_port
    }

    return {scheme: scheme, host: host, port: port, path: path}
}

// -----------------------------------------------------------------
// HTTP response parser
//   Input : raw bytes (headers + body)
//   Output: { status, status_text, headers, body }
// -----------------------------------------------------------------

fn parse_response(raw) {
    let resp = {status: 0, status_text: "", headers: {}, body: ""}

    // split headers / body at blank line
    let sep = string_find(raw, "\r\n\r\n")
    let hdr_part = ""
    if (sep >= 0) {
        hdr_part = string_sub(raw, 0, sep)
        resp.body   = string_sub(raw, sep + 4, len(raw) - sep - 4)
    } else {
        sep = string_find(raw, "\n\n")
        if (sep >= 0) {
            hdr_part = string_sub(raw, 0, sep)
            resp.body = string_sub(raw, sep + 2, len(raw) - sep - 2)
        } else {
            resp.body = raw
            return resp
        }
    }

    let lines = string_split(hdr_part, "\r\n")
    if (len(lines) == 0) {
        lines = string_split(hdr_part, "\n")
    }

    // status line
    if (len(lines) > 0) {
        let parts = string_split(lines[0], " ")
        if (len(parts) >= 2) {
            let s = to_number(parts[1])
            if (s != nil) { resp.status = s }
        }
        if (len(parts) >= 3) {
            resp.status_text = parts[2]
            let i = 3
            while (i < len(parts)) {
                resp.status_text = resp.status_text + " " + parts[i]
                i = i + 1
            }
        }
    }

    // header lines
    let i = 1
    while (i < len(lines)) {
        let line = lines[i]
        let cp = string_find(line, ":")
        if (cp > 0) {
            let key = string_trim(string_sub(line, 0, cp))
            let val = string_trim(string_sub(line, cp + 1, len(line) - cp - 1))
            resp.headers[key] = val
        }
        i = i + 1
    }

    // chunked transfer encoding?
    let te = ""
    if (resp.headers["Transfer-Encoding"] != nil) {
        te = resp.headers["Transfer-Encoding"]
    }
    if (string_contains(te, "chunked")) {
        resp.body = decode_chunked(resp.body)
    }

    return resp
}

// -----------------------------------------------------------------
// Core request function
//
//   http_request(method, url, body_or_nil, headers_or_nil)
//   Returns: { status, status_text, headers, body }  or  nil
// -----------------------------------------------------------------

fn http_request(method, url, body, headers) {
    let u = parse_url(url)
    let use_ssl = (u.scheme == "https")

    // connect
    let sock = nil
    if (use_ssl) {
        sock = net.ssl_connect(u.host, u.port)
    } else {
        sock = net.connect(u.host, u.port)
    }
    if (sock == nil) { return nil }

    // build request
    let req = method + " " + u.path + " HTTP/1.1\r\n"
    req = req + "Host: " + u.host + "\r\n"
    req = req + "Connection: close\r\n"
    req = req + "User-Agent: AkarScript/1.0\r\n"

    let body_str = ""
    if (body != nil) { body_str = to_string(body) }

    if (len(body_str) > 0) {
        req = req + "Content-Length: " + to_string(len(body_str)) + "\r\n"
    }

    if (headers != nil) {
        let hdrs = keys(headers)
        let hi = 0
        while (hi < len(hdrs)) {
            let key = hdrs[hi]
            req = req + key + ": " + to_string(headers[key]) + "\r\n"
            hi = hi + 1
        }
    }

    req = req + "\r\n"
    if (len(body_str) > 0) { req = req + body_str }

    // send
    sock.send(req)

    // read until connection closes
    let raw = ""
    let empties = 0
    while (empties < 3) {
        let chunk = sock.recv(4096, 3000)
        if (chunk == nil or len(chunk) == 0) {
            empties = empties + 1
        } else {
            raw = raw + chunk
            empties = 0
        }
    }
    sock.close()

    if (len(raw) == 0) { return nil }
    return parse_response(raw)
}

// -----------------------------------------------------------------
// Convenience wrappers
//   (Akar enforces exact arity, so we can't make `headers` optional
//    in a single fn — use these short wrappers instead.)
// -----------------------------------------------------------------

fn http_get(url) {
    return http_request("GET", url, nil, nil)
}

fn http_post(url, body) {
    return http_request("POST", url, body, nil)
}

fn http_put(url, body) {
    return http_request("PUT", url, body, nil)
}

fn http_patch(url, body) {
    return http_request("PATCH", url, body, nil)
}

fn http_delete(url) {
    return http_request("DELETE", url, nil, nil)
}

fn http_head(url) {
    return http_request("HEAD", url, nil, nil)
}
