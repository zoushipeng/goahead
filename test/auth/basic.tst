/*
    basic.tst - Basic-style Authentication tests
 */

const HTTP = App.config.uris.http || "127.0.0.1:8080"

let http: Http = new Http

if (App.config.bit_auth) {
    //  Access to basic/basic.html only accepts an authenticated user. This should fail with 401.
    http.get(HTTP + "/auth/basic/basic.html")
    assert(http.status == 401)

    http.setCredentials("joshua", "pass1")
    http.get(HTTP + "/auth/basic/basic.html")
    assert(http.status == 200)
}
