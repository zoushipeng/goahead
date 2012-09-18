/*
    secure.tst - Test SECURE ability
 */

const HTTP = App.config.uris.http || "127.0.0.1:8080"
const HTTPS = App.config.uris.https || "https://127.0.0.1:4443"

let http: Http = new Http

if (App.config.bit_route) {

    //  Will be denied
    http.get(HTTP + "/secure/index.html")
    assert(http.status == 405)

    //  Will be admitted
    http.get(HTTPS + "/secure/index.html")
    assert(http.status == 200)
    assert(http.response.contains("Hello secure world"))
    http.close()
} else {
    test.skip("Routing not enabled")
}

