/*
    methods.tst - Test misc Http methods
 */

const HTTP = App.config.uris.http || "127.0.0.1:8080"
let http: Http = new Http

/*
//  Test methods are caseless
http.connect("GeT", HTTP + "/index.html")
assert(http.status == 200)

//  Put a file
data = Path("test.dat").readString()
http.put(HTTP + "/tmp/test.dat", data)
assert(http.status == 201 || http.status == 204)

//  Delete
http.connect("DELETE", HTTP + "/tmp/test.dat")
if (http.status != 204) {
    print("STATUS IS " + http.status)
}
assert(http.status == 204)

//  Options
http.connect("OPTIONS", HTTP + "/index.html")
assert(http.header("Allow") == "DELETE,GET,HEAD,OPTIONS,POST,PUT")

//  Trace - should be disabled
http.connect("TRACE", HTTP + "/index.html")
assert(http.status == 406)
*/

//  Post
http.post(HTTP + "/index.html", "Some data")
assert(http.status == 200)

http.get(HTTP + "/index.html")
assert(http.status == 200)

//  Head
http.connect("HEAD", HTTP + "/index.html")
assert(http.status == 200)
assert(http.header("Content-Length") > 0)
assert(http.response == "")

