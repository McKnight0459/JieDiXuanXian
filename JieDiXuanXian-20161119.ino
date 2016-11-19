/*
   Name:             JieDiXuanXian.ino
   Created:          2016/10/5 22:28:19
   Author:           张岩
   Hardware:         Arduino MEGA 2560、LED、LCD、时间芯片、按钮、导线、电流变送器、电压变送器、直流电源
   Aescription:      根据零序电流的大小来判断接地的线路
   Update log:       2016-09-05 22：42 检测各条回路的零序电流，当某条回路的零序电流达到定值，红灯发光，蜂鸣器报警
                     2016-09-22 09：13 增加LCD显示屏，增加试验按钮，试验按钮按下，则所有LED亮起，LCD显示屏上显示试验按钮按下
                                       接地时LCD上显示接地的线路名称，接地的零序电流
                     2016-09-25 19：55 增加检测10千伏开口三角处电压，当其大于等于25V时，并满足零序电流定值时，触发报警，接地
                                       时，开口三角电压同时显示在LCD屏幕上
                     2016-09-26 20：28 增加LCD上显示汉字的功能，但是汉字比较小，有些字无法显示完整，如“接”，“零”，等
                     2016-09-28 08：05 增加时间芯片，暂时可显示运行的时刻，发生故障的时刻等，但是不能保存记录，功能待添加
                     2016-10-04 10：10 算法不准确，根据单相接地时，接地电流为非故障线路电容电流之和，更新算法，各条线路的零序
                                       电流比较大小，最大的判定为接地线路
                     2016-10-10 19：35 更新算法，优化代码，更新逻辑结构，去除无法识别的LCD文字
                     2016-10-11 21：23 使用外部中断函数实现试验按钮和选线按钮的功能
*/
#include <Wire.h>
#include <Rtc_Pcf8563.h>
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd( 0x27, 20, 4 );


Rtc_Pcf8563 rtc;

const int  ShiYanButton  = 2;    /* 实验按钮 */
const int XuanXianButton  = 3;    /*选线按钮 */

const int CT82009 = A0;           /* 10千伏I段电压互感器82009的零序电流互感器 */
const int CT82010 = A1;           /* 10千伏II段电压互感器82010的零序电流互感器 */
const int CT82011 = A2;           /* 10千伏升382甲线82011的零序电流互感器 */
const int CT82012 = A3;           /* 10千伏升382乙线82012的零序电流互感器 */
const int CT82013 = A4;           /* 10千伏尚12甲线82013的零序电流互感器 */
const int CT82017 = A5;           /* 10千伏东12甲线82017的零序电流互感器 */
const int CT82018 = A6;           /* 10千伏东12乙线82018的零序电流互感器 */
const int PT82005 = A7;           /* 10千伏I段电压互感器82005的开口三角 */
const int PT82006 = A8;           /* 10千伏II段电压互感器82006的开口三角 */

const int GLED  = 22;           /* 绿灯，正常运行灯 */
const int YLED  = 23;           /* 黄灯，报警时闪烁 */

const int RLED82009 = 24;   /* 红灯，接地选线灯，82009 */
const int RLED82010 = 25;   /* 红灯，接地选线灯，82010 */
const int RLED82011 = 26;   /* 红灯，接地选线灯，82011 */
const int RLED82012 = 27;   /* 红灯，接地选线灯，82012 */
const int RLED82013 = 28;   /* 红灯，接地选线灯，82013 */
const int RLED82017 = 29;   /* 红灯，接地选线灯，82017 */
const int RLED82018 = 30;   /* 红灯，接地选线灯，82018 */

const int I0limit = 153;          /* 零序电流整定值（153相当于6A） */
const int U0limit = 153;          /* 开口三角电压整定值（变比待测） */

const int Lcdtime   = 1000; /* LCD屏幕刷新速度 */
const int DelayTime = 50;   /* 延时时间 */
const int LedDelayTime  = 1000; /* LED延时时间 */

