#!/Users/mob/git/ejs/macosx-x64-debug/bin/ejs
print("HTTP/1.0 200 OK
Content-Type: text/plain

")
for (let [key,value] in App.env) print(key + "=" + value)
print()
