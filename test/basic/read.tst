/*
    read.tst - Various Http read tests
 */

const HTTP = App.config.uris.http || "127.0.0.1:8080"
let http: Http = new Http

//  Test http.read() into a byte array
http.get(HTTP + "/big.asp")
buf = new ByteArray
count = 0
while (http.read(buf) > 0) {
    count += buf.length
}
if (count != 61491) {
    print("COUNT IS " + count + " code " + http.status)
}
assert(count == 61491)
http.close()

http.get(HTTP + "/lines.txt")
lines = http.readLines()
for (l in lines) {
    line = lines[l]
    assert(line.contains("LINE"))
    assert(line.contains((l+1).toString()))
}
assert(http.status == 200)
