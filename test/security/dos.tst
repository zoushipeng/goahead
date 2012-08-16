/*
    Denial of service testing
 */

const HTTP: Uri = App.config.uris.http || "127.0.0.1:8080"

//  Check server available
http = new Http
http.get(HTTP + "/index.html")
assert(http.status == 200)
http.close()

//  MOB - TEMP
if (Config.OS != 'windows') {
    //  Try to crash with DOS attack
    for (i in 2000) {
        let s = new Socket
        try {
            s.connect(HTTP.address)
        } catch (e) {
            print("ERROR", i)
            print(e)
            throw e
        }
        let written = s.write("Any Text")
        assert(written == 8)
        s.close()
    }
}

//  Check server still there
http = new Http
http.get(HTTP + "/index.html")
assert(http.status == 200)
http.close()
