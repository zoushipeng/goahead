/*
    upload.tst - File upload tests
 */

require ejs.unix

const HTTP = App.config.uris.http || "127.0.0.1:8080"
let http: Http = new Http

if (App.config.bit_upload) {

    //  Upload a small file
    http.upload(HTTP + "/action/uploadTest", { myfile: "small.dat"} )
    assert(http.status == 200)
    assert(http.response.contains('CLIENT=small.dat'))
    assert(http.response.contains('SIZE=29'))
    assert(http.response.contains('FILENAME='))
    assert(http.response.contains('FILE_FILENAME_myfile='))
    assert(http.response.contains('FILE_CLIENT_FILENAME_myfile=small.dat'))
    assert(http.response.contains('FILE_SIZE_myfile=29'))
    http.wait()

    //  Test with form data
    http.upload(HTTP + "/action/uploadTest", { myfile: "small.dat"}, {name: "John Smith", address: "100 Mayfair"} )
    assert(http.status == 200)
    assert(http.response.contains('CLIENT=small.dat'))
    assert(http.response.contains('SIZE=29'))
    assert(http.response.contains('FILENAME='))
    assert(http.response.contains('FILE_FILENAME_myfile='))
    assert(http.response.contains('FILE_CLIENT_FILENAME_myfile=small.dat'))
    assert(http.response.contains('FILE_SIZE_myfile=29'))
    assert(http.response.contains('name=John Smith'))
    assert(http.response.contains('address=100 Mayfair'))

} else {
    test.skip("Upload support not enabled")
}
