/*
    post.tst - Stress test post data
 */

const HTTP = App.config.uris.http || "127.0.0.1:4100"

let http: Http = new Http

/* Depths:    0  1  2  3   4   5   6    7    8    9    */
var sizes = [ 1, 2, 4, 8, 16, 32, 64, 128, 256, 512 ]

//  Create test buffer 
buf = new ByteArray
for (i in 64) {
    for (j in 15) {
        buf.writeByte("A".charCodeAt(0) + (j % 26))
    }
    buf.writeByte("\n".charCodeAt(0))
}

//  Scale the count by the test depth
count = sizes[test.depth] * 1024

function postTest(url: String) {
    // print("@@@@ Writing " + count * buf.length + " to " + url)
    http.post(HTTP + url)
    // print("Count " + count + " buf " + buf.length + " total " + count * buf.length)
    for (i in count) {
        let n = http.write(buf)
        // print('WROTE', n)
    }
    http.wait(120 * 1000)
    if (http.status != 200) {
        print("STATUS " + http.status)
        print(http.response)
    }
    assert(http.status == 200)
    assert(http.response)
    http.close()
}

postTest("/index.html")

if (App.config.bit_ejscript) {
    postTest("/form.ejs")
}

if (App.config.bit_php) {
    postTest("/form.php")
}

if (App.config.bit_cgi) {
    postTest("/cgi-bin/cgiProgram")
}

if (App.config.bit_esp) {
    postTest("/test.esp")
}
