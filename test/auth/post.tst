/*
    post.tst - Posted form authentication tests
 */

const HTTP = App.config.uris.http || "127.0.0.1:8080"
// const HTTPS = App.config.uris.https || "127.0.0.1:9090"

let http: Http = new Http

if (App.config.bit_auth) {

    //  Will be denied
    http.get(HTTP + "/auth/post/index.html")
    assert(http.status == 302)
    let location = http.header('location')
    assert(Uri(location).path == '/auth/post/login.html')

    //  Will return login form
    http.get(location)
    assert(http.status == 200)
    assert(http.response.contains("<form"))
    assert(http.response.contains('action="/form/login"'))

    //  Login. Should succeed with the response being a redirect to /auth/post/index.html
    http.reset()
    http.form(HTTP + "/form/login", {username: "joshua", password: "pass1"})
    assert(http.status == 302)
    location = http.header('location')
    assert(Uri(location).path == '/auth/post/')
    let cookie = http.header("Set-Cookie")
    assert(cookie.match(/(-goahead-session-=.*);/)[1])

    //  Now logged in
    http.reset()
    http.setCookie(cookie)
    http.get(HTTP + "/auth/post/index.html")
    assert(http.status == 200)

    //  Now log out. Will be redirected to the login page.
    http.reset()
    http.setCookie(cookie)
    http.post(HTTP + "/form/logout")
    assert(http.status == 302)
    let location = http.header('location')
    assert(Uri(location).path == '/auth/post/login.html')

    //  Now should fail to access index.html and get the login page again.
    http.get(HTTP + "/auth/post/index.html")
    assert(http.status == 302)
    let location = http.header('location')
    assert(Uri(location).path == '/auth/post/login.html')

    http.close()
} else {
    test.skip("Auth not enabled")
}

