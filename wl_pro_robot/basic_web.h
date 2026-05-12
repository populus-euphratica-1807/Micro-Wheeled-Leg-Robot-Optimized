const char basic_web[] PROGMEM = R"=====(

<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>legged wheel robot web ctrl</title>
  <style>
    h2{
        width: auto;
        height: 60px;
        line-height: 60px;
        text-align: center;
        font-family: 等线;
        color: white;
        background-color:cornflowerblue;
        border-radius: 12px;
    }
    input{
        width: 160px;
        height: 30px;
        margin: 0px;
    }
    .sliderLabel{
        float: left;
        text-align: center;
        line-height: 30px;
        height: 30px;  
        width: 200px;
    }
    .sliders{
        width: 400px;
        height: 150px;
        margin: 10px auto;
        position: relative;
    }
    .selects{
        width: 300px;
        height: 50px;
        padding: 10px;
        margin: 10px auto;
        position: relative;
        border-radius: 10px;
    }
    .view {
        width: 250px;
        height: 50px;
        padding: 0px;
        margin: 20px auto;
    }
    .view2 {
        width: 140px;
        height: 30px;
        padding: 0px;
        margin: 15px auto;
        vertical-align:middle;
    }
    .btn1 {
        width: 80px;
        height: 40px;
        padding: 0px;
        margin: 0px;
        border-radius: 10px;
        background-color: white;
        display: inline-block;
    }
    form {
        width: 80px;
        height: 40px;
        padding: 0px;
        margin: 10px auto;
        display: inline-block;
    }
    .buttons{
        width: 300px;
        height: 180px;
        padding: 10px;
        margin: 10px auto;
        position: relative;
    }
    .dir{
        font-size: 15px;
        width: 100px;
        height: 60px;
        text-align: center;
        border-radius: 12px;
        background-color: white;
        color: cornflowerblue;
        border: 3px solid cornflowerblue;
        padding: 0px;
        transition: all 0.3s;
    }
    option {
        text-align: center;
        font-size: 15px;
    }
    #reset {
        width: 100px;
        height: 40px;
        text-align: center;
        border-radius: 12px;
        border: 2px solid;
        font-size: 15px;
        display: inline-block;
        position: absolute;
        top: 10px;
        left: 0px;
    }
    #gait_select {
        width: 100px;
        height: 40px;
        border: 2px solid red;
        font-size: 15px;
        color: red;
        border-radius: 12px;
        display: inline-block;
        position: absolute;
        top: 10px;
        left: 110px;
    }
    #gait_submit {
        width: 100px;
        height: 40px;
        text-align: center;
        border: 2px solid red;
        color: red;
        font-size: 15px;
        display: inline-block;
        background-color: white;
        border-radius: 12px;
        position: absolute;
        top: 10px;
        left: 220px;
    }
    #forward{ display: inline-block; position: absolute; top: 0px; left: 110px; }
    #back{ display: inline-block; position: absolute; left: 110px; top: 140px; }
    #jump{ display: inline-block; position: absolute; top: 70px; left: 110px; }
    #right{ display: inline-block; position: absolute; top: 70px; left: 220px; }
    #left{ display: inline-block; position: absolute; top: 70px; left: 0px; }

    /* 新增 PID 区域样式 */
    #pid_fieldset {
        width: 360px;
        margin: 15px auto;
        border: 2px solid cornflowerblue;
        border-radius: 10px;
        padding: 10px;
        background-color: #f9f9f9;
    }
    #pid_fieldset legend {
        font-weight: bold;
        color: cornflowerblue;
        font-size: 16px;
    }
    #pid_fieldset label {
        display: inline-block;
        width: 30px;
        text-align: right;
        margin-right: 5px;
        font-weight: bold;
    }
    #pid_fieldset input[type="number"] { width: 70px; }
    #pid_fieldset select { width: 160px; height: 30px; }
    .pid-button {
        width: 100px;
        height: 30px;
        margin: 5px;
        border-radius: 8px;
        border: 1px solid cornflowerblue;
        background: white;
        color: cornflowerblue;
        font-weight: bold;
        cursor: pointer;
    }
    .pid-button:hover { background: cornflowerblue; color: white; }

    /* Switch开关样式 */
    input[type='checkbox'].switch{
        outline: none;
        appearance: none;
        -webkit-appearance: none;
        -moz-appearance: none;
        position: relative;
        width: 40px;
        height: 20px;
        background: #ccc;
        border-radius: 10px;
        transition: border-color .3s, background-color .3s;
        margin: 0px 20px 0px 0px;
    }
    input[type='checkbox'].switch::after {
        content: '';
        display: inline-block;
        width: 1rem;
        height:1rem;
        border-radius: 50%;
        background: #fff;
        box-shadow: 0, 0, 2px, #999;
        transition:.4s;
        top: 2px;
        position: absolute;
        left: 2px;
    }
    input[type='checkbox'].switch:checked { background: rgb(78, 78, 240); }
    input[type='checkbox'].switch:checked::after { left: 55%; top: 2px; }
    *{
        -webkit-touch-callout:none; 
        -webkit-user-select:none; 
        -khtml-user-select:none; 
        -moz-user-select:none;
        -ms-user-select:none; 
        user-select:none;
    }
    
    /*摇杆内容*/
    .row { display: inline-flex; clear: both; }
    .columnLateral { float: left; width: 15%; min-width: 300px; }
    #joystick { border: 0px solid #FF0000; }
    .dimSlide { transform: scaleX(2) rotate(0deg); width: 200px; position: relative}
        
  </style>
