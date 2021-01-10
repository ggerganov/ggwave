<?php

$cmd = "ggwave-to-file";

if (isset($_GET['s'])) { $cmd .= " -s".intval($_GET['s']); }
if (isset($_GET['v'])) { $cmd .= " -v".intval($_GET['v']); }
if (isset($_GET['p'])) { $cmd .= " -p".intval($_GET['p']); }

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
