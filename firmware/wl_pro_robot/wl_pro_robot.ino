// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// 双轮足机器人平衡控制系统
// 原始代码版权 (c) 2024 Mu Shibo (https://github.com/MuShibo/Micro-Wheeled_leg-Robot)
// 优化 (c) 2026 Populus
//
// 本软件基于 MIT 许可发布，详见项目根目录 LICENSE 文件
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------

//机器人控制头文件
#include <MPU6050_tockn.h>
#include "Servo_STS3032.h"
#include <SimpleFOC.h>
#include <Arduino.h>

//wifi控制数据传输头文件
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <WiFi.h>
#include <FS.h>
#include "basic_web.h"
#include "robot.h"
#include "wifi.h"
#include <esp_adc_cal.h>

// UDP 通信头文件
#include <WiFiUdp.h>

#define PI 3.14159265f

/************实例定义*************/

//电机实例
BLDCMotor motor1 = BLDCMotor(7);
BLDCMotor motor2 = BLDCMotor(7);
BLDCDriver3PWM driver1 = BLDCDriver3PWM(32,33,25,22);
BLDCDriver3PWM driver2  = BLDCDriver3PWM(26,27,14,12);

//编码器实例
TwoWire I2Cone = TwoWire(0);
TwoWire I2Ctwo = TwoWire(1);
MagneticSensorI2C sensor1 = MagneticSensorI2C(AS5600_I2C);
MagneticSensorI2C sensor2 = MagneticSensorI2C(AS5600_I2C);

//PID控制器实例
PIDController pid_angle     {.P = 1,    .I = 0,   .D = 0, .ramp = 100000, .limit = 8};
PIDController pid_gyro      {.P = 0.06, .I = 0,   .D = 0, .ramp = 100000, .limit = 8};
PIDController pid_distance  {.P = 0.5,  .I = 0,   .D = 0, .ramp = 100000, .limit = 8};
PIDController pid_speed     {.P = 0.7,  .I = 0,   .D = 0, .ramp = 100000, .limit = 8};
PIDController pid_yaw_angle {.P = 1.0,  .I = 0,   .D = 0, .ramp = 100000, .limit = 8};
PIDController pid_yaw_gyro  {.P = 0.04, .I = 0,   .D = 0, .ramp = 100000, .limit = 8};
PIDController pid_lqr_u     {.P = 1,    .I = 15,  .D = 0, .ramp = 100000, .limit = 8};
PIDController pid_zeropoint {.P = 0.002,.I = 0,   .D = 0, .ramp = 100000, .limit = 4};
PIDController pid_roll_angle{.P = 8,    .I = 0,   .D = 0, .ramp = 100000, .limit = 450};

//低通滤波器实例
LowPassFilter lpf_joyy{.Tf = 0.2};
LowPassFilter lpf_zeropoint{.Tf = 0.1};
LowPassFilter lpf_roll{.Tf = 0.3};

float joyy_filtered_global = 0;   


// 扰动相关
float perturb_offset = 0.0f;          // 当前角度偏移（度）
unsigned long perturb_end_time = 0;   // 扰动结束时刻（毫秒）

// commander通信实例
Commander command = Commander(Serial);

void StabAngle(char* cmd)     { command.pid(&pid_angle, cmd);     }
void StabGyro(char* cmd)      { command.pid(&pid_gyro, cmd);      }
void StabDistance(char* cmd)  { command.pid(&pid_distance, cmd);  }
void StabSpeed(char* cmd)     { command.pid(&pid_speed, cmd);     }
void StabYawAngle(char* cmd)  { command.pid(&pid_yaw_angle, cmd); }
void StabYawGyro(char* cmd)   { command.pid(&pid_yaw_gyro, cmd);  }
void lpfJoyy(char* cmd)       { command.lpf(&lpf_joyy, cmd);      }
void StabLqrU(char* cmd)      { command.pid(&pid_lqr_u, cmd);     }
void StabZeropoint(char* cmd) { command.pid(&pid_zeropoint, cmd); }
void lpfZeropoint(char* cmd)  { command.lpf(&lpf_zeropoint, cmd); }
void StabRollAngle(char* cmd) { command.pid(&pid_roll_angle, cmd);}
void lpfRoll(char* cmd)       { command.lpf(&lpf_roll, cmd);      }

