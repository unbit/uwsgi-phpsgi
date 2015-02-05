# uwsgi-phpsgi

uWSGI experimental plugin for implementing a WSGI/PSGI/Rack-like interface for php

This is only a proof of concept, do not use it unless you want to help defining specs, fixing bugs or hurt yourself


Installation
============

Ensure you have (on debian/ubuntu) the libphp5-embed package, then

```
uwsgi --build-plugin https://github.com/unbit/uwsgi-phpsgi
```

Usage
=====

```
uwsgi --plugin phpsgi --http-socket :9090
```

will load app.php from the current directory and will execute its 'application' function at every request:

```php
before

<?php

echo "foobar\n";

function application($env) {
        print_r($env);
        echo "into the entry point\n";
        return ["200 Foobar", ["Content-Type" => "text/plain", "Foo" => "Bar", "Multi-Header" => ["One", "Two", "Three"] ], ["Hello World\n", "Test Test\n", $env['PATH_INFO'] ]];
}

echo "application loaded\n";

?>

after
```

SPECS
=====

The entry point function takes a single associative array as argument (this is the CGI dictionary):

```php
Array
(
    [REQUEST_METHOD] => GET
    [REQUEST_URI] => /foobar
    [PATH_INFO] => /foobar
    [QUERY_STRING] => 
    [SERVER_PROTOCOL] => HTTP/1.1
    [SCRIPT_NAME] => 
    [SERVER_NAME] => ubuntu64
    [SERVER_PORT] => 9090
    [REMOTE_ADDR] => 192.168.173.2
    [HTTP_USER_AGENT] => curl/7.37.1
    [HTTP_HOST] => ubuntu64.local:9090
    [HTTP_ACCEPT] => */*
)
```

The function has to return an array of 3 elements or a string.

In the second case a 200 OK status is returned followed by the returned string (NOTE: this is a shitty way, do not use it, it is here only for testing).

The first (and the right) one is returning the http status string as the first array item, an associative array of headers as teh second, and a string (or an array of strings) as the body:

```php
return ["200 Foobar", ["Content-Type" => "text/plain"], ["Hello"]];
```

Every echo/print-like function, returns output in the logs (not on the client like in classic php)

$_SERVER, $_GET and $_POST (as well as the other classic global php variables) are pointless and obviously not available.

TODO:

Define an api for the request body
