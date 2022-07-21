<?php

$cmd = "ggwave-to-file";

if (isset($_GET['s'])) { $cmd .= " -s".intval($_GET['s']); }
if (isset($_GET['v'])) { $cmd .= " -v".intval($_GET['v']); }
if (isset($_GET['p'])) { $cmd .= " -p".intval($_GET['p']); }
if (isset($_GET['l'])) { $cmd .= " -l".intval($_GET['l']); }
if (isset($_GET['d'])) { if (intval($_GET['d']) > 0) $cmd .= " -d"; }

$descriptorspec = array(
    0 => array("pipe", "r"),
    //1 => array("pipe", "w"),
    2 => array("pipe", "w"),
);

$path_wav = tempnam("/tmp", "ggwave");

$cmd .= " > $path_wav";

$process = proc_open($cmd, $descriptorspec, $pipes);

if (is_resource($process)) {
    $message = $_GET['m'];

    fwrite($pipes[0], $message);
    fclose($pipes[0]);

    //$result = stream_get_contents($pipes[1]);
    //fclose($pipes[1]);

    $log = stream_get_contents($pipes[2]);
    fclose($pipes[2]);

    $return_value = proc_close($process);

    //exec("ffmpeg -i ".$path_wav." ".$path_wav);

    $result = file_get_contents($path_wav);
    $size = filesize($path_wav);

    if ($size == 0) {
        header('Content-type: text/plain');
        echo $log;
    } else {
        //header("Content-Type: audio/wav");
        header("Content-Type: ". mime_content_type($path_wav));
        header("Content-Length: $size");
        header("Accept-Ranges: bytes");
        header('Content-Disposition: attachment; filename="output.wav"');
        header("Content-Transfer-Encoding: binary");
        header("Content-Range: bytes 0-".$size."/".$size);

        echo $result;
    }
}

unlink($path_wav);

?>
