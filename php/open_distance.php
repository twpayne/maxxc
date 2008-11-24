<?php

$R = 6371.0; # radius of the FAI sphere

class Track {

	var $coords;
	var $N;
	var $max_delta;
	var $sigma_delta;

	# initialize a new Track object
	function Track($coords) {
		$this->coords = $coords;
		$this->N = count($coords);
		$this->max_delta = 0;
		for ($i = 1; $i < $this->N; ++$i) {
			$delta = $this->delta($i - 1, $i);
			$this->max_delta = max($this->max_delta, $delta);
		}
	}

	# return the distance in km between the points $i and $j
	function delta($i, $j) {
		global $R;
		$x = sin($this->coords[$i][0]) * sin($this->coords[$j][0]) + cos($this->coords[$i][0]) * cos($this->coords[$j][0]) * cos($this->coords[$i][1] - $this->coords[$j][1]);
		return $x < 1.0 ? $R * acos($x) : 0.0;
	}

	# return the index of the first point which is at most $distance km from point $i
	function forward($i, $distance) {
		$step = $distance / $this->max_delta;
		return $step > 0 ? $i + $step : ++$i;
	}

	# return the index of the point between $begin and $end that is furthest from $i and at least $bound km from $i
	function furthest_from($i, $begin, $end, &$bound) {
		$result = -1;
		for ($j = $begin; $j < $end; ) {
			$distance = $this->delta($i, $j);
			if ($distance > $bound) {
				$bound = $distance;
				$result = $j;
				++$j;
			} else {
				$j = $this->forward($j, $bound - $distance);
			}
		}
		return $result;
	}

	# return the greatest distance from take-off (point 0) that is at least $bound km from take-off
	function max_distance_from_take_off(&$bound) {
		$this->furthest_from(0, 1, $this->N, $bound);
		return $bound;
	}

	# return the greatest distance in km between any two points that is at least $bound km
	function open_distance(&$bound) {
		for ($i = 0; $i < $this->N - 1; ++$i)
			$this->furthest_from($i, $i + 1, $this->N, $bound);
		return $bound;
	}

};

# return an array of coordinates in $file
function parse_igc_coords($file) {
	$result = array();
	while (!feof($file)) {
		$line = fgets($file, 128);
		if (!preg_match('/\AB\d{6}(\d{2})(\d{5})([NS])(\d{3})(\d{5})([EW])[AV]\d{10}\d*\r\n\z/', $line, $matches))
			continue;
		$lat = M_PI * ($matches[1] + $matches[2] / 60000.0) / 180.0;
		if ($matches[3] == 'S')
			$lat *= -1;
		$lon = M_PI * ($matches[4] + $matches[5] / 60000.0) / 180.0;
		if ($matches[6] == 'W')
			$lon *= -1;
		array_push($result, array($lat, $lon));
	}
	return $result;
}

$file = fopen('php://stdin', 'r');
$coords = parse_igc_coords($file);
$track = new Track($coords);
$bound = 0.0;
printf("Max-distance-from-take-off: %s km\n", $track->max_distance_from_take_off($bound));
printf("Open-distance: %s km\n", $track->open_distance($bound));

?>
