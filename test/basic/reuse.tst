/*
    reuse.tst - Test Http reuse for multiple requests
 */

const HTTP = App.config.uris.http || "127.0.0.1:8080"

let http: Http = new Http

http.get(HTTP + "/index.html")
assert(http.status == 200)

http.get(HTTP + "/index.html")
assert(http.status == 200)

http.get(HTTP + "/index.html")
assert(http.status == 200)

http.get(HTTP + "/index.html")
assert(http.status == 200)
