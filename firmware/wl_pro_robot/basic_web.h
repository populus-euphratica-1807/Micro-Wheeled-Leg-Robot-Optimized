const char basic_web[] PROGMEM = R"=====(

<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>WL-PRO 控制中心</title>
  <style>
    :root 
    {
      --primary: #4F6EF7;
      --primary-dark: #3A56D4;
      --danger: #FF4D6A;
      --bg: #F4F6FC;
      --card-bg: #FFFFFF;
      --text: #1E293B;
      --text-light: #64748B;
      --border: #E2E8F0;
      --shadow: 0 4px 12px rgba(0,0,0,0.05);
      --radius: 16px;
    }

    * { box-sizing: border-box; margin: 0; padding: 0; }
    body 
    {
      font-family: 'Segoe UI', 'PingFang SC', 'Microsoft YaHei', sans-serif;
      background: var(--bg);
      color: var(--text);
      min-height: 100vh;
      display: flex;
      justify-content: center;
      padding: 20px;
      user-select: none;
    }

    .container 
    {
      width: 100%;
      max-width: 1100px;
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 20px;
      align-items: start;
    }

    .header 
    {
      grid-column: 1 / -1;
      background: linear-gradient(135deg, var(--primary), #7B8FF7);
      color: white;
      padding: 24px 32px;
      border-radius: var(--radius);
      text-align: center;
      box-shadow: 0 8px 20px rgba(79,110,247,0.25);
    }
    .header h2 { font-size: 1.8rem; font-weight: 700; }
    .header .subtitle { font-size: 0.9rem; opacity: 0.9; margin-top: 4px; }

    .card 
    {
      background: var(--card-bg);
      border-radius: var(--radius);
      padding: 20px;
      box-shadow: var(--shadow);
      border: 1px solid var(--border);
    }
    .card-title 
    {
      font-size: 0.95rem;
      font-weight: 600;
      color: var(--text-light);
      margin-bottom: 15px;
      display: flex;
      align-items: center;
      gap: 6px;
    }

    .full-width 
    {
      grid-column: 1 / -1;
    }

    /* 开关 */
    .switch-container 
    {
      display: flex;
      align-items: center;
      justify-content: center;
      gap: 12px;
      font-weight: 600;
      font-size: 1.1rem;
    }
    input[type='checkbox'].switch 
    {
      appearance: none;
      width: 52px;
      height: 28px;
      background: #CBD5E1;
      border-radius: 28px;
      position: relative;
      cursor: pointer;
      transition: background 0.25s;
    }
    input[type='checkbox'].switch::after 
    {
      content: '';
      width: 22px; height: 22px;
      border-radius: 50%;
      background: white;
      position: absolute;
      top: 3px; left: 3px;
      box-shadow: 0 2px 6px rgba(0,0,0,0.15);
      transition: 0.25s;
    }
    input[type='checkbox'].switch:checked { background: var(--primary); }
    input[type='checkbox'].switch:checked::after { left: 27px; }

    /* 摇杆 */
    #joy1Div 
    {
      width: 200px;
      height: 200px;
      margin: 10px auto;
      border-radius: 50%;
      background: #F1F5F9;
      box-shadow: inset 0 2px 8px rgba(0,0,0,0.06);
      border: 1px solid var(--border);
    }

    /* 滑块 */
    .slider-group { margin-bottom: 14px; }
    .slider-header 
    {
      display: flex;
      justify-content: space-between;
      font-size: 0.85rem;
      color: var(--text-light);
      margin-bottom: 6px;
    }
    input[type=range] 
    {
      width: 100%;
      height: 6px;
      -webkit-appearance: none;
      background: linear-gradient(to right, var(--primary), #A5B4FC);
      border-radius: 6px;
      outline: none;
    }
    input[type=range]::-webkit-slider-thumb 
    {
      -webkit-appearance: none;
      width: 22px; height: 22px;
      border-radius: 50%;
      background: white;
      border: 2px solid var(--primary);
      box-shadow: 0 2px 8px rgba(79,110,247,0.3);
      cursor: pointer;
    }

    /* 方向按钮 */
    .dir-buttons 
    {
      display: grid;
      grid-template-columns: 1fr 1fr 1fr;
      gap: 10px;
      justify-items: center;
      width: 240px;
      margin: 0 auto;
    }
    .dir-btn 
    {
      width: 70px; height: 70px;
      border-radius: 20px;
      background: white;
      border: 2px solid var(--border);
      font-weight: 600; font-size: 0.85rem;
      color: var(--text);
      cursor: pointer;
      display: flex;
      align-items: center;
      justify-content: center;
      transition: 0.2s;
      box-shadow: 0 2px 6px rgba(0,0,0,0.04);
    }
    .dir-btn.pressed 
    {
      background: var(--primary);
      color: white;
      border-color: var(--primary);
      transform: scale(0.95);
    }
    #forward { grid-column: 2; grid-row: 1; }
    #left    { grid-column: 1; grid-row: 2; }
    #jump    { grid-column: 2; grid-row: 2; background: #FEF9C3; border-color: #FACC15; }
    #right   { grid-column: 3; grid-row: 2; }
    #back    { grid-column: 2; grid-row: 3; }

    /* PID 区域 */
    .pid-row 
    {
      display: flex;
      align-items: center;
      gap: 8px;
      margin-bottom: 12px;
      flex-wrap: wrap;
    }
    .pid-select 
    {
      padding: 8px 12px;
      border-radius: 10px;
      border: 1px solid var(--border);
      background: white;
      font-size: 0.85rem;
      flex: 1;
      min-width: 140px;
    }
    .pid-input 
    {
      width: 70px;
      padding: 8px;
      border-radius: 10px;
      border: 1px solid var(--border);
      text-align: center;
      font-size: 0.85rem;
    }
    .btn 
    {
      padding: 8px 16px;
      border-radius: 12px;
      background: var(--primary);
      color: white;
      border: none;
      font-weight: 600;
      font-size: 0.85rem;
      cursor: pointer;
      transition: 0.25s;
      box-shadow: 0 4px 10px rgba(79,110,247,0.25);
    }
    .btn:hover { background: var(--primary-dark); }
    .btn-outline 
    {
      background: white;
      color: var(--primary);
      border: 1.5px solid var(--primary);
      box-shadow: none;
    }
    .btn-outline:hover { background: #EEF2FF; }

    /* 日志 */
    .log-box 
    {
      background: #1E1E2E;
      color: #A6E3A1;
      font-family: 'Courier New', monospace;
      font-size: 0.8rem;
      border-radius: 12px;
      padding: 12px;
      height: 200px;
      overflow-y: auto;
      line-height: 1.5;
      border: 1px solid #313244;
      margin-bottom: 10px;
    }
    .log-error { color: #F38BA8; }
    .log-data  { color: #89B4FA; }

    .flex-center 
    {
      display: flex; justify-content: center; gap: 8px; flex-wrap: wrap;
    }

    hr { border: 0.5px solid var(--border); margin: 16px 0; }

    /* 响应式：窄屏变单列 */
    @media (max-width: 800px) 
    {
      .container 
      {
        grid-template-columns: 1fr;
        max-width: 100%;
      }
      .header h2 { font-size: 1.5rem; }
      .dir-buttons { width: 200px; }
      .dir-btn { width: 60px; height: 60px; font-size: 0.8rem; }
      .pid-input { width: 55px; }
    }
  </style>
</head>
<body onload="javascript:socket_init()">
<div class="container">

  <!-- 头部 -->
  <div class="header">
    <h2>🤖 WL-PRO 控制中心</h2>
    <div class="subtitle">WiFi 遥控 · 双轮足平衡</div>
  </div>

  <!-- 启动开关（跨两列） -->
  <div class="card full-width" style="text-align:center;">
    <div class="switch-container">
      <span>🔴 待命</span>
      <input type="checkbox" id="stable" class="switch" onclick="is_stable()">
      <span style="color:var(--primary);">🟢 启动</span>
    </div>
    <div style="font-size:0.85rem; color:var(--text-light); margin-top:6px;" id="statusText">机器人已锁定</div>
  </div>

  <!-- 左列：摇杆 + 方向键 -->
  <div class="card">
    <div class="card-title">🎮 运动摇杆</div>
    <div id="joy1Div"></div>
  </div>

  <div class="card">
    <div class="card-title">🕹️ 方向快捷键</div>
    <div class="dir-buttons">
      <button class="dir-btn" id="forward">▲<br>前</button>
      <button class="dir-btn" id="left">◀<br>左</button>
      <button class="dir-btn" id="jump">Jump</button>
      <button class="dir-btn" id="right">▶<br>右</button>
      <button class="dir-btn" id="back">▼<br>后</button>
    </div>
  </div>

  <!-- 右列：滑块 + PID -->
  <div class="card">
    <div class="card-title">📐 运动参数</div>
    <div class="slider-group">
      <div class="slider-header"><span>📏 机身高度</span><span id="hLabel">38 mm</span></div>
      <input type="range" min="32" max="85" value="38" id="hSlider" oninput="setHeight()">
    </div>
    <div class="slider-group">
      <div class="slider-header"><span>↔️ 横滚设定</span><span id="rollLabel">0°</span></div>
      <input type="range" min="-30" max="30" value="0" id="rollSlider" oninput="setroll()">
    </div>
    <div class="slider-group">
      <div class="slider-header"><span>🚀 线速度</span><span id="linearLabel">0 mm/s</span></div>
      <input type="range" min="-200" max="200" value="0" id="linearSlider" oninput="setLinear()">
    </div>
    <div class="slider-group">
      <div class="slider-header"><span>🔄 角速度</span><span id="angularLabel">0 °/s</span></div>
      <input type="range" min="-100" max="100" value="0" id="angularSlider" oninput="setAngular()">
    </div>
  </div>

  <div class="card">
    <div class="card-title">📊 PID 实时调参</div>
    <div class="pid-row">
      <select id="cmdSelect" class="pid-select">
        <option value="A">pid_angle</option>
        <option value="B">pid_gyro</option>
        <option value="C">pid_distance</option>
        <option value="E">pid_yaw_angle</option>
        <option value="F">pid_yaw_gyro</option>
        <option value="H">pid_lqr_u</option>
        <option value="I">pid_zeropoint</option>
        <option value="K">pid_roll_angle</option>
      </select>
    </div>
    <div class="pid-row">
      <span>P</span><input type="number" id="pVal" class="pid-input" step="0.01" value="1.0">
      <span>I</span><input type="number" id="iVal" class="pid-input" step="0.01" value="0.0">
      <span>D</span><input type="number" id="dVal" class="pid-input" step="0.01" value="0.0">
      <button class="btn" onclick="sendPID()">应用</button>
    </div>
    <hr>
    <div class="pid-row">
      <select id="lpfSelect" class="pid-select">
        <option value="G">lpf_joyy</option>
        <option value="J">lpf_zeropoint</option>
        <option value="L">lpf_roll</option>
      </select>
    </div>
    <div class="pid-row">
      <span>Tf</span><input type="number" id="tfVal" class="pid-input" step="0.01" value="0.2">
      <button class="btn btn-outline" onclick="sendLPF()">应用</button>
      <button class="btn btn-outline" onclick="refreshParams()">📋 读取</button>
    </div>
    <div id="params_display" style="font-size:0.8rem; background:#F8FAFC; border-radius:10px; padding:8px; margin-top:8px; color:var(--text-light); max-height:100px; overflow-y:auto;">
      点击“读取”查看当前参数
    </div>
  </div>

  <!-- 底部日志（跨两列） -->
  <div class="card full-width">
    <div class="card-title">📡 串口监视器</div>
    <div class="log-box" id="log_container"></div>
    <div class="flex-center">
      <button class="btn btn-outline" style="padding:6px 16px;" onclick="clearLog()">清空日志</button>
    </div>
  </div>

</div>

<script>
  // ----- 全局变量 -----
  var socket;
  var g_roll=0, g_h=38, g_linear=0, g_angular=0, g_stable=0;
  var joyX=0, joyY=0;

  // 日志
  function appendLog(msg) 
  {
    var c = document.getElementById('log_container');
    if(!c) return;
    var d = document.createElement('div');
    if(msg.indexOf("ERROR")!=-1) d.className='log-error';
    else if(msg.indexOf("DATA:")!=-1) d.className='log-data';
    d.innerHTML = msg.replace(/\n/g,'<br>');
    c.appendChild(d); c.scrollTop = c.scrollHeight;
  }
  function clearLog() { var c=document.getElementById('log_container'); if(c)c.innerHTML=''; }

  // WebSocket
  function socket_init() 
  {
    socket = new WebSocket('ws://'+window.location.hostname+':81/');
    socket.onopen = function(e){ appendLog("✅ 连接成功"); };
    socket.onmessage = function(e){ appendLog(e.data); };
    socket.onclose = function(e){ appendLog("⚠️ 连接断开"); };
  }

  // 滑块
  function setroll() 
  {
    var v=parseInt(document.getElementById("rollSlider").value);
    document.getElementById("rollLabel").innerText=v+"°"; g_roll=v; send_data();
  }
  function setHeight() 
  {
    var v=parseInt(document.getElementById("hSlider").value);
    document.getElementById("hLabel").innerText=v+" mm"; g_h=v; send_data();
  }
  function setLinear() 
  {
    var v=parseInt(document.getElementById("linearSlider").value);
    document.getElementById("linearLabel").innerText=v+" mm/s"; g_linear=v; send_data();
  }
  function setAngular() 
  {
    var v=parseInt(document.getElementById("angularSlider").value);
    document.getElementById("angularLabel").innerText=v+" °/s"; g_angular=v; send_data();
  }

  function send_data() 
  {
    var data = 
    {
      roll:g_roll, height:g_h, linear:g_linear, angular:g_angular,
      stable:g_stable, mode:'basic', dir:'stop', joy_y:joyY, joy_x:joyX
    };
    if(socket && socket.readyState === WebSocket.OPEN) socket.send(JSON.stringify(data));
  }

  // 启动开关
  function is_stable() 
  {
    var cb = document.getElementById("stable");
    g_stable = cb.checked ? 1 : 0;
    document.getElementById("statusText").innerText = g_stable ? "🟢 机器人已启动" : "🔴 机器人已锁定";
    send_data();
  }

  // 方向按钮
  var btns = document.getElementsByClassName("dir-btn");
  for(var i=0; i<btns.length; i++) 
  {
    btns[i].addEventListener("mousedown", move);
    btns[i].addEventListener("mouseup", stop);
    btns[i].addEventListener("touchstart", move);
    btns[i].addEventListener("touchend", stop);
  }
  function move() 
  {
    this.classList.add("pressed");
    var d = { dir:this.id, mode:'basic', roll:g_roll, height:g_h, linear:g_linear, angular:g_angular, stable:g_stable, joy_x:joyX, joy_y:joyY };
    if(socket && socket.readyState===WebSocket.OPEN) socket.send(JSON.stringify(d));
  }
  function stop() 
  {
    this.classList.remove("pressed");
    var d = { dir:'stop', mode:'basic', roll:g_roll, height:g_h, linear:g_linear, angular:g_angular, stable:g_stable, joy_x:joyX, joy_y:joyY };
    if(socket && socket.readyState===WebSocket.OPEN) socket.send(JSON.stringify(d));
  }

  // PID / 滤波器
  function sendPID() 
  {
    var cmd = document.getElementById('cmdSelect').value;
    var P = parseFloat(document.getElementById('pVal').value);
    var I = parseFloat(document.getElementById('iVal').value);
    var D = parseFloat(document.getElementById('dVal').value);
    var msg = {mode:"pid", cmd:cmd, P:P, I:I, D:D};
    if(socket && socket.readyState===WebSocket.OPEN) socket.send(JSON.stringify(msg));
  }
  function sendLPF() 
  {
    var cmd = document.getElementById('lpfSelect').value;
    var Tf = parseFloat(document.getElementById('tfVal').value);
    var msg = {mode:"pid", cmd:cmd, Tf:Tf};
    if(socket && socket.readyState===WebSocket.OPEN) socket.send(JSON.stringify(msg));
  }
  function refreshParams() 
  {
    if(socket && socket.readyState===WebSocket.OPEN) 
    {
      socket.send(JSON.stringify({mode:"get_params"}));
      document.getElementById('params_display').innerText = "读取中...";
    }
  }

  // 摇杆类
  var JoyStick = (function(container, parameters) 
  {
    parameters = parameters || {};
    var title = parameters.title || "joystick",
        width = parameters.width || 0,
        height = parameters.height || 0,
        internalFillColor = parameters.internalFillColor || "#4F6EF7",
        internalLineWidth = parameters.internalLineWidth || 2,
        internalStrokeColor = parameters.internalStrokeColor || "#3A56D4",
        externalLineWidth = parameters.externalLineWidth || 2,
        externalStrokeColor = parameters.externalStrokeColor || "#A5B4FC",
        autoReturnToCenter = (parameters.autoReturnToCenter !== false);
    var objContainer = document.getElementById(container);
    var canvas = document.createElement("canvas");
    canvas.id = title;
    if(width===0) width = objContainer.clientWidth;
    if(height===0) height = objContainer.clientHeight;
    canvas.width = width; canvas.height = height;
    objContainer.appendChild(canvas);
    var context = canvas.getContext("2d");
    var isPressing = false;
    var circumference = 2 * Math.PI;
    var internalRadius = (canvas.width-((canvas.width/2)+10))/2;
    var maxMoveStick = internalRadius+5;
    var externalRadius = internalRadius+30;
    var centerX = canvas.width/2, centerY = canvas.height/2;
    var movedX = centerX, movedY = centerY;

    if("ontouchstart" in document.documentElement) 
    {
      canvas.addEventListener("touchstart", function(e){ isPressing=true; });
      canvas.addEventListener("touchmove", function(e)
      {
        e.preventDefault();
        if(isPressing && e.targetTouches[0].target === canvas) 
        {
          movedX = e.targetTouches[0].pageX;
          movedY = e.targetTouches[0].pageY;
          if(canvas.offsetParent.tagName.toUpperCase()==="BODY") 
          {
            movedX -= canvas.offsetLeft; movedY -= canvas.offsetTop;
          } 
          else 
          {
            movedX -= canvas.offsetParent.offsetLeft;
            movedY -= canvas.offsetParent.offsetTop;
          }
          reDraw(); postCoordinate();
        }
      });
      document.addEventListener("touchend", function(e)
      {
        isPressing=false;
        if(autoReturnToCenter) { movedX=centerX; movedY=centerY; }
        reDraw(); releaseControl();
      });
    } 
    else 
    {
      canvas.addEventListener("mousedown", function(e){ isPressing=true; });
      canvas.addEventListener("mousemove", function(e){
        if(isPressing) 
        {
          movedX = e.pageX; movedY = e.pageY;
          if(canvas.offsetParent.tagName.toUpperCase()==="BODY") 
          {
            movedX -= canvas.offsetLeft; movedY -= canvas.offsetTop;
          } 
          else 
          {
            movedX -= canvas.offsetParent.offsetLeft;
            movedY -= canvas.offsetParent.offsetTop;
          }
          reDraw(); postCoordinate();
        }
      });
      document.addEventListener("mouseup", function(e)
      {
        isPressing=false;
        if(autoReturnToCenter) { movedX=centerX; movedY=centerY; }
        reDraw(); releaseControl();
      });
    }

    function drawExternal() 
    {
      context.beginPath();
      context.arc(centerX, centerY, externalRadius, 0, circumference, false);
      context.lineWidth = externalLineWidth;
      context.strokeStyle = externalStrokeColor;
      context.stroke();
    }
    function drawInternal() 
    {
      context.beginPath();
      if(movedX<internalRadius) movedX=maxMoveStick;
      if(movedX+internalRadius>canvas.width) movedX=canvas.width-maxMoveStick;
      if(movedY<internalRadius) movedY=maxMoveStick;
      if(movedY+internalRadius>canvas.height) movedY=canvas.height-maxMoveStick;
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
    function reDraw() 
    {
      context.clearRect(0,0,canvas.width,canvas.height);
      drawExternal(); drawInternal();
    }
    function postCoordinate() 
    {
      joyX = (100*((movedX-centerX)/maxMoveStick)).toFixed();
      joyY = ((100*((movedY-centerY)/maxMoveStick))*-1).toFixed();
      send_data();
    }
    function releaseControl() { joyX=0; joyY=0; send_data(); }
    this.GetX = function(){ return ((100*((movedX-centerX)/maxMoveStick)).toFixed()); };
    this.GetY = function(){ return (((100*((movedY-centerY)/maxMoveStick))*-1).toFixed()); };
  });

  // 初始化摇杆
  var joy1Param = { title: "1", internalFillColor: "#4F6EF7", internalStrokeColor: "#3A56D4", externalStrokeColor: "#A5B4FC" };
  var Joy1 = new JoyStick('joy1Div', joy1Param);

  // 周期性读取摇杆
  setInterval(function() 
  {
    joyX = Joy1.GetX();
    joyY = Joy1.GetY();
    send_data();
  }, 150);
</script>
</body>
</html>

)=====";