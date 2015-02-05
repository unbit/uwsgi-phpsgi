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