//WebServer实例
WebServer webserver;
WebSocketsServer websocket = WebSocketsServer(81);
RobotProtocol rp(20);
int joystick_value[2];

//STS舵机实例
SMS_STS sms_sts;

//MPU6050实例
MPU6050 mpu6050(I2Ctwo);

// UDP 实例与设置
WiFiUDP udp;
#define UDP_TARGET_IP    "192.168.1.100"
#define UDP_TARGET_PORT  12345

// FreeRTOS 任务句柄
TaskHandle_t TaskBalance_Handle = NULL;
TaskHandle_t TaskComm_Handle = NULL;

/************参数定义*************/
#define pi 3.1415927

//LQR自平衡控制器参数
float LQR_angle = 0;
float LQR_gyro  = 0;
float LQR_speed = 0;
float LQR_distance = 0;
float angle_control   = 0;
float gyro_control    = 0;
float speed_control   = 0;
float distance_control = 0;
float LQR_u = 0;
float angle_zeropoint = 2.45;
float distance_zeropoint = -256.0;

//YAW轴控制数据
float YAW_gyro = 0;
float YAW_angle = 0;
float YAW_angle_last = 0;
float YAW_angle_total = 0;
float YAW_angle_zero_point = -10;
float YAW_output = 0;

//腿部舵机控制数据
byte ID[2];
s16 Position[2];
u16 Speed[2];
byte ACC[2];

//逻辑处理标志位
float robot_speed = 0;
float robot_speed_last = 0;
int wrobot_move_stop_flag = 0;
int jump_flag = 0;
float leg_position_add = 0;
int uncontrolable = 0;

//电压检测
uint16_t bat_check_num = 0;
int BAT_PIN = 35;
static esp_adc_cal_characteristics_t adc_chars;
static const adc1_channel_t channel = ADC1_CHANNEL_7;     
static const adc_bits_width_t width = ADC_WIDTH_BIT_12;
static const adc_atten_t atten = ADC_ATTEN_DB_11;
static const adc_unit_t unit = ADC_UNIT_1;
float battery_voltage = 8.4;   

//电量显示LED
#define LED_BAT 13
bool battery_dead = false;

// 手动 PID 覆盖标志 (网页端调参用)
bool manual_pid_speed_override = false;

// ----- 多核线程安全数据发送结构体与互斥锁 -----
portMUX_TYPE dataMux = portMUX_INITIALIZER_UNLOCKED;

struct RobotStateSnapshot 
{
  unsigned long timestamp;
  float angle, gyro, speed, dist;
  float LQR_u;
  float yawout, yaw;
  float joyx, joyy;
  float joyy_filtered;   
  float height;
  float angle_zp, dist_zp;
  float leg_add, bat;
  float motor1_target, motor2_target;
  int unctrl, jump;
} snapshot;

// 基础发送函数
void sendToWeb(const String &msg) 
{
  if(websocket.connectedClients() > 0) 
  {
    String tempMsg = msg; 
    websocket.broadcastTXT(tempMsg);
  }
}

void SerialAndWeb(const String &msg) 
{
  Serial.print(msg);
  sendToWeb(msg);
}

void SerialAndWebLn(const String &msg) 
{
  Serial.println(msg);
  sendToWeb(msg + "\n");
}

// ------------------ UDP 状态发送 ------------------
void sendStatusUDP() 
{
  // 原子性地读取快照
  RobotStateSnapshot snap;
  portENTER_CRITICAL(&dataMux);
  snap = snapshot;
  portEXIT_CRITICAL(&dataMux);

  StaticJsonDocument<1024> doc;
  doc["t"]         = snap.timestamp;
  doc["angle"]     = snap.angle;
  doc["gyro"]      = snap.gyro;
  doc["speed"]     = snap.speed;
  doc["dist"]      = snap.dist;
  doc["LQR_u"]     = snap.LQR_u;
  doc["yawout"]    = snap.yawout;
  doc["yaw"]       = snap.yaw;
  doc["joyx"]      = snap.joyx;
  doc["joyy"]      = snap.joyy;
  doc["joyy_f"]    = snap.joyy_filtered;
  doc["height"]    = snap.height;
  doc["angle_zp"]  = snap.angle_zp;
  doc["dist_zp"]   = snap.dist_zp;
  doc["leg_add"]   = snap.leg_add;
  doc["bat"]       = snap.bat;
  doc["motor1_t"]  = snap.motor1_target;
  doc["motor2_t"]  = snap.motor2_target;
  doc["unctrl"]    = snap.unctrl;
  doc["jump"]      = snap.jump;

  String jsonStr;
  serializeJson(doc, jsonStr);

  udp.beginPacket(UDP_TARGET_IP, UDP_TARGET_PORT);
  udp.print(jsonStr);
  udp.endPacket();
}