int a, b, c, d, e, f, g;
String  A, B, C, D, E, F, G;

volatile int  ShiYanButtonstate = LOW;
volatile int  XuanXianButtonstate = LOW;

const int delayGSM = 2000;

void gsmCN() {
  Serial1.println("AT");
  delay(delayGSM);
  Serial1.println("AT+CMGF=1");
  delay(delayGSM);
  Serial1.println("AT+CSMP=17,167,2,25");
  delay(delayGSM);
  Serial1.println("AT+CSCS=\"UCS2\"");
  delay(delayGSM);
  Serial1.println("AT+CMGS=\"00310035003200340035003900380031003500380036\"");
  delay(delayGSM);
  Serial1.print("53470032003053D87535624053D1751F00310030006B005663A557306545969CFF0C6B63572868C06D4B63A557307EBF8DEFFF0C8BF77B495F854E0B4E00676177ED4FE1768462A5544A");
  delay(delayGSM);
  Serial1.println((char)26);
  delay(delayGSM);
}
void gsmEN() {
  Serial1.println("AT");
  delay(delayGSM);
  Serial1.println("AT+CMGF=1");
  delay(delayGSM);
  Serial1.println("AT+CSCS=\"GSM\"");
  delay(delayGSM);
  Serial1.println("AT+CMGS=\"15245981586\"");
  delay(delayGSM);
  Serial1.print("Fault Earthing Line is ");
  Serial1.print(G);
  Serial1.print( "." );
  Serial1.print( "Electric current is " );
  Serial1.print( g / 25.575 );
  Serial1.print( "A" );
  Serial1.print( "." );
  delay(delayGSM);
  Serial1.println((char)26);
  delay(delayGSM);
}

void setup()
{
  Serial.begin( 9600 );
  Serial1.begin(115200);
  lcd.init();
  lcd.backlight();
  lcd.setCursor( 1, 1 );
  lcd.printstr( "The S20 Substation" );
  delay( DelayTime * 20 );
  lcd.clear();
  lcd.setCursor( 1, 0 );
  lcd.printstr( "The S20 Substation" );
  lcd.setCursor( 1, 2 );
  lcd.printstr( "Booting" );
  for ( int Boot = 9; Boot <= 17; Boot += 2 )
  {
    lcd.setCursor( Boot, 2 );
    lcd.printstr( ".." );
    delay( LedDelayTime );
  }


  /*
     初始化时钟命令，默认时钟已经调好
     rtc.initClock();//初始化时钟
     rtc.setDate(28, 3, 9, 0, 16);//日, 星期, 月, 世纪(1=1900, 0=2000), 年(0-99)
     rtc.setTime(20, 05, 00);//时，分，秒
  */
  pinMode( ShiYanButton, INPUT );
  pinMode( XuanXianButton, INPUT );

  pinMode( GLED, OUTPUT );
  pinMode( YLED, OUTPUT );
  pinMode( RLED82009, OUTPUT );
  pinMode( RLED82010, OUTPUT );
  pinMode( RLED82011, OUTPUT );
  pinMode( RLED82012, OUTPUT );
  pinMode( RLED82013, OUTPUT );
  pinMode( RLED82017, OUTPUT );
  pinMode( RLED82018, OUTPUT );


  /*digitalWrite(GLED, HIGH);
     digitalWrite(YLED, LOW);
     digitalWrite(RLED82009, LOW);
     digitalWrite(RLED82010, LOW);
     digitalWrite(RLED82011, LOW);
     digitalWrite(RLED82012, LOW);
     digitalWrite(RLED82013, LOW);
     digitalWrite(RLED82017, LOW);
     digitalWrite(RLED82018, LOW);
  */
}


