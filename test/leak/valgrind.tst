/*
    valgrind.tst - Valgrind tests on Unix-like systems
 */
let PORT = 4150
let valgrind = Cmd.locate("valgrind")

if (test.os == "LINUX" && test.depth >= 4 && valgrind) {
    let host = "127.0.0.1:" + PORT

    let httpCmd = test.bin.join("http").portable + " -q --zero "
    let appweb = test.bin.join("appweb").portable + " --config appweb.conf --name api.valgrind"
    valgrind += " -q --tool=memcheck --leak-check=yes --suppressions=../../../build/bin/mpr.supp " + appweb + test.mapVerbosity(-2)
    // valgrind = appweb

    //  Run http
    function run(args): String {
        try {
            // print(httpCmd, args)
            let cmd = Cmd(httpCmd + args)
            if (cmd.status != 0) {
                print("STATUS " + cmd.status)
                print(cmd.error)
                print(cmd.response)
            }
            assert(cmd.status == 0)
            return cmd.response
        } catch (e) {
            assert(false, e)
        }
        return null
    }
    /*
        Start valgrind and wait till ready
     */
    // print("VALGRIND CMD: " + valgrind)
    let cmd = Cmd(valgrind, {detach: true})
    let http
    for (i in 10) {
        http = new Http
        try { 
            http.get(host + "/index.html")
            if (http.status == 200) break
        } catch (e) {}
        App.sleep(1000)
        http.close()
    }
    if (http.status != 200) {
        throw "Can't start appweb for valgrind"
    }
    run("-i 100 " + PORT + "/index.html")
    if (App.config.bit_esp) {
        run(PORT + "/test.esp")
    }
    //  MOB - re-enable php when php shutdown is clean
    if (false && App.config.bit_php) {
        run(PORT + "/test.php")
    }
    if (App.config.bit_cgi) {
        run(PORT + "/test.cgi")
    }
    if (App.config.bit_ejscript) {
        run(PORT + "/test.ejs")
    }
    run(PORT + "/exit.esp")
    let ok = cmd.wait(10000)
    if (cmd.status != 0) {
        App.log.error("valgrind status: " + cmd.status)
        App.log.error("valgrind error: " + cmd.error)
        App.log.error("valgrind output: " + cmd.response)
    }
    assert(cmd.status == 0)
    cmd.stop()

} else {
    if (test.os == "LINUX" && !valgrind) {
        test.skip("Run with valgrind installed")
    } else {
        test.skip("Run on Linux at depth 4 with valgrind installed")
    }
}