// ------------------ FreeRTOS 任务函数 ------------------
void TaskBalance(void *pvParameters) 
{
  while (1) 
  {
    bat_check();
    if (battery_dead) 
    {
      motor1.target = 0;
      motor2.target = 0;
      vTaskDelay(10 / portTICK_PERIOD_MS);
      continue;
    }

    mpu6050.update();
    lqr_balance_loop();
    yaw_loop();
    leg_loop();

    // 将自平衡计算输出转矩赋给电机
    motor1.target = (-0.5) * (LQR_u + YAW_output);
    motor2.target = (-0.5) * (LQR_u - YAW_output);

    // 倒地失控后关闭输出
    if (abs(LQR_angle) > 25.0f) 
    {
      uncontrolable = 1;
    }
    if (uncontrolable != 0) 
    {
      if (abs(LQR_angle) < 10.0f) uncontrolable++;
      if (uncontrolable > 200) uncontrolable = 0;
    }

    if (wrobot.go == 0 || uncontrolable != 0) 
    {
      motor1.target = 0;
      motor2.target = 0;
      leg_position_add = 0;
    }

    // 记录上一次的遥控器数据
    wrobot.dir_last  = wrobot.dir;
    wrobot.joyx_last = wrobot.joyx;
    wrobot.joyy_last = wrobot.joyy;

    // 迭代计算FOC相电压
    motor1.loopFOC();
    motor2.loopFOC();
    motor1.move();
    motor2.move();

    // ---- 原子性保存当前状态快照 ----
    portENTER_CRITICAL(&dataMux);
    snapshot.timestamp      = millis();
    snapshot.angle          = LQR_angle;
    snapshot.gyro           = LQR_gyro;
    snapshot.speed          = LQR_speed;
    snapshot.dist           = LQR_distance;
    snapshot.LQR_u          = LQR_u;
    snapshot.yawout         = YAW_output;
    snapshot.yaw            = YAW_angle_total;
    snapshot.joyx           = wrobot.joyx;
    snapshot.joyy           = wrobot.joyy;
    snapshot.joyy_filtered = joyy_filtered_global;

    snapshot.height         = wrobot.height;
    snapshot.angle_zp       = angle_zeropoint;
    snapshot.dist_zp        = distance_zeropoint;
    snapshot.leg_add        = leg_position_add;
    snapshot.bat            = battery_voltage;
    snapshot.motor1_target  = motor1.target;
    snapshot.motor2_target  = motor2.target;
    snapshot.unctrl         = uncontrolable;
    snapshot.jump           = jump_flag;
    portEXIT_CRITICAL(&dataMux);

    vTaskDelay(1);
  }
}

void TaskCommunication(void *pvParameters) 
{
  static unsigned long lastUdpSend = 0;

  while (1) 
  {
    web_loop();
    rp.spinOnce();

    // 每20ms发送一次UDP状态数据
    if (millis() - lastUdpSend >= 20) 
    {
      lastUdpSend = millis();
      sendStatusUDP();
    }

    vTaskDelay(1);
  }
}

