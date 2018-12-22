### Issue Submission Template

Please follow this template when submitting each and every issue. Issues without these details will be marked as "Incomplete" and will not get attention. Make sure you use:

* Use markdown to format. Format code with three back ticks. Look at the source of this issue to see how it is done. 

* Edit down any logs. Please do not t just dump large unedited logs. Only post the relevant portions.

* Use the MakeMe **http** command in preference to a browser to trigger the issue. It is more controllable.

* Wherever possible, create an isolated test case, separate from your application to demonstrate the issue. This helps the team to recreate your issue.

* Include these details on ALL issues.

### Environment

1. O/S Version: 

2. GoAhead Version: 

3. Hardware: 

### Build

1. If built from source, what was your configure command line:

```
./configure [options]
```

### Problem Description:

Put here a full description of the issue. Fully describe the sequence of steps to reproduce the issue.
Include the request URL, the response data and most importantly the HTTP headers in both directions.

Run GoAhead with trace on to capture headers like this:

goahead -4
````
goahead: 4: New connection from 127.0.0.1:56340 to 127.0.0.1:8080

<<< Request
GET /cgi-bin/cgitest?user=john&item=book HTTP/1.1
Accept: */*
Date: Wed, 03 Dec 2014 16:57:43 GMT
Host: 127.0.0.1:8080
Connection: close
User-Agent: Embedthis-http
Accept-Ranges: bytes

>>> Response
HTTP/1.1 200 OK
Server: GoAhead-http
Date: Wed Dec  3 16:57:43 2014
Transfer-Encoding: chunked
Connection: close
Pragma: no-cache
Cache-Control: no-cache
Content-Type:  text/html
````
