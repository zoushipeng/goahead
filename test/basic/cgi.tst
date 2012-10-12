/*
    cgi.tst - CGI tests
 */

const HTTP = App.config.uris.http || "127.0.0.1:8080"
let http: Http = new Http

let vxworks = (global.test && test.hostOs == "VXWORKS")

if (App.config.bit_cgi && Path(test.top).join("test/cgi-bin/cgitest").exists) {
    /* Suport routines */

    function contains(pat): Void {
        assert(http.response.contains(pat))
    }

    function keyword(pat: String): String {
        pat.replace(/\//, "\\/").replace(/\[/, "\\[")
        let reg = RegExp(".*" + pat + "=([^<]*).*", "s")
        return http.response.replace(reg, "$1")
    }

    function match(key: String, value: String): Void {
        if (keyword(key) != value) {
            print(http.response)
            print("\nKey \"" + key + "\"")
            print("Expected: " + value)
            // print("Actual  : " + keyword(value))
        }
        assert(keyword(key) == value)
    }


    /* Tests */

    function forms() {
        //  Test various forms to invoke cgi programs
        http.get(HTTP + "/cgi-bin/cgitest")
        assert(http.status == 200)
        contains("cgitest: Output")

        if (App.test.os == "WIN") {
            http.get(HTTP + "/cgi-bin/cgitest.exe")
            assert(http.status == 200)
            contains("cgitest: Output")
        }
    }

    function extraPath() {
        http.get(HTTP + "/cgi-bin/cgitest")
        assert(http.status == 200)
        match("PATH_INFO", "")
        match("PATH_TRANSLATED", "")

        http.get(HTTP + "/cgi-bin/cgitest/extra/path")
        match("PATH_INFO", "/extra/path")
        let scriptFilename = keyword("SCRIPT_FILENAME")
        let path = Path(scriptFilename).dirname.join("extra/path")
        let translated = Path(keyword("PATH_TRANSLATED"))
        assert(path == translated)
    }

    function query() {
        http.get(HTTP + "/cgi-bin/cgitest/extra/path?a=b&c=d&e=f")
        match("SCRIPT_NAME", "/cgi-bin/cgitest")
        match("PATH_INFO", "/extra/path")
        contains("QVAR a=b")
        contains("QVAR c=d")

        http.get(HTTP + "/cgi-bin/cgitest?a+b+c")
        match("QUERY_STRING", "a+b+c")
        contains("QVAR a b c")

        //
        //  Query string vars should not be turned into variables for GETs
        //  Extra path only supported for cgi programs with extensions.
        //
        http.get(HTTP + "/cgi-bin/cgitest/extra/path?var1=a+a&var2=b%20b&var3=c")
        match("SCRIPT_NAME", "/cgi-bin/cgitest")
        match("QUERY_STRING", "var1=a+a&var2=b%20b&var3=c")
        match("QVAR var1", "a a")
        match("QVAR var2", "b b")
        match("QVAR var3", "c")

        //
        //  Post data should be turned into variables
        //
        http.form(HTTP + "/cgi-bin/cgitest/extra/path?var1=a+a&var2=b%20b&var3=c", 
            { name: "Peter", address: "777 Mulberry Lane" })
        match("QUERY_STRING", "var1=a+a&var2=b%20b&var3=c")
        match("QVAR var1", "a a")
        match("QVAR var2", "b b")
        match("QVAR var3", "c")
        match("PVAR name", "Peter")
        match("PVAR address", "777 Mulberry Lane")
    }

    function encoding() {
        http.get(HTTP + "/cgi-bin/cgitest/extra%20long/a/../path|/a/..?var%201=value%201")
        match("QUERY_STRING", "var%201=value%201")
        match("SCRIPT_NAME", "/cgi-bin/cgitest")
        match("QVAR var 1", "value 1")
        match("PATH_INFO", "/extra long/path|/")

        let scriptFilename = keyword("SCRIPT_FILENAME")
        let path = Path(scriptFilename).dirname.join("extra long/path|")
        let translated = Path(keyword("PATH_TRANSLATED"))
        assert(path == translated)
    }

    function status() {
        let http = new Http
        http.setHeader("SWITCHES", "-s%20711")
        http.get(HTTP + "/cgi-bin/cgitest?mob=111")
        assert(http.status == 711)
        http.close()
    }

    function location() {
        let http = new Http
        http.setHeader("SWITCHES", "-l%20/index.html")
        http.followRedirects = false
        http.get(HTTP + "/cgi-bin/cgitest")
        assert(http.status == 302)
        http.close()
    }

    function quoting() {
        http.get(HTTP + "/cgi-bin/cgitest?a+b+c")
        match("QUERY_STRING", "a+b+c")
        match("QVAR a b c", "")

        http.get(HTTP + "/cgi-bin/cgitest?a=1&b=2&c=3")
        match("QUERY_STRING", "a=1&b=2&c=3")
        match("QVAR a", "1")
        match("QVAR b", "2")
        match("QVAR c", "3")

        http.get(HTTP + "/cgi-bin/cgitest?a%20a=1%201+b%20b=2%202")
        match("QUERY_STRING", "a%20a=1%201+b%20b=2%202")
        match("QVAR a a", "1 1 b b=2 2")

        http.get(HTTP + "/cgi-bin/cgitest?a%20a=1%201&b%20b=2%202")
        match("QUERY_STRING", "a%20a=1%201&b%20b=2%202")
        match("QVAR a a", "1 1")
        match("QVAR b b", "2 2")
    }
    forms()
    extraPath()
    query()
    encoding()
    status()
    location()
    quoting()

} else {
    test.skip("CGI not enabled")
}
