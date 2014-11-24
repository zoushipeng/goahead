/*
    Test directory traversal
 */

const HTTP: Uri = App.config.uris.http || "127.0.0.1:8080"

http = new Http
http.get(HTTP + "/../../appweb.conf")
assert(http.status == 400)
http.close()

http.get(HTTP + "/../../index.html")
assert(http.status == 400)
http.close()

/* Test windows '\' delimiter */
http.get(HTTP + "/..%5Cappweb.conf")
assert(http.status == 400)
http.close()

http.get(HTTP + "/../../../../../.x/.x/.x/.x/.x/.x/etc/passwd")
assert(http.status == 400)
http.close()

