<?php

$cmd = "ggwave-to-file";

if (isset($_GET['p']) && $_GET['p'] == '0') { $cmd .= " -p0"; }
if (isset($_GET['p']) && $_GET['p'] == '1') { $cmd .= " -p1"; }
if (isset($_GET['p']) && $_GET['p'] == '2') { $cmd .= " -p2"; }
if (isset($_GET['p']) && $_GET['p'] == '3') { $cmd .= " -p3"; }
if (isset($_GET['p']) && $_GET['p'] == '4') { $cmd .= " -p4"; }
if (isset($_GET['p']) && $_GET['p'] == '5') { $cmd .= " -p5"; }

$descriptorspec = array(
    0 => array("pipe", "r"),
    1 => array("pipe", "w"),
    2 => array("pipe", "w"),
);

$process = proc_open($cmd, $descriptorspec, $pipes);

if (is_resource($process)) {
    $message = $_GET['m'];

    fwrite($pipes[0], $message);
    fclose($pipes[0]);

    $result = stream_get_contents($pipes[1]);
    fclose($pipes[1]);

    $log = stream_get_contents($pipes[2]);
    fclose($pipes[2]);

    $return_value = proc_close($process);

    if (strlen($result) == 0) {
        header('Content-type: text/plain');
        echo $log;
    } else {
        header('Content-Disposition: attachment; filename="output.wav"');
        header("Content-Transfer-Encoding: binary");
        header("Content-Type: audio/wav");

        echo $result;
    }
}

?>
