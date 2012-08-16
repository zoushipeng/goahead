/*
    form.tst - Web form authentication http tests
 */

const HTTP = App.config.uris.http || "127.0.0.1:8080"

let http: Http = new Http

if (App.config.bit_auth) {
    http.get(HTTP + "/auth/form/form.html")
    assert(http.status == 302)
    let location = http.header('location')
    assert(Uri(location).path == '/form/login')

    http.form(location, { 
        username: "joshua", 
        password: "pass1", 
    })
    assert(http.status == 200)
    assert(http.response.contains("Logged in"))
    let cookie = http.header("Set-Cookie")
    assert(cookie && cookie.match(/(-goahead-session-=.*);/)[1])
    http.close()
}