void XuanXianbutton()
{
  XuanXianButtonstate = !XuanXianButtonstate;
  delay( DelayTime );
  Serial.print( " IO(82009):" );
  Serial.print( a );
  Serial.print( "A" );
  Serial.print( " IO(82010):" );
  Serial.print( b );
  Serial.print( "A" );
  Serial.print( " IO(82011):" );
  Serial.print( c );
  Serial.print( "A" );
  Serial.print( " IO(82012):" );
  Serial.print( d );
  Serial.print( "A" );
  Serial.print( " IO(82013):" );
  Serial.print( e );
  Serial.print( "A" );
  Serial.print( " IO(82017):" );
  Serial.print( f );
  Serial.print( "A" );
  Serial.print( " IO(82018):" );
  Serial.print( g );
  Serial.print( "A" );
  Serial.print( " UO(82005):" );
  Serial.print( digitalRead(PT82005)  );
  Serial.print( "V" );
  Serial.print( " UO(82006):" );
  Serial.print( digitalRead(PT82006)  );
  Serial.println( "V" );
  delay( DelayTime );
}


void ShiYanbutton()
{
  ShiYanButtonstate = !ShiYanButtonstate;
  digitalWrite( GLED, HIGH );
  digitalWrite( YLED, HIGH );
  digitalWrite( RLED82009, HIGH );
  digitalWrite( RLED82010, HIGH );
  digitalWrite( RLED82011, HIGH );
  digitalWrite( RLED82012, HIGH );
  digitalWrite( RLED82013, HIGH );
  digitalWrite( RLED82017, HIGH );
  digitalWrite( RLED82018, HIGH );
}