// ------------------ lqr自平衡控制 ------------------
void lqr_balance_loop()
{
  LQR_distance  = (-0.5) * (motor1.shaft_angle + motor2.shaft_angle);
  LQR_speed     = (-0.5) * (motor1.shaft_velocity + motor2.shaft_velocity);
  LQR_angle = (float)mpu6050.getAngleY();
  LQR_gyro  = (float)mpu6050.getGyroY(); 

  // 计算自平衡输出
  // 检查是否处于扰动窗口
  if (perturb_offset != 0.0f && millis() > perturb_end_time) 
  {
    perturb_offset = 0.0f;   // 时间到，清零
  }
  float effective_zeropoint = angle_zeropoint + perturb_offset;
  angle_control = pid_angle(LQR_angle - effective_zeropoint);
  gyro_control      = pid_gyro(LQR_gyro);

  // 运动细节优化处理
  if(wrobot.joyy != 0)
  {
    distance_zeropoint = LQR_distance;
    pid_lqr_u.reset();
  }

  if( (wrobot.joyx_last!=0 && wrobot.joyx==0) || (wrobot.joyy_last!=0 && wrobot.joyy==0) )
  {
    wrobot_move_stop_flag = 1;
  }
  if( (wrobot_move_stop_flag==1) && (abs(LQR_speed)<0.5) )
  {
    distance_zeropoint = LQR_distance;
    wrobot_move_stop_flag = 0;
  }

  if( abs(LQR_speed)>15 )
  {
    distance_zeropoint = LQR_distance;
  }

  // 计算位移控制输出，同时保留滤波后的 joyy 值供快照使用
  float joyy_filtered = lpf_joyy(wrobot.joyy);
  joyy_filtered_global = joyy_filtered;   // 存储到全局变量
  distance_control  = pid_distance(LQR_distance - distance_zeropoint);
  speed_control     = pid_speed(LQR_speed - 0.1 * joyy_filtered);

  // 轮部离地检测
  robot_speed_last = robot_speed;
  robot_speed = LQR_speed;
  if( abs(robot_speed-robot_speed_last) > 10 || abs(robot_speed) > 50 || (jump_flag != 0) )
  {
    distance_zeropoint = LQR_distance;
    LQR_u = angle_control + gyro_control;
    pid_lqr_u.reset();
  }
  else
  {
    LQR_u = angle_control + gyro_control + distance_control + speed_control; 
  }
  
  // 触发条件：遥控器无信号输入、轮部位移控制正常介入、不处于跳跃后的恢复时期
  if( abs(LQR_u)<5 && wrobot.joyy == 0 && abs(distance_control)<4 && (jump_flag == 0))
  {
    LQR_u = pid_lqr_u(LQR_u);
    angle_zeropoint -= pid_zeropoint(lpf_zeropoint(distance_control));
  }
  else
  {
    pid_lqr_u.reset();
  }

  // 平衡控制参数自适应
  if(wrobot.height < 50)
  {
    pid_speed.P = 0.7;
  }
  else if(wrobot.height < 64)
  {
    pid_speed.P = 0.6;
  }
  else
  {
    pid_speed.P = 0.5;
  }
}

//腿部动作控制
void leg_loop()
{
  jump_loop();
  if(jump_flag == 0)
  {
    ACC[0] = 8;
    ACC[1] = 8;
    Speed[0] = 200;
    Speed[1] = 200;
    float roll_angle  = (float)mpu6050.getAngleX() + 2.0;
    leg_position_add = pid_roll_angle(lpf_roll(roll_angle));
    Position[0] = 2048 + 12 + 8.4*(wrobot.height-32) - leg_position_add;
    Position[1] = 2048 - 12 - 8.4*(wrobot.height-32) - leg_position_add;
    if( Position[0]<2110 )  Position[0]=2110;
    else if( Position[0]>2510 ) Position[0]=2510;
    if( Position[1]<1586 )  Position[1]=1586;
    else if( Position[1]>1986 ) Position[1]=1986;
    sms_sts.SyncWritePosEx(ID, 2, Position, Speed, ACC);
  }  
}

//跳跃控制
void jump_loop()
{
  if( (wrobot.dir_last == 5) && (wrobot.dir == 4) && (jump_flag == 0) )
  {
      ACC[0] = 0;
      ACC[1] = 0;
      Speed[0] = 0;
      Speed[1] = 0;
      Position[0] = 2048 + 12 + 8.4*(80-32);
      Position[1] = 2048 - 12 - 8.4*(80-32);
      sms_sts.SyncWritePosEx(ID, 2, Position, Speed, ACC);
      jump_flag = 1;
  }
  if( jump_flag > 0 )
  {
    jump_flag++;
    if( (jump_flag > 30) && (jump_flag < 35) )
    {
      ACC[0] = 0;
      ACC[1] = 0;
      Speed[0] = 0;
      Speed[1] = 0;
      Position[0] = 2048 + 12 + 8.4*(40-32);
      Position[1] = 2048 - 12 - 8.4*(40-32);
      sms_sts.SyncWritePosEx(ID, 2, Position, Speed, ACC);
      jump_flag = 40;
    }
    if(jump_flag > 200) jump_flag = 0;
  }
}

