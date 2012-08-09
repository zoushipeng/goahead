/*
    stream.tst - Http tests using streams
 */

const HTTP = App.config.uris.http || "127.0.0.1:8080"
let http: Http = new Http

http.get(HTTP + "/big.asp")
ts = new TextStream(http)
lines = ts.readLines()
assert(lines.length == 801)
assert(lines[0].contains("aaaaabbb") && lines[0].contains("0 "))
assert(lines[799].contains("aaaaabbb") && lines[799].contains("799"))

//TODO more 
