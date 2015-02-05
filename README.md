# uwsgi-phpsgi

uWSGI experimental plugin for implementing a WSGI/PSGI/Rack-like interface for php


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
