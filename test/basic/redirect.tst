/*
    redirect.tst - Redirection tests
 */

const HTTP = App.config.uris.http || "127.0.0.1:8080"
let http: Http = new Http

//  First just test a normal get
http.get(HTTP + "/dir/index.html")
assert(http.status == 200)

http.followRedirects = false
http.get(HTTP + "/dir")
assert(http.status == 302)

http.followRedirects = true
http.get(HTTP + "/dir")
assert(http.status == 200)

/*
http.followRedirects = true
http.get(HTTP + "/dir/")
assert(http.status == 200)
assert(http.response.contains("Hello /dir/index.html"))
*/