void loop()
{
  /* 试验按钮外部中断 */
  attachInterrupt( 0, ShiYanbutton, CHANGE );
  /*选线按钮外部中断 */
  attachInterrupt( 1, XuanXianbutton, RISING );
  /* 系统正常运行时LCD的显示 */
  lcd.clear();
  lcd.setCursor( 1, 0 );
  lcd.printstr( "The S20 Substation" );
  lcd.setCursor( 2, 2 );
  lcd.printstr( "Running normally" );
  lcd.setCursor( 0, 3 );
  lcd.printstr( rtc.formatDate() );
  lcd.setCursor( 12, 3 );
  lcd.print( rtc.formatTime() );

  digitalWrite( GLED, HIGH );
  digitalWrite( YLED, LOW );
  digitalWrite( RLED82009, LOW );
  digitalWrite( RLED82010, LOW );
  digitalWrite( RLED82011, LOW );
  digitalWrite( RLED82012, LOW );
  digitalWrite( RLED82013, LOW );
  digitalWrite( RLED82017, LOW );
  digitalWrite( RLED82018, LOW );

  /* a=(82009---82010) */
  if ( analogRead( A0 ) > analogRead( A1 ) )
  {
    a = analogRead( A0 );
    A = "82009";
  } else {
    a = analogRead( A1 );
    A = "82010";
  }
  /* b=(82011---82012) */
  if ( analogRead( A2 ) > analogRead( A3 ) )
  {
    b = analogRead( A2 );
    B = "82011";
  } else {
    b = analogRead( A3 );
    B = "82012";
  }
  /* c=(82013---82017) */
  if ( analogRead( A4 ) > analogRead( A5 ) )
  {
    c = analogRead( A4 );
    C = "82013";
  } else {
    c = analogRead( A5 );
    C = "82017";
  }
  d = analogRead( A6 );
  D = "82018";
  /* f=(82009---82010--82011--82012) */
  if ( a > b )
  {
    e = a;
    E = A;
  } else {
    e = b;
    E = B;
  }
  /* g=(82013---82017--82018) */
  if ( c > d )
  {
    f = c;
    F = C;
  } else {
    f = d;
    F = D;
  }
  /* ALL */
  if ( e > f )
  {
    g = e;
    G = E;
  } else {
    g = f;
    G = F;
  }
  if ( (G == "82009") && (analogRead( A0 ) >= I0limit) && ((analogRead(PT82005) >= U0limit) || (analogRead(PT82006) >= U0limit)) )
  {
    digitalWrite( GLED, LOW );
    digitalWrite( YLED, HIGH );
    digitalWrite( RLED82009, HIGH );
    digitalWrite( RLED82010, LOW );
    digitalWrite( RLED82011, LOW );
    digitalWrite( RLED82012, LOW );
    digitalWrite( RLED82013, LOW );
    digitalWrite( RLED82017, LOW );
    digitalWrite( RLED82018, LOW );
    lcd.clear();
    lcd.setCursor( 1, 0 );
    lcd.printstr( "The S20 Substation" );
    lcd.setCursor( 0, 1 );
    lcd.print( G );
    lcd.setCursor( 6, 1 );
    lcd.printstr( "Fault Earthing" );
    lcd.setCursor( 0, 2 );
    lcd.printstr( "IO" );
    lcd.setCursor( 3, 2 );
    lcd.print( "(" );
    lcd.setCursor( 4, 2 );
    lcd.print( G );
    lcd.setCursor( 9, 2 );
    lcd.print( ")" );
    lcd.setCursor( 11, 2 );
    lcd.printstr( "=" );
    lcd.setCursor( 13, 2 );
    lcd.print( g / 25.575 );
    lcd.setCursor( 19, 2 );
    lcd.printstr( "A" );
    lcd.setCursor( 0, 3 );
    lcd.printstr( rtc.formatDate() );
    lcd.setCursor( 12, 3 );
    lcd.print( rtc.formatTime() );
    gsmCN(); 
    delay(delayGSM);
    gsmEN();
    Serial.print( "Fault Earthing Line is " );
    Serial.print( G );
    Serial.print( ".  " );
    Serial.print( "Electric current is " );
    Serial.print( g / 25.575 );
    Serial.print( "A" );
    Serial.println( "." );
    delay( DelayTime * 50 );
  } else if ( (G == "82010") && (analogRead( A1 ) >= I0limit) && ((analogRead(PT82005) >= U0limit) || (analogRead(PT82006) >= U0limit)) )
  {
    digitalWrite( GLED, LOW );
    digitalWrite( YLED, HIGH );
    digitalWrite( RLED82009, LOW );
    digitalWrite( RLED82010, HIGH );
    digitalWrite( RLED82011, LOW );
    digitalWrite( RLED82012, LOW );
    digitalWrite( RLED82013, LOW );
    digitalWrite( RLED82017, LOW );
    digitalWrite( RLED82018, LOW );
    lcd.clear();
    lcd.setCursor( 1, 0 );
    lcd.printstr( "The S20 Substation" );
    lcd.setCursor( 0, 1 );
    lcd.print( G );
    lcd.setCursor( 6, 1 );
    lcd.printstr( "Fault Earthing" );
    lcd.setCursor( 0, 2 );
    lcd.printstr( "IO" );
    lcd.setCursor( 3, 2 );
    lcd.print( "(" );
    lcd.setCursor( 4, 2 );
    lcd.print( G );
    lcd.setCursor( 9, 2 );
    lcd.print( ")" );
    lcd.setCursor( 11, 2 );
    lcd.printstr( "=" );
    lcd.setCursor( 13, 2 );
    lcd.print( g / 25.575 );
    lcd.setCursor( 19, 2 );
    lcd.printstr( "A" );
    lcd.setCursor( 0, 3 );
    lcd.printstr( rtc.formatDate() );
    lcd.setCursor( 12, 3 );
    lcd.print( rtc.formatTime() );
    gsmCN(); 
    delay(delayGSM);
    gsmEN();
    Serial.print( "Fault Earthing Line is " );
    Serial.print( G );
    Serial.print( ".  " );
    Serial.print( "Electric current is " );
    Serial.print( g / 25.575 );
    Serial.print( "A" );
    Serial.println( "." );
    delay( DelayTime * 50 );
  } else if ( (G == "82011") && (analogRead( A2 ) >= I0limit) && ((analogRead(PT82005) >= U0limit) || (analogRead(PT82006) >= U0limit)) )
  {
    digitalWrite( GLED, LOW );
    digitalWrite( YLED, HIGH );
    digitalWrite( RLED82009, LOW );
    digitalWrite( RLED82010, LOW );
    digitalWrite( RLED82011, HIGH );
    digitalWrite( RLED82012, LOW );
    digitalWrite( RLED82013, LOW );
    digitalWrite( RLED82017, LOW );
    digitalWrite( RLED82018, LOW );
    lcd.clear();
    lcd.setCursor( 1, 0 );
    lcd.printstr( "The S20 Substation" );
    lcd.setCursor( 0, 1 );
    lcd.print( G );
    lcd.setCursor( 6, 1 );
    lcd.printstr( "Fault Earthing" );
    lcd.setCursor( 0, 2 );
    lcd.printstr( "IO" );
    lcd.setCursor( 3, 2 );
    lcd.print( "(" );
    lcd.setCursor( 4, 2 );
    lcd.print( G );
    lcd.setCursor( 9, 2 );
    lcd.print( ")" );
    lcd.setCursor( 11, 2 );
    lcd.printstr( "=" );
    lcd.setCursor( 13, 2 );
    lcd.print( g / 25.575 );
    lcd.setCursor( 19, 2 );
    lcd.printstr( "A" );
    lcd.setCursor( 0, 3 );
    lcd.printstr( rtc.formatDate() );
    lcd.setCursor( 12, 3 );
    lcd.print( rtc.formatTime() );
    gsmCN(); 
    delay(delayGSM);
    gsmEN();
    Serial.print( "Fault Earthing Line is " );
    Serial.print( G );
    Serial.print( ".  " );
    Serial.print( "Electric current is " );
    Serial.print( g / 25.575 );
    Serial.print( "A" );
    Serial.println( "." );
    delay( DelayTime * 50 );
  } else if ( (G == "82012") && (analogRead( A3 ) >= I0limit) && ((analogRead(PT82005) >= U0limit) || (analogRead(PT82006) >= U0limit)) )
  {
    digitalWrite( GLED, LOW );
    digitalWrite( YLED, HIGH );
    digitalWrite( RLED82009, LOW );
    digitalWrite( RLED82010, LOW );
    digitalWrite( RLED82011, LOW );
    digitalWrite( RLED82012, HIGH );
    digitalWrite( RLED82013, LOW );
    digitalWrite( RLED82017, LOW );
    digitalWrite( RLED82018, LOW );
    lcd.clear();
    lcd.setCursor( 1, 0 );
    lcd.printstr( "The S20 Substation" );
    lcd.setCursor( 0, 1 );
    lcd.print( G );
    lcd.setCursor( 6, 1 );
    lcd.printstr( "Fault Earthing" );
    lcd.setCursor( 0, 2 );
    lcd.printstr( "IO" );
    lcd.setCursor( 3, 2 );
    lcd.print( "(" );
    lcd.setCursor( 4, 2 );
    lcd.print( G );
    lcd.setCursor( 9, 2 );
    lcd.print( ")" );
    lcd.setCursor( 11, 2 );
    lcd.printstr( "=" );
    lcd.setCursor( 13, 2 );
    lcd.print( g / 25.575 );
    lcd.setCursor( 19, 2 );
    lcd.printstr( "A" );
    lcd.setCursor( 0, 3 );
    lcd.printstr( rtc.formatDate() );
    lcd.setCursor( 12, 3 );
    lcd.print( rtc.formatTime() );
    gsmCN(); 
    delay(delayGSM);
    gsmEN();
    Serial.print( "Fault Earthing Line is " );
    Serial.print( G );
    Serial.print( ".  " );
    Serial.print( "Electric current is " );
    Serial.print( g / 25.575 );
    Serial.print( "A" );
    Serial.println( "." );
    delay( DelayTime * 50 );
  } else if ( (G == "82013") && (analogRead( A4 ) >= I0limit) && ((analogRead(PT82005) >= U0limit) || (analogRead(PT82006) >= U0limit)) )
  {
    digitalWrite( GLED, LOW );
    digitalWrite( YLED, HIGH );
    digitalWrite( RLED82009, LOW );
    digitalWrite( RLED82010, LOW );
    digitalWrite( RLED82011, LOW );
    digitalWrite( RLED82012, LOW );
    digitalWrite( RLED82013, HIGH );
    digitalWrite( RLED82017, LOW );
    digitalWrite( RLED82018, LOW );
    lcd.clear();
    lcd.setCursor( 1, 0 );
    lcd.printstr( "The S20 Substation" );
    lcd.setCursor( 0, 1 );
    lcd.print( G );
    lcd.setCursor( 6, 1 );
    lcd.printstr( "Fault Earthing" );
    lcd.setCursor( 0, 2 );
    lcd.printstr( "IO" );
    lcd.setCursor( 3, 2 );
    lcd.print( "(" );
    lcd.setCursor( 4, 2 );
    lcd.print( G );
    lcd.setCursor( 9, 2 );
    lcd.print( ")" );
    lcd.setCursor( 11, 2 );
    lcd.printstr( "=" );
    lcd.setCursor( 13, 2 );
    lcd.print( g / 25.575 );
    lcd.setCursor( 19, 2 );
    lcd.printstr( "A" );
    lcd.setCursor( 0, 3 );
    lcd.printstr( rtc.formatDate() );
    lcd.setCursor( 12, 3 );
    lcd.print( rtc.formatTime() );
    gsmCN(); 
    delay(delayGSM);
    gsmEN();
    Serial.print( "Fault Earthing Line is " );
    Serial.print( G );
    Serial.print( ".  " );
    Serial.print( "Electric current is " );
    Serial.print( g / 25.575 );
    Serial.print( "A" );
    Serial.println( "." );
    delay( DelayTime * 50 );
  } else if ( (G == "82017") && (analogRead( A5 ) >= I0limit) && ((analogRead(PT82005) >= U0limit) || (analogRead(PT82006) >= U0limit)) )
  {
    digitalWrite( GLED, LOW );
    digitalWrite( YLED, HIGH );
    digitalWrite( RLED82009, LOW );
    digitalWrite( RLED82010, LOW );
    digitalWrite( RLED82011, LOW );
    digitalWrite( RLED82012, LOW );
    digitalWrite( RLED82013, LOW );
    digitalWrite( RLED82017, HIGH );
    digitalWrite( RLED82018, LOW );
    lcd.clear();
    lcd.setCursor( 1, 0 );
    lcd.printstr( "The S20 Substation" );
    lcd.setCursor( 0, 1 );
    lcd.print( G );
    lcd.setCursor( 6, 1 );
    lcd.printstr( "Fault Earthing" );
    lcd.setCursor( 0, 2 );
    lcd.printstr( "IO" );
    lcd.setCursor( 3, 2 );
    lcd.print( "(" );
    lcd.setCursor( 4, 2 );
    lcd.print( G );
    lcd.setCursor( 9, 2 );
    lcd.print( ")" );
    lcd.setCursor( 11, 2 );
    lcd.printstr( "=" );
    lcd.setCursor( 13, 2 );
    lcd.print( g / 25.575 );
    lcd.setCursor( 19, 2 );
    lcd.printstr( "A" );
    lcd.setCursor( 0, 3 );
    lcd.printstr( rtc.formatDate() );
    lcd.setCursor( 12, 3 );
    lcd.print( rtc.formatTime() );
    gsmCN(); 
    delay(delayGSM);
    gsmEN();
    Serial.print( "Fault Earthing Line is " );
    Serial.print( G );
    Serial.print( ".  " );
    Serial.print( "Electric current is " );
    Serial.print( g / 25.575 );
    Serial.print( "A" );
    Serial.println( "." );
    delay( DelayTime * 50 );
  } else if ( (G == "82018") && (analogRead( A6 ) >= I0limit) && ((analogRead(PT82005) >= U0limit) || (analogRead(PT82006) >= U0limit)) )
  {
    digitalWrite( GLED, LOW );
    digitalWrite( YLED, HIGH );
    digitalWrite( RLED82009, LOW );
    digitalWrite( RLED82010, LOW );
    digitalWrite( RLED82011, LOW );
    digitalWrite( RLED82012, LOW );
    digitalWrite( RLED82013, LOW );
    digitalWrite( RLED82017, LOW );
    digitalWrite( RLED82018, HIGH );
    lcd.clear();
    lcd.setCursor( 1, 0 );
    lcd.printstr( "The S20 Substation" );
    lcd.setCursor( 0, 1 );
    lcd.print( G );
    lcd.setCursor( 6, 1 );
    lcd.printstr( "Fault Earthing" );
    lcd.setCursor( 0, 2 );
    lcd.printstr( "IO" );
    lcd.setCursor( 3, 2 );
    lcd.print( "(" );
    lcd.setCursor( 4, 2 );
    lcd.print( G );
    lcd.setCursor( 9, 2 );
    lcd.print( ")" );
    lcd.setCursor( 11, 2 );
    lcd.printstr( "=" );
    lcd.setCursor( 13, 2 );
    lcd.print( g / 25.575 );
    lcd.setCursor( 19, 2 );
    lcd.printstr( "A" );
    lcd.setCursor( 0, 3 );
    lcd.printstr( rtc.formatDate() );
    lcd.setCursor( 12, 3 );
    lcd.print( rtc.formatTime() );
    gsmCN(); 
    delay(delayGSM);
    gsmEN();
    Serial.print( "Fault Earthing Line is " );
    Serial.print( G );
    Serial.print( ".  " );
    Serial.print( "Electric current is " );
    Serial.print( g / 25.575 );
    Serial.print( "A" );
    Serial.println( "." );
    delay( DelayTime * 50 );
  } else {
    digitalWrite( GLED, HIGH );
    digitalWrite( YLED, LOW );
    digitalWrite( RLED82009, LOW );
    digitalWrite( RLED82010, LOW );
    digitalWrite( RLED82011, LOW );
    digitalWrite( RLED82012, LOW );
    digitalWrite( RLED82013, LOW );
    digitalWrite( RLED82017, LOW );
    digitalWrite( RLED82018, LOW );
    Serial.print( "NO Fault Earthing." );
    Serial.print( " IO(82009):" );
    Serial.print( a );
    Serial.print( "A" );
    Serial.print( " IO(82010):" );
    Serial.print( b );
    Serial.print( "A" );
    Serial.print( " IO(82011):" );
    Serial.print( c );
    Serial.print( "A" );
    Serial.print( " IO(82012):" );
    Serial.print( d );
    Serial.print( "A" );
    Serial.print( " IO(82013):" );
    Serial.print( e );
    Serial.print( "A" );
    Serial.print( " IO(82017):" );
    Serial.print( f );
    Serial.print( "A" );
    Serial.print( " IO(82018):" );
    Serial.print( g );
    Serial.print( "A" );
    Serial.print( " UO(82005):" );
    Serial.print( analogRead(PT82005) );
    Serial.print( "V" );
    Serial.print( " UO(82006):" );
    Serial.print( analogRead(PT82006) );
    Serial.println( "V" );
    delay( DelayTime * 50 );
  }
}
/*
   Serial.print("a=");
   Serial.print(a);
   Serial.print(" b=");
   Serial.print(b);
   Serial.print(" c=");
   Serial.print(c);
   Serial.print(" d=");
   Serial.print(d);
   Serial.print(" e=");
   Serial.print(e);
   Serial.print(" f=");
   Serial.print(f);
   Serial.print(" A=");
   Serial.print(A);
   Serial.print(" B=");
   Serial.print(B);
   Serial.print(" C=");
   Serial.print(C);
   Serial.print(" D=");
   Serial.print(D);
   Serial.print(" E=");
   Serial.print(E);
   Serial.print(" F=");
   Serial.print(F);
   Serial.print(" G=");
   Serial.println(G);
   }*/

