/*
    form.tst - Basic session form tests
 */

const HTTP = App.config.uris.http || "127.0.0.1:8080"
let http: Http = new Http

//  GET /form/login
http.get(HTTP + "/form/login")
assert(http.status == 200)
assert(http.response.contains("Please Login"))
let cookie = http.header("Set-Cookie")
if (cookie) {
    cookie = cookie.match(/(-goahead-session-=.*);/)[1]
}
assert(cookie && cookie.contains("-goahead-session-="))
http.close()


//  POST /form/login
http.setCookie(cookie)
http.form(HTTP + "/form/login", { 
    username: "joshua", 
    password: "pass1", 
})
assert(http.status == 200)
assert(http.response.contains("Logged in"))
assert(!http.sessionCookie)
http.close()


//  GET /form/login
http.setCookie(cookie)
http.get(HTTP + "/form/login")
assert(http.status == 200)
assert(http.response.contains("Logged in"))
http.close()
