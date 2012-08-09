/*
    bigUrl.tst - Stress test very long URLs 
 */

const HTTP = App.config.uris.http || "127.0.0.1:4100"
// const HTTP = App.config.uris.http || "vx:4100"
let depth = (global.test && test.depth) || 4

let http: Http = new Http

// Create a very long query
let queryPart = ""
for (i in 100) {
    queryPart += + "key" + i + "=" + 1234567890 + "&"
}

//  Vary up the query length based on the depth


for (iter in depth) {
    let query = ""
    for (i in 5 * (iter + 3)) {
        query += queryPart + "&"
    }
    query = query.trim("&")

    // Test /index.html
    http.get(HTTP + "/index.html?" + query)
    assert(http.status == 200)
    assert(http.response.contains("Hello /index.html"))

    if (App.config.bit_ejscript) {
        //  Test /index.ejs
        http.get(HTTP + "/index.ejs?" + query)
        assert(http.status == 200)
        assert(http.response.contains("Hello /index.ejs"))
    }
    //  TODO - esp, cgi, php
}
http.close()
