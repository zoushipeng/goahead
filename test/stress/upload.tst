/*
    upload.tst - Stress test uploads
 */

const HTTP = App.config.uris.http || "127.0.0.1:4100"
const TESTFILE = "upload-" + hashcode(self) + ".tdat"

if (App.config.bld_ejscript) {

    let http: Http = new Http

    /* Depths:    0  1  2  3   4   5   6    7    8    9    */
    var sizes = [ 1, 2, 4, 8, 16, 32, 64, 128, 256, 512 ]

    //  Create test data 
    buf = new ByteArray
    for (i in 64) {
        for (j in 15) {
            buf.writeByte("A".charCodeAt(0) + (j % 26))
        }
        buf.writeByte("\n".charCodeAt(0))
    }

    //  Create test data file
    f = File(TESTFILE).open({mode: "w"})
    for (i in (sizes[test.depth] * 1024)) {
        f.write(buf)
    }
    f.close()

    if (test.threads == 1) {
        size = Path(TESTFILE).size

        http.upload(HTTP + "/upload.ejs", { file: TESTFILE })
        assert(http.status == 200)
        http.close()

        let uploaded = Path("../web/tmp").join(Path(TESTFILE).basename)
        assert(uploaded.size == size)
        Cmd.sh("diff " + uploaded + " " + TESTFILE)
    }
    Path(TESTFILE).remove()

} else {
    test.skip("Ejscript not enabled")
}
