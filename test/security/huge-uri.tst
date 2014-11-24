/*
    Very large URI test
 */ 
const HTTP: Uri = App.config.uris.http || "127.0.0.1:8080"

//  This writes a ~100K URI. LimitUri should be less than 100K for this unit test.

let data = "/"
for (i in 1000) {
    data += "012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678\n"
}

/*
    Test LimitUri
 */
let s = new Socket
s.connect(HTTP.address)
let count = 0
try {
    count += s.write("GET ")
    count += s.write(data)
    count += s.write(" HTTP/1.1\r\n\r\n")
} catch {
    // App.log.error("Write failed. Wrote  " + count + " of " + data.length + " bytes.")
    //  Check appweb.conf LimitRequestHeader. This must be sufficient to accept the write the header.
}

/* Server should just close the connection without a response */
response = new ByteArray
while ((n = s.read(response, -1)) != null) { }
if (response.length > 0) {
    /* May not get a response if the write above fails. Then we get a conn reset */
    assert(response.toString().contains('413 Request too large'))
}
s.close()

//  Check server still up
http = new Http
http.get(HTTP + "/index.html")
assert(http.status == 200)
http.close()
