/*
    bigUrl.tst - Stress test very long URLs 
 */

const HTTP = App.config.uris.http || "127.0.0.1:8080"
// const HTTP = App.config.uris.http || "vx:8080"
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
    if (query.length < 2000) {
        assert(http.status == 200)
        assert(http.response.contains("Hello /index.html"))
    } else {
        if (http.status != 413) {
            print('STATUS', http.status)
            dump('HEADERS', http.headers)
            print('response', http.response)
        }
        assert(http.status == 413)
    }
}
http.close()
