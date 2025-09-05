<?php
//handle POST request to save data
if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    $temperature = $_POST['temperature'] ?? null;
    $humidity = $_POST['humidity'] ?? null;
    $battery = $_POST['battery'] ?? null;

    if ($temperature !== null && $humidity !== null && $battery !== null) {
        //create data array
        $data = [
            'timestamp' => time(),
            'temperature' => (float)$temperature,
            'humidity' => (float)$humidity,
            'battery' => (float)$battery,
        ];

        //read existing data
        $file = 'data.json';
        $existingData = file_exists($file) ? json_decode(file_get_contents($file), true) : [];

        //append new data
        $existingData[] = $data;

        //save updated data
        file_put_contents($file, json_encode($existingData));
        
        echo "Data saved successfully!";
    } else {
        echo "Error: Temperature, Humidity and Battery state are required.";
    }
    exit;
}

//serve the HTML/Chart.js part
?>
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Temperature and Humidity Chart</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <style>
        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
            font-family: 'Arial', sans-serif;
        }

        body {
            background-color: #f4f4f9;
            color: #333;
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: center;
            height: 100vh;
            padding: 20px;
            width: 100vw;
        }

        h1 {
            margin-bottom: 20px;
            font-size: 1.8rem;
            text-align: center;
        }

        .container {
            background: white;
            padding: 20px;
            border-radius: 12px;
            box-shadow: 0 4px 10px rgba(0, 0, 0, 0.1);
            width: 90%;
            display: flex;
            flex-wrap: wrap;
            justify-content: space-between;
        }

        .chart-section {
            flex: 2;
            min-width: 60%;
        }

        .table-section {
            flex: 1;
            min-width: 35%;
            max-height: 60vh;
            overflow-y: auto;
            padding-left: 20px;
        }

        .filters {
            display: flex;
            justify-content: center;
            gap: 10px;
            margin-bottom: 20px;
            flex-wrap: wrap;
        }

        input[type="date"], button {
            padding: 10px;
            font-size: 1rem;
            border-radius: 8px;
            border: 1px solid #ccc;
            outline: none;
            transition: all 0.3s ease;
        }

        button {
            background: #007bff;
            color: white;
            border: none;
            cursor: pointer;
        }

        button:hover {
            background: #0056b3;
        }

        .chart-container {
            width: 100%;
            height: 60vh;
        }

        canvas {
            max-width: 100%;
        }

        table {
            width: 100%;
            border-collapse: collapse;
            margin-top: 10px;
        }

        th, td {
            border: 1px solid #ddd;
            padding: 8px;
            text-align: center;
        }

        th {
            background-color: #007bff;
            color: white;
        }

        @media (max-width: 768px) {
            .container {
                flex-direction: column;
                align-items: center;
            }
            .table-section {
                padding-left: 0;
                width: 100%;
            }
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="chart-section">
            <h1>Temperature & Humidity Data</h1>
            <div class="filters">
                <input type="date" id="start-date">
                <input type="date" id="end-date">
                <button onclick="updateChart()">Filter</button>
            </div>
            <div class="chart-container">
                <canvas id="dataChart"></canvas>
            </div>
        </div>
        <div class="table-section">
            <h2>Data Table</h2>
            <table>
                <thead>
                    <tr>
                        <th>Timestamp</th>
                        <th>Temperature (°C)</th>
                        <th>Humidity (%)</th>
                        <th>Battery (V)</th>
                    </tr>
                </thead>
                <tbody id="dataTableBody">
                </tbody>
            </table>
        </div>
    </div>

    <script>
        let chart;

        async function fetchData() {
            try {
                const response = await fetch('data.json');
                if (!response.ok) throw new Error('Failed to load data');
                return await response.json();
            } catch (error) {
                console.error(error);
                return [];
            }
        }

        async function renderChart(startDate = null, endDate = null) {
            const data = await fetchData();
            if (data.length === 0) {
                console.warn('No data available to display.');
                return;
            }

            const filteredData = data.filter(item => {
                const itemDate = new Date(item.timestamp * 1000);
                return (!startDate || itemDate >= startDate) && (!endDate || itemDate <= endDate);
            }).sort((a, b) => b.timestamp - a.timestamp);

            if (filteredData.length === 0) {
                console.warn('No data found for selected date range.');
                return;
            }

            const timestamps = filteredData.map(item =>
                new Date(item.timestamp * 1000).toLocaleString()
            );
            timestamps.reverse();
            const temperatures = filteredData.map(item => item.temperature);
            temperatures.reverse();
            const humidities = filteredData.map(item => item.humidity);
            humidities.reverse();

            const ctx = document.getElementById('dataChart').getContext('2d');
            if (chart) chart.destroy();

            chart = new Chart(ctx, {
                type: 'line',
                data: {
                    labels: timestamps,
                    datasets: [
                        {
                            label: 'Temperature (°C)',
                            data: temperatures,
                            borderColor: 'red',
                            backgroundColor: 'rgba(255, 0, 0, 0.2)',
                            fill: true,
                        },
                        {
                            label: 'Humidity (%)',
                            data: humidities,
                            borderColor: 'blue',
                            backgroundColor: 'rgba(0, 0, 255, 0.2)',
                            fill: true,
                        }
                    ]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                }
            });

            const tableBody = document.getElementById('dataTableBody');
            tableBody.innerHTML = '';
            filteredData.forEach(item => {
                const row = `<tr>
                                <td>${new Date(item.timestamp * 1000).toLocaleString()}</td>
                                <td>${item.temperature}</td>
                                <td>${item.humidity}</td>
                                <td>${item.battery || "N/A"}</td>
                            </tr>`;
                tableBody.innerHTML += row;
            });
        }

        function updateChart() {
            const startDate = new Date(document.getElementById('start-date').value);
            const endDate = new Date(document.getElementById('end-date').value);
            renderChart(startDate, endDate);
        }
        
        function getDefaultDates() {
            const today = new Date();
            const lastWeek = new Date();
            today.setDate(today.getDate() + 1);
            lastWeek.setDate(today.getDate() - 3);

            const formatDate = (date) => date.toISOString().split('T')[0];

            return {
                startDate: formatDate(lastWeek),
                endDate: formatDate(today)
            };
        }
                                 
        function setDefaults() {
            document.getElementById('start-date').value = getDefaultDates().startDate;
            document.getElementById('end-date').value = getDefaultDates().endDate;
        }

        window.onload = function () {
            setDefaults();
            renderChart(new Date(getDefaultDates().startDate), new Date(getDefaultDates().endDate));
        };
    </script>
</body>
</html>

