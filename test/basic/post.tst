/*
    post.tst - Post method tests
 */

const HTTP = App.config.uris.http || "127.0.0.1:8080"

let http: Http = new Http

http.post(HTTP + "/action/test", "Some data")
assert(http.status == 200)
http.close()

http.form(HTTP + "/action/test", {name: "John", address: "700 Park Ave"})
assert(http.response.contains('name: John'))
assert(http.response.contains('address: 700 Park Ave'))
http.close()
