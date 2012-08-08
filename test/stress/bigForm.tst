/*
    bigForm.tst - Stress test very large form data (triggers chunking)
 */

const HTTP = App.config.uris.http || "127.0.0.1:4100"

let http: Http = new Http

let form = {}
for (i in 10000) {
    form["field_" + i] = Date.ticks
}

http.form(HTTP + "/test.esp", form)
assert(http.status == 200)
assert(http.response.contains("ESP Test Program"))
http.close()
