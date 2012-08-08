#!/bin/sh

echo "Content-type: text/html"
echo ""
echo "PATH $PATH"
echo "<html><body><p>Simple CGI program</p>"
echo "<pre>" 
env
echo "</pre>"
echo "</body></html>"
