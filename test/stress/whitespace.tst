/*
    Test various whitespace
 */ 
const HTTP: Uri = App.config.uris.http || "127.0.0.1:8080"
const DELAY  = 500

let s
let count = 0
let response = new ByteArray

//  Leading white space
s = new Socket
s.connect(HTTP.address)
count += s.write(" GET /index.html HTTP/1.0\r\n\r\n")
assert(count > 0)
for (count = 0; (n = s.read(response, -1)) != null; count += n) { }
assert(response.toString().contains('200 OK'))
assert(response.toString().contains('Hello /index'))
s.close()

//  white space after method
s = new Socket
s.connect(HTTP.address)
count += s.write("GET         /index.html HTTP/1.0\r\n\r\n")
assert(count > 0)
for (count = 0; (n = s.read(response, -1)) != null; count += n) { }
assert(response.toString().contains('200 OK'))
assert(response.toString().contains('Hello /index'))
s.close()

//  white space after URI
s = new Socket
s.connect(HTTP.address)
count += s.write("GET /index.html      HTTP/1.0\r\n\r\n")
assert(count > 0)
for (count = 0; (n = s.read(response, -1)) != null; count += n) { }
assert(response.toString().contains('200 OK'))
assert(response.toString().contains('Hello /index'))
s.close()

