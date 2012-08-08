/*
    misc.tst - Misc. Http tests
 */

assert(Uri("a.txt").mimeType == "text/plain")
assert(Uri("a.html").mimeType == "text/html")
assert(Uri("a.json").mimeType == "application/json")
