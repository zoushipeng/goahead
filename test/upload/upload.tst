/*
    upload.tst - File upload tests
 */

require ejs.unix

const HTTP = App.config.uris.http || "127.0.0.1:8080"
let http: Http = new Http

if (App.config.bit_upload) {
    http.upload(HTTP + "/proc/upload", { myfile: "test.dat"} )
    assert(http.status == 200)
    assert(http.response.contains('"clientFilename": "test.dat"'))
    assert(http.response.contains('Uploaded'))
    http.wait()

    //  Test with form data
    http.upload(HTTP + "/proc/upload", { myfile: "test.dat"}, {name: "John Smith", address: "100 Mayfair"} )
    assert(http.status == 200)
    assert(http.response.contains('"clientFilename": "test.dat"'))
    assert(http.response.contains('Uploaded'))
    assert(http.response.contains('"address": "100 Mayfair"'))

} else {
    test.skip("Upload support not enabled")
}