//yaw轴转向控制
void yaw_loop()
{
  yaw_angle_addup();
  YAW_angle_total += wrobot.joyx*0.002;
  float yaw_angle_control = pid_yaw_angle(YAW_angle_total);
  float yaw_gyro_control  = pid_yaw_gyro(YAW_gyro);
  YAW_output = yaw_angle_control + yaw_gyro_control;  
}

void web_loop()
{
  webserver.handleClient();
  websocket.loop();
  // rp.spinOnce() 已在通信任务中调用，这里移除避免重复
}

//yaw轴角度累加函数 (无更改)
void yaw_angle_addup() 
{
  YAW_angle  = (float)mpu6050.getAngleZ();;
  YAW_gyro   = (float)mpu6050.getGyroZ();

  if(YAW_angle_zero_point == (-10))
  {
    YAW_angle_zero_point = YAW_angle;
  }

  float yaw_angle_1,yaw_angle_2,yaw_addup_angle;
  if(YAW_angle > YAW_angle_last)
  {
    yaw_angle_1 = YAW_angle - YAW_angle_last;
    yaw_angle_2 = YAW_angle - YAW_angle_last - 2*PI;
  }
  else
  {
    yaw_angle_1 = YAW_angle - YAW_angle_last;
    yaw_angle_2 = YAW_angle - YAW_angle_last + 2*PI;
  }

  if(abs(yaw_angle_1)>abs(yaw_angle_2))
  {
    yaw_addup_angle=yaw_angle_2;
  }
  else
  {
    yaw_addup_angle=yaw_angle_1;
  }

  YAW_angle_total = YAW_angle_total + yaw_addup_angle;
  YAW_angle_last = YAW_angle;
}

void basicWebCallback(void)
{
  webserver.send(300, "text/html", basic_web);
}

