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
$lig_gray = imagecolorallocate($image, 200, 200, 200);

imagefill($image, 0, 0, $white);

//path to a TTF font file (you can change to yours)
$font_path = __DIR__ . '/arial.ttf';
$font_size_title = 50;
$font_size_text = 30;
$font_size_calendar = 14;

//draw title
imagettftext($image, $font_size_title, 0, 20, 95, $black, $font_path, "Upcoming events");

//group events by day label
$grouped_events = [];

foreach ($events as $event) {
    $event_date;
    if (date('Y', $event['time']) == date('Y')) {
        $event_date = date('d.m', $event['time']);
    } else {
        $event_date = date('d.m.Y', $event['time']);
    }
    
    $today = date('d.m');
    $tomorrow = date('d.m', strtotime('+1 day'));

    if ($event_date === $today) {
        $label = 'Today';
    } elseif ($event_date === $tomorrow) {
        $label = 'Tomorrow';
    } else {
        if (date('Y', $event['time']) == date('Y')) {
            $label = date('d.m', $event['time']);
        } else {
            $label = date('d.m.Y', $event['time']);
        }
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

//calendar

$dayOfWeek = (int) (new DateTime('first day of this month'))->format('N');
$month = (int) date("n");
$year = (int) date("Y");
$daysInMonth = (int) date("t");
$today = (int) date("j");

if ($dayOfWeek == 1 && $month == 2 && $daysInMonth) {
    $rectHeight = 150+(5*35);
} else {
    $rectHeight = 150+(6*35);
}
imagefilledrectangle($image, 660, 150, 905, $rectHeight, $white);
imagesetthickness($image, 2);
imagefilledrectangle($image, 660, 150, 905, 185, $black);
$daysArray = ["MO", "TU", "WE", "TH", "FR", "SA", "SU"];
for($x=1; $x<=7; $x++) {
    imageline($image, 660+($x*35), 150, 660+($x*35), $rectHeight, $black);
    imagettftext($image, $font_size_calendar, 0, 630+($x*35)-1, 175, $white, $font_path, $daysArray[$x-1]);
}

imageline($image, 660, 185, 905, 185, $black);

$Y = 210;
for ($day=1; $day<=$daysInMonth; $day++) {
    $x = (strlen((string) $day) == 1 ? 636 : 632)+(((($dayOfWeek-1)%7)+1)*35);
    $dayFormatted = "".((strlen((string)$day)) == 1 ? "0".(string)$day : $day).".".((strlen((string)$month)) == 1 ? "0".(string)$month : $month);
    if (!empty($grouped_events[$dayFormatted])) {
        imagefilledrectangle($image, $x-6, $Y-24, $x+26, $Y+8, $lig_gray);
    }
    if ($day == $today) {
        imagefilledrectangle($image, $x-6, $Y-24, $x+26, $Y+8, $black);
    }
    imagettftext($image, $font_size_calendar, 0, $x, $Y, $day == $today ? $white : $black, $font_path, (string) $day);
    if ((($dayOfWeek-1)%7)==6) {
        $Y+=35;
        imageline($image, 660, 185+($Y-210), 905, 185+($Y-210), $black);
    }
    $dayOfWeek++;
}
imagesetthickness($image, 3);
imagerectangle($image, 660, 150, 905, $rectHeight, $black);
imagesetthickness($image, 1);

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
