<?php
while (ob_get_level()) ob_end_clean();
ini_set('zlib.output_compression', 'Off');
date_default_timezone_set('Europe/Warsaw');

//offset in seconds to add to UTC timestamp
$timezone_offset = 7200;

//download ics
$ics_url = 'https://calendar.google.com/calendar/ical/<your_calendar_id>/public/basic.ics'; //replace this link with your own
$ics_data = file_get_contents($ics_url);

//parse ics
preg_match_all('/BEGIN:VEVENT(.*?)END:VEVENT/s', $ics_data, $matches);
$events = [];

foreach ($matches[1] as $event_data) {
    preg_match('/DTSTART(?:;[^:]+)?:([0-9T]+)/', $event_data, $start_match);
    preg_match('/SUMMARY:(.+)/', $event_data, $summary_match);

    if ($start_match && $summary_match) {
        $start_raw = trim($start_match[1]);
        $timestamp = strtotime($start_raw);
        if ($timestamp !== false && ($timestamp+$timezone_offset) >= time()) {
            $events[] = [
                'time' => $timestamp+$timezone_offset,
                'summary' => trim($summary_match[1])
            ];
        }
    }
}

//sort and limit to 5 upcoming events
usort($events, fn($a, $b) => $a['time'] - $b['time']);
$events = array_slice($events, 0, 5);

//generate image
$width = 960;
$height = 540;
$image = imagecreatetruecolor($width, $height);
$white = imagecolorallocate($image, 255, 255, 255);
$black = imagecolorallocate($image, 0, 0, 0);
$gray = imagecolorallocate($image, 160, 160, 160);
$mid_gray = imagecolorallocate($image, 100, 100, 100);

imagefill($image, 0, 0, $white);

//path to a TTF font file (you can change to yours)
$font_path = __DIR__ . '/arial.ttf';
$font_size_title = 50;
$font_size_text = 30;

//draw title
imagettftext($image, $font_size_title, 0, 20, 95, $black, $font_path, "Upcoming events");

//group events by day label
$grouped_events = [];

foreach ($events as $event) {
    $event_date = date('Y-m-d', $event['time']);
    $today = date('Y-m-d');
    $tomorrow = date('Y-m-d', strtotime('+1 day'));

    if ($event_date === $today) {
        $label = 'Today';
    } elseif ($event_date === $tomorrow) {
        $label = 'Tomorrow';
    } else {
        $label = date('Y-m-d', $event['time']);
    }

    $grouped_events[$label][] = $event;
}

//draw events with categories
$y = 165;
if (empty($events)) {
    imagettftext($image, $font_size_text, 0, 20, $y, $black, $font_path, "No events.");
} else {
    foreach ($grouped_events as $label => $group) {
        if ($y <= 500) {
          //category label
          imagettftext($image, $font_size_text, 0, 20, $y, $black, $font_path, "- $label");
          $y += 50;
          
          //events under this category
          foreach ($group as $event) {
            if ($y <= 500) {
              $line = date('H:i', $event['time']) . " - " . $event['summary'];
              imagettftext($image, $font_size_text, 0, 40, $y, $black, $font_path, $line);
              $y += 45;
            }
          }
          
          $y += 20; //extra space between categories
        }
    }
}

//footer
imagettftext($image, 12, 0, 20, $height - 20, $mid_gray, $font_path, "Generated " . date('Y-m-d H:i:s'));

//output image
ob_start();
imagejpeg($image);
$jpg_data = ob_get_clean();
imagedestroy($image);

//send headers
header('Content-Type: image/jpg');
header('Content-Disposition: inline; filename="events.jpg"');
header('Cache-Control: no-cache');
header('Connection: close');
header('Content-Length: ' . strlen($jpg_data));

echo $jpg_data;
?>
