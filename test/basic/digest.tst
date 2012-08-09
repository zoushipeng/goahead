/*
    digest.tst - Digest authentication tests
 */

const HTTP = App.config.uris.http || "127.0.0.1:8080"
let http: Http = new Http

if (App.config.bit_user_management) {
    http.setCredentials("anybody", "PASSWORD WONT MATTER")
    http.get(HTTP + "/index.html")
    assert(http.status == 200)

    // Any valid user 
    http.setCredentials("joshua", "pass1")
    http.get(HTTP + "/digest/digest.html")
    assert(http.status == 200)

    // Must be rejected 
    http.setCredentials("joshua", "WRONG PASSWORD")
    http.get(HTTP + "/digest/digest.html")
    assert(http.status == 401)

    // Group access 
    http.setCredentials("mary", "pass2")
    http.get(HTTP + "/digest/group/group.html")
    assert(http.status == 200)

    // Must be rejected - Joshua is not in group 
    http.setCredentials("joshua", "pass1")
    http.get(HTTP + "/digest/group/group.html")
    assert(http.status == 401)

    // User access - Joshua is the required member 
    http.setCredentials("joshua", "pass1")
    http.get(HTTP + "/digest/user/user.html")
    assert(http.status == 200)

    // Must be rejected - Mary is not in group 
    http.setCredentials("mary", "pass1")
    http.get(HTTP + "/digest/user/user.html")
    assert(http.status == 401)

    if (test.os == "WIN") {
        // Case won't matter 
        http.setCredentials("joshua", "pass1")
        http.get(HTTP + "/diGEST/diGEST.hTMl")
        assert(http.status == 200)
    }
}
