/*
    alias.tst - Alias http tests
 */

const HTTP = App.config.uris.http || "127.0.0.1:8080"

let http: Http = new Http

/*
    aliasTest() mapps bad.html to atest.html
 */
http.get(HTTP + "/alias/bad.html")
assert(http.status == 200)
assert(http.response.contains("alias/atest.html"))