</head>

<!-- onload 事件在页面载入完成后立即触发 -->
<body onload="javascript:socket_init()">
    <h2>WL-PRO WiFi遥控模式</h2>
    <div class="view2">
        <input type="checkbox" id="stable" class="switch" onclick="is_stable()" style="vertical-align:middle">Robot Go!</input>
    </div>
    <center>
        <div class="row">
           <div class="columnLateral">
            <div id="joy1Div" style="width:200px;height:200px;margin:10px"></div>
          </div>
        </div>
    </center>
    
    <div class="sliders">
        <div>
            <input type="range" min="32" max="85" value="38" id="hSlider" oninput="setHeight()" />
            <label class="sliderLabel" for="hSlider" id="hLabel">BaseHeight: 38mm</label>
        </div>
        <div>
            <input type="range" min="-30" max="30" value="0" id="rollSlider" oninput="setroll()" />
            <label class="sliderLabel" for="rollSlider" id="rollLabel">Roll: 0°</label>
        </div>
        <div>
            <input type="range" min="-200" max="200" value="0" id="linearSlider" oninput="setLinear()" />
            <label class="sliderLabel" for="linearSlider" id="linearLabel">LinearVel: 0mm/s</label>
        </div>
        <div>
            <input type="range" min="-100" max="100" value="0" id="angularSlider" oninput="setAngular()" />
            <label class="sliderLabel" for="angularSlider" id="angularLabel">AngularVel: 0°/s</label>
        </div>
    </div>

    <!-- ========== 新增：PID 实时调参区域 ========== -->
    <fieldset id="pid_fieldset">
        <legend>PID 实时调参</legend>
        <div style="margin-bottom:8px;">
            <label style="width:60px">控制器:</label>
            <select id="cmdSelect">
                <option value="A">pid_angle</option>
                <option value="B">pid_gyro</option>
                <option value="C">pid_distance</option>
                <!-- 修复：HTML注释不能用// -->
                <!-- <option value="D">pid_speed</option> -->
                <option value="E">pid_yaw_angle</option>
                <option value="F">pid_yaw_gyro</option>
                <option value="H">pid_lqr_u</option>
                <option value="I">pid_zeropoint</option>
                <option value="K">pid_roll_angle</option>
            </select>
        </div>
        <div style="margin-bottom:5px;">
            <label>P:</label><input type="number" id="pVal" step="0.01" value="1.0">
            <label>I:</label><input type="number" id="iVal" step="0.01" value="0.0">
            <label>D:</label><input type="number" id="dVal" step="0.01" value="0.0">
        </div>
        <button class="pid-button" onclick="sendPID()">发送 PID</button>

        <hr style="width:80%; margin:10px auto;">
        <div style="margin-bottom:5px;">
            <label style="width:60px">滤波器:</label>
            <select id="lpfSelect">
                <option value="G">lpf_joyy</option>
                <option value="J">lpf_zeropoint</option>
                <option value="L">lpf_roll</option>
            </select>
        </div>
        <div style="margin-bottom:5px;">
            <label>Tf:</label><input type="number" id="tfVal" step="0.01" value="0.2">
        </div>
        <button class="pid-button" onclick="sendLPF()">发送 Tf</button>
    </fieldset>
        <!-- ========== 新增：当前参数实时显示区域 ========== -->
        <fieldset id="params_fieldset" style="width:90%; max-width:500px; margin:20px auto; border:2px solid #555; border-radius:10px; background:#f0f0f0;">
        <legend style="color:#333; font-weight:bold;">📊 当前运行参数</legend>
        <button class="pid-button" onclick="refreshParams()" style="width:150px; margin-bottom:10px;">🔄 刷新当前参数</button>
        <div id="params_display" style="width:100%; min-height:150px; overflow-y:auto; background:white; color:#333; font-family:'Courier New', monospace; font-size:12px; padding:8px; border:1px solid #ccc; white-space: pre-wrap;">
        点击上方按钮刷新参数...
        </div>
    </fieldset>
    <!-- ========== 参数显示区域结束 ========== -->
    <!-- ========== PID 区域结束 ========== -->
    
    <div class="buttons">
        <button class="dir" id="forward">Forward</button>
        <button class="dir" id="back">Back</button>
        <button class="dir" id="left">Left</button>
        <button class="dir" id="right">Right</button>
        <button class="dir" id="jump">Jump</button>
    </div>
    
    <!-- 新增：网页串口监视器区域 -->
    <hr style="margin-top:30px;">
    <fieldset id="monitor_fieldset" style="width:90%; max-width:600px; margin:20px auto; border:2px solid #333; border-radius:10px; background:#1e1e1e;">
        <legend style="color:#ccc; font-weight:bold;">📡 实时串口监视器</legend>
        <div id="log_container" style="width:100%; height:200px; overflow-y:scroll; background:black; color:#00ff00; font-family:'Courier New', monospace; font-size:12px; padding:5px; border:1px solid #555;">
        </div>
        <button class="pid-button" onclick="clearLog()" style="margin-top:5px; background:#333; color:#fff; border-color:#666;">清空日志</button>
     </fieldset>

    <!-- ========================================== -->
    <!-- 修复：所有 JavaScript 代码放在这里 -->
    <!-- ========================================== -->
    <script>
      
        // --- 1. 最优先定义：日志显示函数 ---
        function appendLog(message) {
            var logContainer = document.getElementById('log_container');
            if (!logContainer) return;
            
            var line = document.createElement('div');
            if(message.indexOf("ERROR") !== -1) {
                line.style.color = "#ff5555";
            } else if(message.indexOf("DATA:") !== -1) {
                line.style.color = "#55aaff";
            } else {
                line.style.color = "#00ff00";
            }
            line.innerHTML = message.replace(/\n/g, '<br>');
            logContainer.appendChild(line);
            logContainer.scrollTop = logContainer.scrollHeight;
        }

        function clearLog() {
            var c = document.getElementById('log_container');
            if(c) c.innerHTML = "";
        }

        // --- 2. 全局变量 ---
        var socket; 
        var g_roll=0; g_h=38; 
        var g_linear = 0; g_angular = 0; g_stable = 0; 
        var joyX = 0;
        var joyY = 0;

        // --- 3. Socket 初始化 ---
        function socket_init() {
            socket = new WebSocket('ws://' + window.location.hostname + ':81/');
    
            socket.onopen = function(event) {
                console.log("WebSocket Connected!");
                appendLog("--- WebSocket Connected ---");
            };

            socket.onmessage = function(event) {
                console.log("Received:", event.data);
                appendLog(event.data);
            };

            socket.onclose = function(event) {
                console.log("WebSocket Disconnected");
                appendLog("--- Disconnected ---");
            };
        }

        // --- 4. 控制逻辑函数 ---
        function setroll() {
            val = document.getElementById("rollSlider").value;
            val = parseInt(val);
            document.getElementById("rollLabel").innerHTML = "Roll: " + val + "°";
            g_roll = val;
            send_data();
        }  
        function setHeight() {
            val = document.getElementById("hSlider").value;
            val = parseInt(val);
            document.getElementById("hLabel").innerHTML = "BaseHeight: " + val + "mm";
            g_h = val;
            send_data();
        }    
        function setLinear() {
            val = document.getElementById("linearSlider").value;
            val = parseInt(val);
            document.getElementById("linearLabel").innerHTML = "LinearVel: " + val + "mm/s";
            g_linear = val;
            send_data();
        }
        function setAngular() {
            val = document.getElementById("angularSlider").value;
            val = parseInt(val);
            document.getElementById("angularLabel").innerHTML = "AngularVel: " + val + "°/s";
            g_angular = val;
            send_data();
        }
        
        function send_data() {
            var data = {'roll':g_roll,'height':g_h,
                        'linear':g_linear,'angular':g_angular,'stable':g_stable,
                        'mode':'basic','dir':"stop",
                        'joy_y':joyY,'joy_x':joyX,};
            if(socket && socket.readyState === WebSocket.OPEN) {
                socket.send(JSON.stringify(data));
            }
        }
        
        function is_stable() {
            var obj = document.getElementById("stable");
            if(obj.checked) {
                g_stable = 1;
            } else {
                g_stable = 0;
            }
            send_data();
        }

        // --- 5. PID 发送函数 ---
        function sendPID() {
           var cmd = document.getElementById('cmdSelect').value;
           var P = parseFloat(document.getElementById('pVal').value) || 0;
           var I = parseFloat(document.getElementById('iVal').value) || 0;
           var D = parseFloat(document.getElementById('dVal').value) || 0;
           var msg = {mode: "pid", cmd: cmd, P: P, I: I, D: D};
           if(socket && socket.readyState === WebSocket.OPEN) {
               socket.send(JSON.stringify(msg));
           }
        }

        function sendLPF() {
           var cmd = document.getElementById('lpfSelect').value;
           var Tf = parseFloat(document.getElementById('tfVal').value) || 0;
           var msg = {mode: "pid", cmd: cmd, Tf: Tf};
           if(socket && socket.readyState === WebSocket.OPEN) {
               socket.send(JSON.stringify(msg));
           }
        }

        // --- 6. 按钮逻辑 ---
        var buttons = document.getElementsByClassName("dir");
        for(i=0;i<buttons.length;i++) {
            buttons[i].addEventListener("mousedown",move,true);
            buttons[i].addEventListener("mouseup",stop,true);
            buttons[i].addEventListener("touchstart",move,true);
            buttons[i].addEventListener("touchend",stop,true);
        }
        
        function move() {
            this.style = "background-color: cornflowerblue; color: white;";
            var data = {'dir':this.id,'mode':'basic',
                        'roll':g_roll,'height':g_h,
                        'linear':g_linear,'angular':g_angular,'stable':g_stable,
                        'joy_x':joyX,'joy_y':joyY,};
            if(socket && socket.readyState === WebSocket.OPEN) {
                socket.send(JSON.stringify(data));
            }
        }
        
        function stop() {
            this.style = "background-color: white; color: cornflowerblue;";
            var data = {'dir':"stop",'mode':'basic',
                        'roll':g_roll,'height':g_h,
                        'linear':g_linear,'angular':g_angular,'stable':g_stable,
                        'joy_x':joyX,'joy_y':joyY,};
            if(socket && socket.readyState === WebSocket.OPEN) {
                socket.send(JSON.stringify(data));
            }
        }
        
        // --- 7. 摇杆类定义 ---
        var JoyStick = (function(container, parameters) {
            parameters = parameters || {};
            var title = (typeof parameters.title === "undefined" ? "joystick" : parameters.title),
                width = (typeof parameters.width === "undefined" ? 0 : parameters.width),
                height = (typeof parameters.height === "undefined" ? 0 : parameters.height),
                internalFillColor = (typeof parameters.internalFillColor === "undefined" ? "#00979C" : parameters.internalFillColor),
                internalLineWidth = (typeof parameters.internalLineWidth === "undefined" ? 2 : parameters.internalLineWidth),
                internalStrokeColor = (typeof parameters.internalStrokeColor === "undefined" ? "#00979C" : parameters.internalStrokeColor),
                externalLineWidth = (typeof parameters.externalLineWidth === "undefined" ? 2 : parameters.externalLineWidth),
                externalStrokeColor = (typeof parameters.externalStrokeColor ===  "undefined" ? "#0097BC" : parameters.externalStrokeColor),
                autoReturnToCenter = (typeof parameters.autoReturnToCenter === "undefined" ? true : parameters.autoReturnToCenter);
            
            var objContainer = document.getElementById(container);
            var canvas = document.createElement("canvas");
            canvas.id = title;
            if(width === 0) { width = objContainer.clientWidth; }
            if(height === 0) { height = objContainer.clientHeight; }
            canvas.width = width;
            canvas.height = height;
            objContainer.appendChild(canvas);
            var context=canvas.getContext("2d");
            
            var isPressing = 0;
            var isMoving = 0;
            var isRelease = 0;
            var circumference = 2 * Math.PI;
            var internalRadius = (canvas.width-((canvas.width/2)+10))/2;
            var maxMoveStick = internalRadius + 5;
            var externalRadius = internalRadius + 30;
            var centerX = canvas.width / 2;
            var centerY = canvas.height / 2;
            var movedX = centerX;
            var movedY = centerY;
            
            if("ontouchstart" in document.documentElement) {
                canvas.addEventListener("touchstart", onTouchStart, true);
                canvas.addEventListener("touchmove", onTouchMove, true);
                document.addEventListener("touchend", onTouchEnd, true);
            } else {
                canvas.addEventListener("mousedown", onMouseDown, true);
                canvas.addEventListener("mousemove", onMouseMove, true);
                document.addEventListener("mouseup", onMouseUp, true);
            }
            
            drawExternal();
            drawInternal();

            function drawExternal() {
                context.beginPath();
                context.arc(centerX, centerY, externalRadius, 0, circumference, false);
                context.lineWidth = externalLineWidth;
                context.strokeStyle = externalStrokeColor;
                context.stroke();
            }

            function drawInternal() {
                context.beginPath();
                if(movedX<internalRadius) { movedX=maxMoveStick; }
                if((movedX+internalRadius) > canvas.width) { movedX = canvas.width-(maxMoveStick); }
                if(movedY<internalRadius) { movedY=maxMoveStick; }
                if((movedY+internalRadius) > canvas.height) { movedY = canvas.height-(maxMoveStick); }
                context.arc(movedX, movedY, internalRadius, 0, circumference, false);
                var grd = context.createRadialGradient(centerX, centerY, 5, centerX, centerY, 200);
                grd.addColorStop(0, internalFillColor);
                grd.addColorStop(1, internalStrokeColor);
                context.fillStyle = grd;
                context.fill();
                context.lineWidth = internalLineWidth;
                context.strokeStyle = internalStrokeColor;
                context.stroke();
            }

            function postCoordinate() { 
                joyX = (100*((movedX - centerX)/maxMoveStick)).toFixed();
                joyY = ((100*((movedY - centerY)/maxMoveStick))*-1).toFixed();
                send_data();
            }

            function releaseControl() { 
                joyX = 0;
                joyY = 0;
                send_data();
            }  
            
            function onTouchStart(event) {
                isPressing = 1;
                isMoving = 0;
                isRelease = 0;
            }

            function onTouchMove(event) {
                event.preventDefault();
                if(isPressing === 1 && event.targetTouches[0].target === canvas) {
                    isMoving = 1;
                    isRelease = 0;
                    movedX = event.targetTouches[0].pageX;
                    movedY = event.targetTouches[0].pageY;
                    if(canvas.offsetParent.tagName.toUpperCase() === "BODY") {
                        movedX -= canvas.offsetLeft;
                        movedY -= canvas.offsetTop;
                    } else {
                        movedX -= canvas.offsetParent.offsetLeft;
                        movedY -= canvas.offsetParent.offsetTop;
                    }
                    context.clearRect(0, 0, canvas.width, canvas.height);
                    drawExternal();
                    drawInternal();
                    postCoordinate();
                }
            } 

            function onTouchEnd(event) {
                isPressing = 0;
                isMoving = 0;
                isRelease = 1;
                if(autoReturnToCenter) {
                    movedX = centerX;
                    movedY = centerY;
                }
                context.clearRect(0, 0, canvas.width, canvas.height);
                drawExternal();
                drawInternal();
                releaseControl();
            }
    
            function onMouseDown(event) {
                isPressing = 1;
                isMoving = 0;
                isRelease = 0;
            }

            function onMouseMove(event) {
                if(isPressing === 1) {
                    isMoving = 1;
                    isRelease = 0;
                    movedX = event.pageX;
                    movedY = event.pageY;
                    if(canvas.offsetParent.tagName.toUpperCase() === "BODY") {
                        movedX -= canvas.offsetLeft;
                        movedY -= canvas.offsetTop;
                    } else {
                        movedX -= canvas.offsetParent.offsetLeft;
                        movedY -= canvas.offsetParent.offsetTop;
                    }
                    context.clearRect(0, 0, canvas.width, canvas.height);
                    drawExternal();
                    drawInternal();
                    postCoordinate();
                }
            }

            function onMouseUp(event) {
                isPressing = 0;
                isMoving = 0;
                isRelease = 1;
                if(autoReturnToCenter) {
                    movedX = centerX;
                    movedY = centerY;
                }
                context.clearRect(0, 0, canvas.width, canvas.height);
                drawExternal();
                drawInternal();
                releaseControl();
            }

            this.GetX = function () {
                return (100*((movedX - centerX)/maxMoveStick)).toFixed();
            };
            this.GetY = function () {
                return ((100*((movedY - centerY)/maxMoveStick))*-1).toFixed();
            };
        });
                // --- 新增：刷新参数函数 ---
        function refreshParams() {
           var msg = {mode: "get_params"};
           if(socket && socket.readyState === WebSocket.OPEN) {
               socket.send(JSON.stringify(msg));
               document.getElementById('params_display').innerHTML = "正在读取参数...";
           }
        }
        // --- 8. 最后：初始化摇杆 ---
        var joy1Param = { "title": "1" };  
        var Joy1 = new JoyStick('joy1Div', joy1Param);
        
        setInterval(function() { 
            joyX = Joy1.GetX();
            joyY = Joy1.GetY();
            send_data();
        }, 150);

    </script> 
                
</body>
</html>

)=====";