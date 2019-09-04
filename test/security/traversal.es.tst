/*
    Test directory traversal
 */

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"

let status

http = new Http
http.get(HTTP + "/../auth.conf")
try {
    ttrue(!http.status)
} catch (err) {
    ttrue(err.toString().contains('Connection reset'))
}
http.close()

http.get(HTTP + "/../../index.html")
try {
    ttrue(!http.status)
} catch (err) {
    ttrue(err.toString().contains('Connection reset'))
}
http.close()

/* Test windows '\' delimiter */
http.get(HTTP + "/..%5Cauth.conf")
try {
    ttrue(!http.status)
} catch (err) {
    ttrue(err.toString().contains('Connection reset'))
}
http.close()

http.get(HTTP + "/../../../../../.x/.x/.x/.x/.x/.x/etc/passwd")
try {
    ttrue(!http.status)
} catch (err) {
    ttrue(err.toString().contains('Connection reset'))
}
http.close()

