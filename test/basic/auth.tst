/*
    auth.tst - Authentication http tests
 */

const HTTP = App.config.uris.http || "127.0.0.1:8080"

let http: Http = new Http

if (App.config.bit_user_management) {
    //  Access to basic/basic.html accepts by any valid user
    http.get(HTTP + "/basic/basic.html")
    assert(http.status == 401)

    http.setCredentials("joshua", "pass1")
    http.get(HTTP + "/basic/basic.html")
    assert(http.status == 200)

    http.setCredentials("mary", "pass2")
    http.get(HTTP + "/basic/basic.html")
    assert(http.status == 200)


    //  Access to basic/user/user.html accepts joshua only
    http.setCredentials(null, null)
    http.get(HTTP + "/basic/user/user.html")
    assert(http.status == 401)

    http.setCredentials("joshua", "pass1")
    http.get(HTTP + "/basic/user/user.html")
    assert(http.status == 200)

    http.setCredentials("mary", "pass2")
    http.get(HTTP + "/basic/user/user.html")
    assert(http.status == 401)

    if (App.config.bit_cgi && global.test && test.hostOs != "VXWORKS") {
        /* Requires /bin/sh */
        http.setCredentials(null, null)
        http.get(HTTP + "/basic/basic.cgi")
        assert(http.status == 401)
        http.setCredentials("joshua", "pass1")
        http.get(HTTP + "/basic/basic.cgi")
        assert(http.status == 200)
        assert(http.response.contains("SCRIPT_NAME=/basic/basic.cgi"))
        assert(!http.response.contains("PATH_INFO"))
        assert(!http.response.contains("PATH_TRANSLATED"))
        assert(http.response.contains("REMOTE_USER=joshua"))
    }

    //  Digest tests
    //  Access to digest/digest.html accepts by any valid user
    http.get(HTTP + "/digest/digest.html")
    assert(http.status == 401)

    http.setCredentials("joshua", "pass1")
    http.get(HTTP + "/digest/digest.html")
    assert(http.status == 200)

    http.setCredentials("mary", "pass2")
    http.get(HTTP + "/digest/digest.html")
    assert(http.status == 200)

    //  Access to digest/user/user.html accepts joshua only
    http.setCredentials(null, null)
    http.get(HTTP + "/digest/user/user.html")
    assert(http.status == 401)

    http.setCredentials("joshua", "pass1")
    http.get(HTTP + "/digest/user/user.html")
    assert(http.status == 200)

    http.setCredentials("mary", "pass2")
    http.get(HTTP + "/digest/user/user.html")
    assert(http.status == 401)
}