void webSocketEventCallback(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{
  if(type == WStype_TEXT)
  {
    String payload_str = String((char*) payload);   
    StaticJsonDocument<300> doc;  
    DeserializationError error = deserializeJson(doc, payload_str);
    
    if (error) 
    {
      SerialAndWebLn("JSON parse failed");
      return;
    }

    String mode_str = doc["mode"];
    // ========== 新增扰动指令 ==========
    if(mode_str == "perturb")
    {
      float offset = doc["offset"] | 0.0f;
      unsigned int duration = doc["duration"] | 0;
      if (offset != 0.0f && duration > 0) 
      {
        perturb_offset = offset;
        perturb_end_time = millis() + duration;
        SerialAndWebLn("Perturb: offset=" + String(offset) + "°, dur=" + String(duration) + "ms");
      }
      return;
    }
    if(mode_str == "basic")
    {
      rp.parseBasic(doc);
    }
    else if(mode_str == "pid")
    {
      String cmd = doc["cmd"];
      
      // 处理恢复自动的特殊指令
      if(cmd == "R") 
      {
        manual_pid_speed_override = false;
        SerialAndWebLn("NOTICE: pid_speed returned to AUTO mode.");
        return;
      }

      float P = doc["P"] | -9999.0f;         
      float I = doc["I"] | -9999.0f;
      float D = doc["D"] | -9999.0f;
      float Tf = doc["Tf"] | -9999.0f;      

      String commanderStr = "";
      if (cmd == "G" || cmd == "J" || cmd == "L") 
      {
        if (Tf > -9999.0f) commanderStr = cmd + "Tf" + String(Tf, 4);
      } 
      else 
      {
        commanderStr = cmd;
        if (P > -9999.0f) commanderStr += "P" + String(P, 4);
        if (I > -9999.0f) commanderStr += "I" + String(I, 4);
        if (D > -9999.0f) commanderStr += "D" + String(D, 4);
      }

      if (commanderStr.length() > 0) 
      {
        String logMsg = "Received PID set: " + commanderStr;
        Serial.println(logMsg);
        sendToWeb(logMsg + "\n");
        command.run(const_cast<char*>(commanderStr.c_str()));

        PIDController* targetPid = nullptr; 
        LowPassFilter* targetLpf = nullptr;
        
        if (!(cmd == "G" || cmd == "J" || cmd == "L")) 
        {
          if(cmd == "A") targetPid = &pid_angle;
          else if(cmd == "B") targetPid = &pid_gyro;
          else if(cmd == "C") targetPid = &pid_distance;
          else if(cmd == "D") 
          {
            targetPid = &pid_speed;
            manual_pid_speed_override = true;
            SerialAndWebLn(" NOTICE: pid_speed is now in MANUAL mode.");
          }
          else if(cmd == "E") targetPid = &pid_yaw_angle;
          else if(cmd == "F") targetPid = &pid_yaw_gyro;
          else if(cmd == "H") targetPid = &pid_lqr_u;
          else if(cmd == "I") targetPid = &pid_zeropoint;
          else if(cmd == "K") targetPid = &pid_roll_angle;
          
          if(targetPid != nullptr) 
          {
            targetPid->reset(); 
            String pidMsg = "Updated PID: P=" + String(targetPid->P, 4) + 
                             " I=" + String(targetPid->I, 4) + 
                             " D=" + String(targetPid->D, 4);
            SerialAndWebLn(pidMsg);
          }
        } 
        else 
        {
          if(cmd == "G") targetLpf = &lpf_joyy;
          else if(cmd == "J") targetLpf = &lpf_zeropoint;
          else if(cmd == "L") targetLpf = &lpf_roll;
          
          if(targetLpf != nullptr) 
          {
            String lpfMsg = "Updated LPF: Tf=" + String(targetLpf->Tf, 4);
            SerialAndWebLn(lpfMsg);
          }
        }
      }
    }
    else if(mode_str == "get_params")
    {
      String paramsStr = "";
      paramsStr += "===== PID 控制器参数 =====\n";
      paramsStr += "pid_angle:     P=" + String(pid_angle.P,4)     + " I=" + String(pid_angle.I,4)     + " D=" + String(pid_angle.D,4)     + "\n";
      paramsStr += "pid_gyro:      P=" + String(pid_gyro.P,4)      + " I=" + String(pid_gyro.I,4)      + " D=" + String(pid_gyro.D,4)      + "\n";
      paramsStr += "pid_distance:  P=" + String(pid_distance.P,4)  + " I=" + String(pid_distance.I,4)  + " D=" + String(pid_distance.D,4)  + "\n";
      paramsStr += "pid_speed:     P=" + String(pid_speed.P,4)     + " I=" + String(pid_speed.I,4)     + " D=" + String(pid_speed.D,4)     + "\n";
      paramsStr += "pid_yaw_angle: P=" + String(pid_yaw_angle.P,4) + " I=" + String(pid_yaw_angle.I,4) + " D=" + String(pid_yaw_angle.D,4) + "\n";
      paramsStr += "pid_yaw_gyro:  P=" + String(pid_yaw_gyro.P,4)  + " I=" + String(pid_yaw_gyro.I,4)  + " D=" + String(pid_yaw_gyro.D,4)  + "\n";
      paramsStr += "pid_lqr_u:     P=" + String(pid_lqr_u.P,4)     + " I=" + String(pid_lqr_u.I,4)     + " D=" + String(pid_lqr_u.D,4)     + "\n";
      paramsStr += "pid_zeropoint: P=" + String(pid_zeropoint.P,4) + " I=" + String(pid_zeropoint.I,4) + " D=" + String(pid_zeropoint.D,4) + "\n";
      paramsStr += "pid_roll_angle:P=" + String(pid_roll_angle.P,4)+ " I=" + String(pid_roll_angle.I,4)+ " D=" + String(pid_roll_angle.D,4)+ "\n";
      paramsStr += "\n===== 低通滤波器参数 =====\n";
      paramsStr += "lpf_joyy:      Tf=" + String(lpf_joyy.Tf,4)      + "\n";
      paramsStr += "lpf_zeropoint: Tf=" + String(lpf_zeropoint.Tf,4) + "\n";
      paramsStr += "lpf_roll:      Tf=" + String(lpf_roll.Tf,4)      + "\n";
      
      SerialAndWebLn(paramsStr);
      return;
    }
  }
}

//电压检测初始化
void adc_calibration_init()
{
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) 
    {
      printf("eFuse Two Point: Supported\n");
    } 
    else 
    {
      printf("eFuse Two Point: NOT supported\n");
    }
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK) 
    {
      printf("eFuse Vref: Supported\n");
    } 
    else 
    {
      printf("eFuse Vref: NOT supported\n");
    }
}

