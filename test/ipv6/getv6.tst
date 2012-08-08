/*
    getv6.tst - IPv6 GET tests
 */

const HTTP = App.config.ipv6 || "[::1]:4113"
let http: Http = new Http

//  Basic get. Validate response code and contents
http.get(HTTP + "/index.html")
assert(http.status == 200)
// assert(http.readString().contains("Hello"))

//  Validate get contents
http.get(HTTP + "/index.html")
assert(http.readString(12) == "<html><head>")
assert(http.readString(7) == "<title>")

//  Validate get contents
http.get(HTTP + "/index.html")
assert(http.response.endsWith("</html>\n"))
assert(http.response.endsWith("</html>\n"))

//  Test Get with a body. Yes this is valid Http, although unusual.
http.get(HTTP + "/index.html", {name: "John", address: "700 Park Ave"})
assert(http.status == 200)
