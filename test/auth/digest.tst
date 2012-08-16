/*
    digest.tst - Digest authentication http tests
 */

const HTTP = App.config.uris.http || "127.0.0.1:8080"

let http: Http = new Http

if (App.config.bit_auth) {
    //  Access to digest/digest.html accepts by any valid user
    http.get(HTTP + "/auth/digest/digest.html")
    assert(http.status == 401)

    http.setCredentials("joshua", "pass1")
    http.get(HTTP + "/auth/digest/digest.html")
    assert(http.status == 200)
}
