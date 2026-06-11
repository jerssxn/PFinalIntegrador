#ifndef WEBPAGE_H
#define WEBPAGE_H

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>VitalGuard S3 - Panel de Telemetría Médica</title>
    <style>
        :root {
            --bg-color: #0b0f19;
            --card-bg: rgba(22, 28, 45, 0.6);
            --card-border: rgba(255, 255, 255, 0.08);
            --text-primary: #f3f4f6;
            --text-secondary: #9ca3af;
            --primary: #ef4444; /* Heart Red */
            --primary-glow: rgba(239, 68, 68, 0.4);
            --oxygen: #06b6d4; /* Cyan Oxygen */
            --oxygen-glow: rgba(6, 182, 212, 0.4);
            --success: #10b981; /* Green */
            --success-glow: rgba(16, 185, 129, 0.2);
            --warning: #f59e0b; /* Amber */
            --warning-glow: rgba(245, 158, 11, 0.2);
        }

        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
        }

        body {
            background-color: var(--bg-color);
            background-image: 
                radial-gradient(at 0% 0%, rgba(31, 41, 55, 0.5) 0, transparent 50%),
                radial-gradient(at 100% 0%, rgba(17, 24, 39, 0.8) 0, transparent 50%);
            color: var(--text-primary);
            min-height: 100vh;
            padding: 1.5rem;
            display: flex;
            flex-direction: column;
            gap: 1.5rem;
            overflow-x: hidden;
        }

        /* Glassmorphism Header */
        header {
            background: var(--card-bg);
            backdrop-filter: blur(12px);
            -webkit-backdrop-filter: blur(12px);
            border: 1px solid var(--card-border);
            padding: 1.25rem 2rem;
            border-radius: 16px;
            display: flex;
            justify-content: space-between;
            align-items: center;
            box-shadow: 0 8px 32px 0 rgba(0, 0, 0, 0.37);
        }

        .logo-area {
            display: flex;
            align-items: center;
            gap: 0.75rem;
        }

        .logo-icon {
            width: 32px;
            height: 32px;
            background: linear-gradient(135deg, var(--primary), #ec4899);
            border-radius: 8px;
            display: flex;
            align-items: center;
            justify-content: center;
            box-shadow: 0 0 15px var(--primary-glow);
            animation: pulse-logo 2s infinite;
        }

        .logo-text h1 {
            font-size: 1.3rem;
            font-weight: 800;
            letter-spacing: 1px;
            background: linear-gradient(to right, #ffffff, #9ca3af);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
        }

        .logo-text span {
            font-size: 0.7rem;
            color: var(--primary);
            font-weight: 700;
            text-transform: uppercase;
        }

        .status-badge {
            background: rgba(16, 185, 129, 0.1);
            border: 1px solid rgba(16, 185, 129, 0.3);
            color: var(--success);
            padding: 0.4rem 1rem;
            border-radius: 50px;
            font-size: 0.8rem;
            font-weight: 600;
            display: flex;
            align-items: center;
            gap: 0.5rem;
            box-shadow: 0 0 10px var(--success-glow);
        }

        .status-badge.disconnected {
            background: rgba(239, 68, 68, 0.1);
            border: 1px solid rgba(239, 68, 68, 0.3);
            color: var(--primary);
            box-shadow: 0 0 10px var(--primary-glow);
        }

        .status-dot {
            width: 8px;
            height: 8px;
            background-color: currentColor;
            border-radius: 50%;
            display: inline-block;
            animation: blink 1.2s infinite;
        }

        /* Responsive Dashboard Grid */
        .dashboard-grid {
            display: grid;
            grid-template-columns: 2fr 1fr;
            gap: 1.5rem;
            flex-grow: 1;
        }

        @media (max-width: 1024px) {
            .dashboard-grid {
                grid-template-columns: 1fr;
            }
        }

        .left-column {
            display: flex;
            flex-direction: column;
            gap: 1.5rem;
        }

        /* Premium Card Design */
        .card {
            background: var(--card-bg);
            backdrop-filter: blur(12px);
            -webkit-backdrop-filter: blur(12px);
            border: 1px solid var(--card-border);
            border-radius: 16px;
            padding: 1.5rem;
            box-shadow: 0 8px 32px 0 rgba(0, 0, 0, 0.3);
            transition: transform 0.3s ease, border-color 0.3s ease;
        }

        .card:hover {
            border-color: rgba(255, 255, 255, 0.15);
        }

        .card-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 1.25rem;
        }

        .card-title {
            font-size: 1rem;
            font-weight: 600;
            color: var(--text-secondary);
            display: flex;
            align-items: center;
            gap: 0.5rem;
        }

        /* PPG Waveform Card */
        .wave-container {
            position: relative;
            width: 100%;
            height: 250px;
            background: rgba(10, 14, 23, 0.8);
            border-radius: 12px;
            border: 1px solid rgba(255, 255, 255, 0.05);
            overflow: hidden;
        }

        canvas {
            display: block;
            width: 100%;
            height: 100%;
        }

        .chart-grid-overlay {
            position: absolute;
            inset: 0;
            pointer-events: none;
            background-size: 30px 30px;
            background-image: 
                linear-gradient(to right, rgba(255, 255, 255, 0.03) 1px, transparent 1px),
                linear-gradient(to bottom, rgba(255, 255, 255, 0.03) 1px, transparent 1px);
        }

        /* Numerical Indicators Grid */
        .metrics-grid {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 1.5rem;
        }

        @media (max-width: 640px) {
            .metrics-grid {
                grid-template-columns: 1fr;
            }
        }

        .metric-card {
            display: flex;
            flex-direction: column;
            justify-content: center;
            position: relative;
            overflow: hidden;
        }

        .metric-value-wrap {
            display: flex;
            align-items: baseline;
            gap: 0.5rem;
            margin-top: 0.5rem;
        }

        .metric-value {
            font-size: 4rem;
            font-weight: 800;
            line-height: 1;
        }

        .bpm-text {
            color: var(--primary);
            text-shadow: 0 0 20px var(--primary-glow);
        }

        .spo2-text {
            color: var(--oxygen);
            text-shadow: 0 0 20px var(--oxygen-glow);
        }

        .metric-unit {
            font-size: 1.25rem;
            font-weight: 600;
            color: var(--text-secondary);
        }

        .pulse-icon {
            font-size: 2rem;
            animation: heartbeat 1s infinite alternate;
            display: inline-block;
        }

        .pulse-active {
            animation: heartbeat-fast 0.6s infinite;
        }

        /* Diagnostic & Details Panel (Right Column) */
        .right-column {
            display: flex;
            flex-direction: column;
            gap: 1.5rem;
        }

        .diagnostic-box {
            display: flex;
            flex-direction: column;
            gap: 1rem;
            background: rgba(255, 255, 255, 0.02);
            border: 1px solid rgba(255, 255, 255, 0.05);
            border-radius: 12px;
            padding: 1.25rem;
            margin-top: 1rem;
        }

        .diag-status {
            font-size: 1.2rem;
            font-weight: 700;
            padding: 0.5rem 1rem;
            border-radius: 8px;
            text-align: center;
            border: 1px solid transparent;
        }

        .diag-status.normal {
            background: rgba(16, 185, 129, 0.1);
            border-color: rgba(16, 185, 129, 0.2);
            color: var(--success);
        }

        .diag-status.warning {
            background: rgba(245, 158, 11, 0.1);
            border-color: rgba(245, 158, 11, 0.2);
            color: var(--warning);
        }

        .diag-status.danger {
            background: rgba(239, 68, 68, 0.1);
            border-color: rgba(239, 68, 68, 0.2);
            color: var(--primary);
        }

        .diagnostic-info {
            font-size: 0.9rem;
            color: var(--text-secondary);
            line-height: 1.5;
        }

        /* Log Table */
        .log-container {
            max-height: 200px;
            overflow-y: auto;
            border-radius: 8px;
            border: 1px solid rgba(255, 255, 255, 0.05);
        }

        .log-table {
            width: 100%;
            border-collapse: collapse;
            font-size: 0.85rem;
            text-align: left;
        }

        .log-table th {
            background: rgba(255, 255, 255, 0.05);
            padding: 0.75rem;
            font-weight: 600;
            color: var(--text-secondary);
            position: sticky;
            top: 0;
            z-index: 1;
        }

        .log-table td {
            padding: 0.75rem;
            border-bottom: 1px solid rgba(255, 255, 255, 0.03);
        }

        .log-table tr:hover td {
            background: rgba(255, 255, 255, 0.02);
        }

        /* Buttons and Interactive Elements */
        .btn-group {
            display: flex;
            gap: 0.75rem;
            margin-top: 1rem;
        }

        .btn {
            flex: 1;
            padding: 0.75rem 1rem;
            border-radius: 8px;
            font-weight: 600;
            font-size: 0.85rem;
            cursor: pointer;
            border: 1px solid transparent;
            transition: all 0.2s ease;
            display: flex;
            justify-content: center;
            align-items: center;
            gap: 0.5rem;
        }

        .btn-primary {
            background: linear-gradient(135deg, #ef4444, #b91c1c);
            color: white;
            box-shadow: 0 4px 15px rgba(239, 68, 68, 0.3);
        }

        .btn-primary:hover {
            transform: translateY(-1px);
            box-shadow: 0 6px 20px rgba(239, 68, 68, 0.4);
        }

        .btn-secondary {
            background: rgba(255, 255, 255, 0.05);
            color: var(--text-primary);
            border-color: rgba(255, 255, 255, 0.1);
        }

        .btn-secondary:hover {
            background: rgba(255, 255, 255, 0.1);
            border-color: rgba(255, 255, 255, 0.2);
        }

        /* Animations */
        @keyframes blink {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.3; }
        }

        @keyframes pulse-logo {
            0%, 100% { transform: scale(1); box-shadow: 0 0 15px var(--primary-glow); }
            50% { transform: scale(1.05); box-shadow: 0 0 25px rgba(239, 68, 68, 0.7); }
        }

        @keyframes heartbeat {
            0% { transform: scale(1); }
            14% { transform: scale(1.3); }
            28% { transform: scale(1.15); }
            42% { transform: scale(1.4); }
            70% { transform: scale(1); }
        }

        @keyframes heartbeat-fast {
            0% { transform: scale(1); }
            50% { transform: scale(1.2); }
            100% { transform: scale(1); }
        }

        /* Footer styling */
        footer {
            text-align: center;
            padding: 1rem;
            color: var(--text-secondary);
            font-size: 0.75rem;
            border-top: 1px solid var(--card-border);
            margin-top: auto;
        }

        .no-data-msg {
            color: var(--warning);
            font-size: 0.9rem;
            text-align: center;
            padding: 2rem 0;
            font-weight: 500;
        }
    </style>
</head>
<body>

    <!-- Header area -->
    <header>
        <div class="logo-area">
            <div class="logo-icon">
                <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="3" stroke-linecap="round" stroke-linejoin="round" style="color: white;">
                    <path d="M22 12h-4l-3 9L9 3l-3 9H2"/>
                </svg>
            </div>
            <div class="logo-text">
                <h1>VITALGUARD S3</h1>
                <span>ESP32S3 Health Telemetry</span>
            </div>
        </div>
        <div class="status-badge" id="conn-badge">
            <span class="status-dot"></span>
            <span id="conn-text">Monitoreo Activo</span>
        </div>
    </header>

    <!-- Main Grid -->
    <div class="dashboard-grid">
        
        <!-- Left Column (Signal & Metrics) -->
        <div class="left-column">
            
            <!-- PPG Waveform Card -->
            <div class="card">
                <div class="card-header">
                    <div class="card-title">
                        <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                            <polyline points="22 12 18 12 15 21 9 3 6 12 2 12"/>
                        </svg>
                        Onda Fotopletismográfica (PPG) Real-Time
                    </div>
                    <span style="font-size: 0.8rem; color: var(--text-secondary);" id="fps-counter">Signal rate: 40Hz</span>
                </div>
                <div class="wave-container">
                    <div class="chart-grid-overlay"></div>
                    <canvas id="ppgCanvas"></canvas>
                </div>
            </div>

            <!-- Numbers / Metrics Grid -->
            <div class="metrics-grid">
                
                <!-- BPM Card -->
                <div class="card metric-card">
                    <div class="card-header">
                        <div class="card-title">
                            <span class="pulse-icon">❤️</span> Frecuencia Cardíaca
                        </div>
                    </div>
                    <div class="metric-value-wrap">
                        <span class="metric-value bpm-text" id="bpm-val">--</span>
                        <span class="metric-unit">BPM</span>
                    </div>
                    <div style="font-size: 0.8rem; color: var(--text-secondary); margin-top: 0.5rem;" id="bpm-detail">
                        Esperando señal estable...
                    </div>
                </div>

                <!-- SpO2 Card -->
                <div class="card metric-card">
                    <div class="card-header">
                        <div class="card-title">
                            <span>💧</span> Saturación de Oxígeno
                        </div>
                    </div>
                    <div class="metric-value-wrap">
                        <span class="metric-value spo2-text" id="spo2-val">--</span>
                        <span class="metric-unit">%</span>
                    </div>
                    <div style="font-size: 0.8rem; color: var(--text-secondary); margin-top: 0.5rem;" id="spo2-detail">
                        Oximetría de pulso infrarrojo
                    </div>
                </div>

            </div>

        </div>

        <!-- Right Column (Diagnostics & Log) -->
        <div class="right-column">
            
            <!-- Diagnosis & Health status -->
            <div class="card">
                <div class="card-header">
                    <div class="card-title">
                        <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                            <path d="M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z"/>
                        </svg>
                        Diagnóstico Clínico
                    </div>
                </div>
                <div class="diagnostic-box">
                    <div class="diag-status normal" id="diag-badge">ESPERANDO DEDO</div>
                    <p class="diagnostic-info" id="diag-desc">
                        Coloque la yema de su dedo índice suavemente sobre el sensor MAX30102. Evite presionar demasiado fuerte para no obstruir el flujo capilar.
                    </p>
                </div>
            </div>

            <!-- Real-time Event Log -->
            <div class="card" style="flex-grow: 1; display: flex; flex-direction: column;">
                <div class="card-header">
                    <div class="card-title">
                        <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                            <path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"/>
                            <polyline points="14 2 14 8 20 8"/>
                            <line x1="16" y1="13" x2="8" y2="13"/>
                            <line x1="16" y1="17" x2="8" y2="17"/>
                            <polyline points="10 9 9 9 8 9"/>
                        </svg>
                        Historial de Mediciones
                    </div>
                </div>
                
                <div class="log-container">
                    <table class="log-table" id="log-table">
                        <thead>
                            <tr>
                                <th>Hora</th>
                                <th>BPM</th>
                                <th>SpO2</th>
                                <th>Condición</th>
                            </tr>
                        </thead>
                        <tbody id="log-body">
                            <tr id="no-data-row">
                                <td colspan="4" class="no-data-msg">No hay registros guardados</td>
                            </tr>
                        </tbody>
                    </table>
                </div>

                <div class="btn-group">
                    <button class="btn btn-secondary" onclick="clearLog()">Limpiar</button>
                    <button class="btn btn-primary" onclick="downloadCSV()">Exportar CSV</button>
                </div>
            </div>

        </div>

    </div>

    <!-- Footer -->
    <footer>
        VitalGuard S3 © 2026 - Proyecto Integrador de Ingeniería | Desarrollado con ESP32-S3 y MAX30102
    </footer>

    <!-- Scripts -->
    <script>
        // WebSocket setup
        let gateway = `ws://${window.location.hostname}:81/`;
        let websocket;
        
        // PPG Chart configuration
        const canvas = document.getElementById('ppgCanvas');
        const ctx = canvas.getContext('2d');
        let ppgData = [];
        const maxPoints = 150; // Scroll length
        
        // UI elements
        const bpmVal = document.getElementById('bpm-val');
        const spo2Val = document.getElementById('spo2-val');
        const connBadge = document.getElementById('conn-badge');
        const connText = document.getElementById('conn-text');
        const pulseIcon = document.querySelector('.pulse-icon');
        const diagBadge = document.getElementById('diag-badge');
        const diagDesc = document.getElementById('diag-desc');
        const logBody = document.getElementById('log-body');
        
        // Internal variables
        let measurementHistory = [];
        let stableMeasurementCounter = 0;
        let lastBpm = 0;
        let lastSpo2 = 0;

        // Resize Canvas dynamically
        function resizeCanvas() {
            canvas.width = canvas.parentElement.clientWidth;
            canvas.height = canvas.parentElement.clientHeight;
        }
        window.addEventListener('resize', resizeCanvas);
        resizeCanvas();

        // WebSocket initialization
        function initWebSocket() {
            console.log('Iniciando conexion WebSocket...');
            websocket = new WebSocket(gateway);
            websocket.onopen = onOpen;
            websocket.onclose = onClose;
            websocket.onmessage = onMessage;
            websocket.onerror = (err) => console.error("Error WS:", err);
        }

        function onOpen(event) {
            console.log('WS Conectado');
            connBadge.className = 'status-badge';
            connText.innerText = 'Monitoreo Activo';
            document.querySelector('.status-dot').style.animation = 'blink 1.2s infinite';
        }

        function onClose(event) {
            console.log('WS Desconectado');
            connBadge.className = 'status-badge disconnected';
            connText.innerText = 'Desconectado';
            document.querySelector('.status-dot').style.animation = 'none';
            // Reconnect logic
            setTimeout(initWebSocket, 2000);
        }

        // Process incoming telemetry data
        function onMessage(event) {
            try {
                let data = JSON.parse(event.data);
                
                // 1. Plot raw / filtered value
                if (data.type === "ppg") {
                    ppgData.push(data.val);
                    if (ppgData.length > maxPoints) {
                        ppgData.shift();
                    }
                    drawPPG();
                } 
                // 2. Metrics package
                else if (data.type === "stats") {
                    let bpm = Math.round(data.bpm);
                    let spo2 = Math.round(data.spo2);
                    let hasFinger = data.finger;

                    if (!hasFinger) {
                        // Reset displays
                        bpmVal.innerText = "--";
                        spo2Val.innerText = "--";
                        diagBadge.innerText = "ESPERANDO DEDO";
                        diagBadge.className = "diag-status warning";
                        diagDesc.innerText = "Coloque la yema de su dedo índice suavemente sobre el sensor MAX30102.";
                        pulseIcon.classList.remove('pulse-active');
                        stableMeasurementCounter = 0;
                        return;
                    }

                    // Pulse effect
                    if (data.beat) {
                        pulseIcon.classList.add('pulse-active');
                        setTimeout(() => pulseIcon.classList.remove('pulse-active'), 150);
                    }

                    // Display readings
                    if (bpm > 30 && bpm < 200) {
                        bpmVal.innerText = bpm;
                        lastBpm = bpm;
                    } else {
                        bpmVal.innerText = "--";
                    }

                    if (spo2 > 50 && spo2 <= 100) {
                        spo2Val.innerText = spo2;
                        lastSpo2 = spo2;
                    } else {
                        spo2Val.innerText = "--";
                    }

                    // Diagnostics and status logic
                    evaluateHealthStatus(bpm, spo2);
                    
                    // Periodically record stable measurements in the history log
                    if (bpm > 45 && bpm < 160 && spo2 > 80 && spo2 <= 100) {
                        stableMeasurementCounter++;
                        // Every ~5 seconds of stable data, log it
                        if (stableMeasurementCounter >= 10) {
                            addLogEntry(bpm, spo2);
                            stableMeasurementCounter = 0;
                        }
                    }
                }
            } catch (e) {
                // Ignore parsing errors of non-JSON packets
            }
        }

        // Draw the ECG/PPG wave on HTML5 Canvas
        function drawPPG() {
            ctx.clearRect(0, 0, canvas.width, canvas.height);
            
            if (ppgData.length < 2) return;
            
            // Auto scaling logic based on min/max of current dataset
            let minVal = Math.min(...ppgData);
            let maxVal = Math.max(...ppgData);
            let range = maxVal - minVal;
            if (range === 0) range = 1;
            
            // Leave a 10% padding top/bottom
            let pad = range * 0.1;
            minVal -= pad;
            maxVal += pad;
            range = maxVal - minVal;

            ctx.lineWidth = 3.5;
            ctx.lineJoin = 'round';
            ctx.lineCap = 'round';

            // Create beautiful gradient for the wave
            let gradient = ctx.createLinearGradient(0, 0, canvas.width, 0);
            gradient.addColorStop(0, '#ef4444');
            gradient.addColorStop(0.5, '#ec4899');
            gradient.addColorStop(1, '#06b6d4');
            ctx.strokeStyle = gradient;

            // Draw line
            ctx.beginPath();
            let sliceWidth = canvas.width / (maxPoints - 1);
            let x = 0;

            for (let i = 0; i < ppgData.length; i++) {
                // Normalize value to 0.0 - 1.0, then scale to canvas height (Y goes down!)
                let norm = (ppgData[i] - minVal) / range;
                let y = canvas.height - (norm * canvas.height);
                
                if (i === 0) {
                    ctx.moveTo(x, y);
                } else {
                    ctx.lineTo(x, y);
                }
                x += sliceWidth;
            }
            
            ctx.stroke();

            // Glow under the wave path
            ctx.shadowBlur = 10;
            ctx.shadowColor = 'rgba(239, 68, 68, 0.4)';
            
            // Draw a subtle area fill below the line
            ctx.beginPath();
            ctx.moveTo(0, canvas.height);
            x = 0;
            for (let i = 0; i < ppgData.length; i++) {
                let norm = (ppgData[i] - minVal) / range;
                let y = canvas.height - (norm * canvas.height);
                ctx.lineTo(x, y);
                x += sliceWidth;
            }
            ctx.lineTo(x - sliceWidth, canvas.height);
            ctx.closePath();
            
            let fillGrad = ctx.createLinearGradient(0, 0, 0, canvas.height);
            fillGrad.addColorStop(0, 'rgba(239, 68, 68, 0.15)');
            fillGrad.addColorStop(1, 'rgba(6, 182, 212, 0.0)');
            ctx.fillStyle = fillGrad;
            ctx.shadowBlur = 0; // turn off glow for fill
            ctx.fill();
        }

        // Diagnostic analyzer
        function evaluateHealthStatus(bpm, spo2) {
            if (spo2 <= 0 || bpm <= 0) return;
            
            let badge = diagBadge;
            let desc = diagDesc;

            if (spo2 < 90) {
                badge.innerText = "ALERTA DE HIPOXIA";
                badge.className = "diag-status danger";
                desc.innerText = "Saturación muy baja de oxígeno detectada (" + spo2 + "%). Podría ser señal de hipoxia. Respire profundamente y descanse. Busque atención médica si persiste.";
            } else if (spo2 >= 90 && spo2 < 95) {
                badge.innerText = "OXIGENACIÓN BAJA";
                badge.className = "diag-status warning";
                desc.innerText = "Nivel de oxígeno ligeramente disminuido. Recomendable mantenerse en reposo, respirar de forma constante y verificar que el sensor esté bien colocado.";
            } else {
                // SpO2 OK, check heart rate
                if (bpm > 100) {
                    badge.innerText = "TAQUICARDIA";
                    badge.className = "diag-status warning";
                    desc.innerText = "Frecuencia cardíaca acelerada (" + bpm + " BPM) en reposo. Puede deberse a esfuerzo físico, estrés, ansiedad o cafeína.";
                } else if (bpm < 50) {
                    badge.innerText = "BRADICARDIA";
                    badge.className = "diag-status warning";
                    desc.innerText = "Frecuencia cardíaca baja (" + bpm + " BPM). Común en atletas de alto rendimiento o durante reposo profundo. Si se acompaña de mareos, consulte a un médico.";
                } else {
                    badge.innerText = "RITMO SINUSAL NORMAL";
                    badge.className = "diag-status normal";
                    desc.innerText = "Tanto el ritmo cardíaco (" + bpm + " BPM) como la saturación de oxígeno (" + spo2 + "%) se encuentran en niveles fisiológicos óptimos en reposo.";
                }
            }
        }

        // Add telemetry record to UI table
        function addLogEntry(bpm, spo2) {
            let timeStr = new Date().toLocaleTimeString();
            let condition = diagBadge.innerText;
            
            // Save inside internal array
            measurementHistory.push({ time: timeStr, bpm: bpm, spo2: spo2, cond: condition });
            
            // Remove "No data" row if it exists
            const noData = document.getElementById('no-data-row');
            if (noData) noData.remove();
            
            // Insert new row in table
            let tr = document.createElement('tr');
            
            let classColor = "var(--text-primary)";
            if (condition.includes("ALERTA") || condition.includes("HIPOXIA")) classColor = "var(--primary)";
            else if (condition.includes("NORMAL")) classColor = "var(--success)";
            else classColor = "var(--warning)";

            tr.innerHTML = `
                <td>${timeStr}</td>
                <td style="font-weight: 700; color: var(--primary);">${bpm}</td>
                <td style="font-weight: 700; color: var(--oxygen);">${spo2}%</td>
                <td style="color: ${classColor}; font-weight: 600;">${condition}</td>
            `;
            
            logBody.insertBefore(tr, logBody.firstChild);

            // Limit history entries shown in UI
            if (logBody.children.length > 25) {
                logBody.lastChild.remove();
            }
        }

        // Export data as Excel/CSV
        function downloadCSV() {
            if (measurementHistory.length === 0) {
                alert("No hay mediciones en el historial para exportar.");
                return;
            }
            
            let csvContent = "data:text/csv;charset=utf-8,";
            csvContent += "Hora,BPM,Saturacion SpO2 (%),Condicion Clinica\n";
            
            measurementHistory.forEach(row => {
                csvContent += `"${row.time}",${row.bpm},${row.spo2},"${row.cond}"\n`;
            });
            
            let encodedUri = encodeURI(csvContent);
            let link = document.createElement("a");
            link.setAttribute("href", encodedUri);
            link.setAttribute("download", `reporte_salud_vitalguard_${new Date().toISOString().slice(0,10)}.csv`);
            document.body.appendChild(link);
            link.click();
            document.body.removeChild(link);
        }

        // Clear history
        function clearLog() {
            if (confirm("¿Está seguro de borrar el historial de mediciones?")) {
                measurementHistory = [];
                logBody.innerHTML = `
                    <tr id="no-data-row">
                        <td colspan="4" class="no-data-msg">No hay registros guardados</td>
                    </tr>
                `;
            }
        }

        // Start WebSocket connection
        window.onload = initWebSocket;
    </script>
</body>
</html>
)rawliteral";

#endif // WEBPAGE_H