//电压检测（添加全局电压变量更新）
void bat_check()
{
  if(bat_check_num > 1000)
  {
    uint32_t sum = 0;
    sum= analogRead(BAT_PIN);
    uint32_t voltage = esp_adc_cal_raw_to_voltage(sum, &adc_chars);
    double battery=(voltage*3.97)/1000.0;
    battery_voltage = battery;   // 更新全局电压

    if(battery>7.8)
    {
      digitalWrite(LED_BAT,HIGH);
    }
    else
    {
      digitalWrite(LED_BAT,LOW);
    }

    bat_check_num = 0;
    if(battery < 7.2) 
    {
      battery_dead = true;
    }
  }
  else
    bat_check_num++;
}

void setup() 
{
  Serial.begin(115200);
  Serial2.begin(1000000);

  WiFi_SetAP();
  webserver.begin();
  webserver.on("/", HTTP_GET, basicWebCallback);
  websocket.begin();
  websocket.onEvent(webSocketEventCallback);

  // 舵机初始化
  sms_sts.pSerial = &Serial2;
  ID[0] = 1;
  ID[1] = 2;
  ACC[0] = 30;
  ACC[1] = 30;
  Speed[0] = 300;
  Speed[1] = 300;  
  Position[0] = 2148;
  Position[1] = 1948;
  sms_sts.SyncWritePosEx(ID, 2, Position, Speed, ACC);

  // 电压检测
  adc_calibration_init();
  adc1_config_width(width);
  adc1_config_channel_atten(channel, atten);
  esp_adc_cal_characterize(unit, atten, width, 0, &adc_chars);
  pinMode(LED_BAT,OUTPUT);
  
  // 编码器设置
  I2Cone.begin(19,18, 400000UL); 
  I2Ctwo.begin(23,5, 400000UL); 
  sensor1.init(&I2Cone);
  sensor2.init(&I2Ctwo);

  // MPU6050设置
  mpu6050.begin();
  mpu6050.calcGyroOffsets(true);
  
  // 连接motor对象与编码器对象
  motor1.linkSensor(&sensor1);
  motor2.linkSensor(&sensor2);

  // 速度环PID参数
  motor1.PID_velocity.P = 0.05;
  motor1.PID_velocity.I = 1;
  motor1.PID_velocity.D = 0;

  motor2.PID_velocity.P = 0.05;
  motor2.PID_velocity.I = 1;
  motor2.PID_velocity.D = 0;

  // 驱动器设置
  motor1.voltage_sensor_align = 6;
  motor2.voltage_sensor_align = 6;
  driver1.voltage_power_supply = 8;
  driver2.voltage_power_supply = 8;
  driver1.init();
  driver2.init();

  // 连接motor对象与驱动器对象
  motor1.linkDriver(&driver1);
  motor2.linkDriver(&driver2);

  motor1.torque_controller = TorqueControlType::voltage;
  motor2.torque_controller = TorqueControlType::voltage;   
  motor1.controller = MotionControlType::torque;
  motor2.controller = MotionControlType::torque;
  
  // monitor相关设置
  motor1.useMonitoring(Serial);
  motor2.useMonitoring(Serial);
  // 电机初始化
  motor1.init();
  motor1.initFOC(); 
  motor2.init();
  motor2.initFOC();

  // 映射电机到commander (供网页端调用)
  command.add('A', StabAngle, "pid angle");
  command.add('B', StabGyro, "pid gyro");
  command.add('C', StabDistance, "pid distance");
  command.add('D', StabSpeed, "pid speed");
  command.add('E', StabYawAngle, "pid yaw angle");
  command.add('F', StabYawGyro, "pid yaw gyro");
  command.add('G', lpfJoyy, "lpf joyy");
  command.add('H', StabLqrU, "pid lqr u");
  command.add('I', StabZeropoint, "pid zeropoint");
  command.add('J', lpfZeropoint, "lpf zeropoint");
  command.add('K', StabRollAngle, "pid roll angle");
  command.add('L', lpfRoll, "lpf roll");

  // 启动 UDP
  udp.begin(12346);

  // 创建双核任务（堆栈大小略调整）
  xTaskCreatePinnedToCore(
    TaskBalance,
    "Balance",
    8192,
    NULL,
    2,
    &TaskBalance_Handle,
    0);

  xTaskCreatePinnedToCore(
    TaskCommunication,
    "Comm",
    4096,     // 通信任务堆栈 6144→4096 足矣
    NULL,
    1,
    &TaskComm_Handle,
    1);
}

void loop() 
{
  vTaskDelay(1000);
}





