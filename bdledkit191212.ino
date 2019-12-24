#include "WiFi.h"
#include "BluetoothSerial.h"
BluetoothSerial SerialBT;

//RGB
typedef struct {
  uint8_t red;
  uint8_t green;
  uint8_t blue;
}RGB;

typedef struct{//ベストマッチリスト
    int bottle0;
    int bottle1;
}Bestmatch;

typedef struct{//ボトル番号は配列番号+1で習得
    uint8_t number_inside;//内部のボトル番号3進数4桁
    uint8_t name[16];
    RGB color;
}Bottle;
/*
  3進数について
  * パターン 3進数
  * 01 01     0
  * 01 10     1
  * 01 11     2
  * 10 01    10
  * 10 10    11
  * 10 11    12
  * 11 01    20
  * 
*/

//ボタン長押し判定用
typedef struct {
  int pinnum;
  int hold;
  int cnt;
  int oldvalue;
}BUTTON;

//ビルドドライバーの状態
typedef struct {
  bool Rset;
  bool Lset;
  int Rbottlenum;
  int Lbottlenum;
  RGB rcolor;
  RGB lcolor;
  uint8_t henshinprocess;
}STATE;

//pindefine
#define LEDR_R 32
#define LEDR_G 33
#define LEDR_B 25
#define LEDL_R 26
#define LEDL_G 14
#define LEDL_B 27
#define SWR_1  23
#define SWR_2  22
#define SWR_3  21
#define SWL_1  19
#define SWL_2  18
#define SWL_3  17
#define SWLV   13
#define MODESW0 4
#define MODESW1 16

#define LEDREAD_R 36
#define LEDREAD_G 39
#define LEDREAD_B 34

#define sh 450

#define GPIO_IN_REG (*(volatile uint32_t *)0x3FF4403C)
#define REG_SWR_1 (( GPIO_IN_REG & 0x00800000 ) >> 23)
#define REG_SWR_2 (( GPIO_IN_REG & 0x00400000 ) >> 22)
#define REG_SWR_3 (( GPIO_IN_REG & 0x00200000 ) >> 21)
#define REG_SWL_1 (( GPIO_IN_REG & 0x00080000 ) >> 19)
#define REG_SWL_2 (( GPIO_IN_REG & 0x00040000 ) >> 18)
#define REG_SWL_3 (( GPIO_IN_REG & 0x00020000 ) >> 17)
#define REG_SWLV  (( GPIO_IN_REG & 0x00002000 ) >> 13)



hw_timer_t * timer10ms_p = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

//グローバル変数

//modeは3つ
//mode 0 単体モード
//mode 1 スマホ連携モードボトル疑似装填
//mode 2 ラビットドラゴンモード
int bdmode = 0;

const RGB goldencolor = {0x5f,0xff,0};
const RGB silvercolor = {0xff,0xff,0xff};
const RGB blackcolor = {0x00,0x00,0x00};




unsigned int LoopCnt = 0;
int AnalysisCnt = 3;
int AnalysisPart = 0;//1が1ピンクロック,3が3ピンクロック,0が待機中
uint8_t AnalysisCode = 0;
bool AnalysisRedyR = true, AnalysisRedyL = true;
bool rsw1old, rsw3old, lsw1old, lsw3old,swlvold;
bool rsw1real, rsw3real, lsw1real, lsw3real,swlvreal;


#define BUTTONHOLDCNT 10


BUTTON swr1 = {SWR_1, 0,0,0};
BUTTON swr2 = {SWR_2, 0,0,0};
BUTTON swr3 = {SWR_3, 0,0,0};
BUTTON swl1 = {SWL_1, 0,0,0};
BUTTON swl2 = {SWL_2, 0,0,0};
BUTTON swl3 = {SWL_3, 0,0,0};


//ビルドドライバーの状態を保存する変数
STATE bdstate = {0,0,0,0,{0,0,0},{0,0,0},0};

//通信受信文字列
char SerialrecSTR[64] = {0};
int SerialrecCNT = 0;

void flash0(RGB R, RGB L);//ボトル装填時
void flash1(RGB R, RGB L);//レバースイッチon後のとんてんかん
void flash2(RGB R, RGB L);//きゅいんきゅいーーん
void flash3(RGB R, RGB L);//鋼のムーンサルトおおお
void flash4(RGB R, RGB L);//ベストマッチ!


//LEDcontrol RGB値と時間と光量(%で指定)を指定して光らす
void lc(RGB R, RGB L, int time, uint8_t vol){
  ledcWrite(0,(int)((double)R.red  *((double)vol*0.01)) );
  ledcWrite(1,(int)((double)R.green *((double)vol*0.01)) );
  ledcWrite(2,(int)((double)R.blue*((double)vol*0.01)) );
  ledcWrite(3,(int)((double)L.red  *((double)vol*0.01)) );
  ledcWrite(4,(int)((double)L.green *((double)vol*0.01)) );
  ledcWrite(5,(int)((double)L.blue*((double)vol*0.01)) );
  delay(time);
}

Bottle BottleLib[122] = {
{0b00000000,"dummy",{0,0,0}},
{0b01010101,"rabbit",{0xff,0,0}},
{0b01010110,"tank",{0,0,0xff}},
{0b01010111,"gorilla",{0xff,0xff,0}},
{0b01011001,"diamond",{0,0xff,0xff}},
{0b01011010,"ninja",{0x0f,0,0xff}},
{0b01011011,"comic",{0x5f,0xff,0}},
{0b01011101,"taka",{0xff,0xff,0}},
{0b01011110,"gatling",{0x3f,0xff,0xff}},
{0b01011111,"panda",{0x3f,0xff,0xff}},
{0b01100101,"rocket",{0,0xff,0xff}},
{0b01100110,"octopus",{0xff,0,0}},
{0b01100111,"light",{0x5f,0xff,0}},
{0b01101001,"kyoryu",{0,0xff,0}},
{0b01101010,"F1",{0xff,0,0}},
{0b01101011,"same",{0,0,0xff}},
{0b01101101,"bike",{0xff,0,0}},
{0b01101110,"kaizoku",{0,0xff,0xff}},
{0b01101111,"densyha",{0,0xff,0}},
{0b01110101,"spider",{0x0f,0,0xff}},
{0b01110110,"rezoko",{0x3f,0xff,0xff}},
{0b01110111,"turtle",{0,0xff,0}},
{0b01111001,"watch",{0x3f,0xff,0xff}},
{0b01111010,"kabutomushi",{0xff,0xff,0}},
{0b01111011,"camera",{0,0,0xff}},
{0b01111101,"tora",{0x5f,0xff,0}},
{0b01111110,"UFO",{0xff,0,0xff}},
{0b01111111,"phoenix",{0xff,0,0}},
{0b10010101,"robot",{0x3f,0xff,0xff}},
{0b10010110,"dragon",{0,0,0xff}},
{0b10010111,"lock",{0x5f,0xff,0}},
{0b10011001,"kujira",{0,0,0xff}},
{0b10011010,"jet",{0,0xff,0xff}},
{0b10011011,"unicorn",{0,0xff,0xff}},
{0b10011101,"keshigomu",{0x3f,0xff,0xff}},
{0b10011110,"obake",{0x3f,0xff,0xff}},
{0b10011111,"magnet",{0,0,0xff}},
{0b10100101,"shika",{0,0,0xff}},
{0b10100110,"pyramid",{0x5f,0xff,0}},
{0b10100111,"rose",{0xff,0,0}},
{0b10101001,"helicopter",{0,0xff,0}},
{0b10101010,"bat",{0x0f,0,0xff}},
{0b10101011,"engine",{0xff,0,0}},
{0b10101101,"hati",{0x5f,0xff,0}},
{0b10101110,"sensuikan",{0,0xff,0xff}},
{0b10101111,"sai",{0x3f,0xff,0xff}},
{0b10110101,"dryer",{0xff,0,0}},
{0b10110110,"scorpion",{0xff,0,0}},
{0b10110111,"gold",{0x5f,0xff,0}},
{0b10111001,"wolf",{0x3f,0xff,0xff}},
{0b10111010,"smapho",{0,0,0xff}},
{0b10111011,"kuma",{0x5f,0xff,0}},
{0b10111101,"terevi",{0x3f,0xff,0xff}},
{0b10111110,"dog",{0xff,0xff,0}},
{0b10111111,"mike",{0x3f,0xff,0xff}},
{0b11010101,"kirinn",{0x5f,0xff,0}},
{0b11010110,"senpuuki",{0,0,0xff}},
{0b11010111,"crocodile",{0x0f,0,0xff}},
{0b11011001,"remocon",{0x3f,0xff,0xff}},
{0b11011010,"penguin",{0x3f,0xff,0xff}},
{0b11011011,"skaboo",{0,0xff,0}},
{0b11011101,"soujiki",{0,0xff,0}},
{0b11011110,"lion",{0x5f,0xff,0}},
{0b11011111,"syobosya",{0xff,0,0}},
{0b11100101,"harinezumi",{0x3f,0xff,0xff}},
{0b11100110,"cobra",{0x0f,0,0xff}},
{0b11100111,"buttobasoul",{0xff,0,0}},
{0b11101001,"ganbarizing",{0,0,0xff}},
{0b11101010,"cake",{0x3f,0xff,0xff}},
{0b11101011,"santaclaus",{0xff,0,0}},
{0b11101101,"cupmen",{0x3f,0xff,0xff}},
{0b11101110,"energydrink",{0xff,0,0}},
{0b11101111,"special",{0x5f,0xff,0}},
{0b11110101,"premium",{0,0,0xff}},
{0b11110110,"card",{0,0xff,0xff}},
{0b11110111,"momotaros",{0xff,0,0}},
{0b11111001,"tantei",{0x0f,0,0xff}},
{0b11111010,"USBmemory",{0,0xff,0}},
{0b11111011,"orange",{0xff,0xff,0}},
{0b11111101,"doctor",{0x3f,0xff,0xff}},
{0b11111110,"game",{0xff,0,0xff}},
{0b11111111,"yuujo",{0xff,0xff,0}},
{0b01010101,"keisatukan",{0x3f,0xff,0xff}},
{0b01010110,"parker",{0xff,0xff,0}},
{0b01010111,"mahotukai",{0xff,0,0}},
{0b01011001,"medal",{0x5f,0xff,0}},
{0b01011010,"supersentai",{0xff,0,0}},
{0b01011011,"castle",{0x0f,0,0xff}},
{0b01011101,"shimauma",{0x0f,0,0xff}},
{0b01011110,"hammer",{0x0f,0,0xff}},
{0b01011111,"kangaroo",{0x5f,0xff,0}},
{0b01100101,"hasami",{0x0f,0,0xff}},
{0b01100110,"hukuro",{0x0f,0,0xff}},
{0b01100111,"spanner",{0x0f,0,0xff}},
{0b01101001,"kuwagata",{0x0f,0,0xff}},
{0b01101010,"CD",{0x0f,0,0xff}},
{0b01101011,"effect1",{0,0,0}},
{0b01101101,"effect2",{0,0,0}},
{0b01101110,"effect3",{0,0,0}},
{0b01101111,"effect4",{0,0,0}},
{0b01110101,"1go",{0,0xff,0}},
{0b01110110,"kuuga",{0xff,0,0}},
{0b01110111,"agito",{0xff,0,0}},
{0b01111001,"ryuki",{0xff,0,0}},
{0b01111010,"faiz",{0x5f,0xff,0}},
{0b01111011,"blade",{0,0,0xff}},
{0b01111101,"hibiki",{0x0f,0,0xff}},
{0b01111110,"kabuto",{0,0xff,0xff}},
{0b01111111,"den-o",{0xff,0,0}},
{0b10010101,"kiva",{0x5f,0xff,0}},
{0b10010110,"decade",{0xff,0,0xff}},
{0b10010111,"w",{0x0f,0,0xff}},
{0b10011001,"ooo",{0xff,0,0}},
{0b10011010,"fourze",{0x3f,0xff,0xff}},
{0b10011011,"wizard",{0x3f,0xff,0xff}},
{0b10011101,"gaim",{0x0f,0,0xff}},
{0b10011110,"drive",{0xff,0,0}},
{0b10011111,"ghost",{0xff,0xff,0}},
{0b10100101,"ex-aid",{0xff,0,0xff}},
{0b10100110,"kamenrider",{0,0,0xff}},
{0b10100111,"showarider",{0,0xff,0}},
{0b10101001,"heiseirider",{0xff,0,0}}
};

Bestmatch matchLib[50]{
{0  ,0},
{1  ,2},
{3  ,4},
{5  ,6},
{8  ,7},
{9  ,10},
{11 ,12},
{13 ,14},
{15 ,16},
{17 ,18},
{19 ,20},
{21 ,22},
{23 ,24},
{25 ,26},
{27 ,28},
{29 ,30},
{31 ,32},
{33 ,34},
{36 ,35},
{37 ,38},
{39 ,40},
{41 ,42},
{43 ,44},
{45 ,46},
{47 ,48},
{49 ,50},
{51 ,52},
{53 ,54},
{55 ,56},
{57 ,58},
{59 ,60},
{61 ,62},
{63 ,64},
{66 ,67},
{68 ,69},
{70 ,71},
{74 ,24},
{75 ,18},
{76 ,77},
{78 ,30},
{79 ,80},
{81 ,10},
{82 ,14},
{83 ,35},
{84 ,4},
{85 ,7},
{86 ,119},
{113,114},
{115,116},
{117,118}
};

int Encode_Bottle(uint8_t code){//奇妙な3進数からボトル番号へ変換
    int i=0;
    int re=0;
    if(AnalysisPart == 1){
      for(i=0;i<=81;i++){
          if(code == BottleLib[i].number_inside){
              re=i;
              break;
          }
      }
    }
    if(AnalysisPart == 3){
      for(i=82;i<=121;i++){
          if(code == BottleLib[i].number_inside){
              re=i;
              break;
          }
      }
    }
    return re;
}

bool bestmatchcheck(int bottlenum0, int bottlenum1){
  int i=0;
  int tmp = 0;
  int re = false;
  for(i=0;i<=49;i++){
    if(bottlenum0 == matchLib[i].bottle0){
      tmp = i;
      break;
    }
  }
  if(matchLib[tmp].bottle1 == bottlenum1){
    re = true;
  }

  for(i=0;i<=49;i++){
    if(bottlenum1 == matchLib[i].bottle0){
      tmp = i;
      break;
    }
  }
  if(matchLib[tmp].bottle1 == bottlenum0){
    re = true;
  }

  return re;
}

void buttonholdset(BUTTON * button){
  if(button->cnt > BUTTONHOLDCNT){
    button->hold = 1;
    button->cnt = 0;
  }
  if(button->oldvalue == digitalRead(button->pinnum) && button->oldvalue == 1) button->cnt++;
  else{
    button->cnt = 0;
    button->hold = 0;
  }
  button->oldvalue = digitalRead(button->pinnum);
}


void bottlereset(bool side){
  if(side == 0){
    delay(5);
    digitalWrite(SWR_1, LOW);
    digitalWrite(SWR_2, LOW);
    digitalWrite(SWR_3, LOW);
    delay(5);
    digitalWrite(SWR_1, HIGH);
    delay(5);
    digitalWrite(SWR_1, LOW);
    delay(5);
    digitalWrite(SWR_1, HIGH);
    delay(5);
    digitalWrite(SWR_1, LOW);
    delay(100);
  }
  else{
    delay(5);
    digitalWrite(SWL_1, LOW);
    digitalWrite(SWL_2, LOW);
    digitalWrite(SWL_3, LOW);
    delay(5);
    digitalWrite(SWL_1, HIGH);
    delay(5);
    digitalWrite(SWL_1, LOW);
    delay(5);
    digitalWrite(SWL_1, HIGH);
    delay(5);
    digitalWrite(SWL_1, LOW);
    delay(100);
  }
}

void bottleset(bool side, int bottlenum){
  //クロックが右肩か左肩かで分けて3^4*2個あるので82以上なら81を引きます
  //ボトルナンバーはコードのプラス1なので1引きます
  //コードは三進数なので変換します
  //三進数のコードは2bit使って一桁とした8bitの4桁です
  //しかし00はピンを出さないといけない関係上使用していないので各桁に1足します
  //01 0, 10 1, 11 2の対応です。
  //1桁(2bit)ずつ出力します
  uint8_t codedigit[4] = {0};
  bool part = 0;//クロックが右肩か左肩か0が1ピンクロック
  int i = 0;

  if(bottlenum >=82 ){
    bottlenum = bottlenum - 81;
    part = 1;
  }
  bottlenum = bottlenum -1;
  
  codedigit[0] = bottlenum % 3;
  bottlenum = bottlenum/3;
  codedigit[1] = bottlenum % 3;
  bottlenum = bottlenum/3;
  codedigit[2] = bottlenum % 3;
  codedigit[3] = bottlenum/3;

  for(i=0;i<4;i++) codedigit[i]++;
  

  if(side == 0 && part == 0){
    digitalWrite(SWR_1, LOW);
    digitalWrite(SWR_2, LOW);
    digitalWrite(SWR_3, LOW);
    delay(100);
    for(i=3;i>=0;i--){
      digitalWrite(SWR_1, HIGH);
      delay(5);
      digitalWrite(SWR_2, (codedigit[i] & 0b01));
      digitalWrite(SWR_3, ((codedigit[i] & 0b10)>>1));
      delay(5);
      digitalWrite(SWR_1, LOW);
      if(i==0) break;
      delay(5);
      digitalWrite(SWR_2, LOW);
      digitalWrite(SWR_3, LOW);
      delay(5);
    }
  }
  else if(side == 0 && part == 1){
    digitalWrite(SWR_1, LOW);
    digitalWrite(SWR_2, LOW);
    digitalWrite(SWR_3, LOW);
    delay(100);
    for(i=3;i>=0;i--){
      digitalWrite(SWR_3, HIGH);
      delay(5);
      digitalWrite(SWR_1, (codedigit[i] & 0b01));
      digitalWrite(SWR_2, ((codedigit[i] & 0b10)>>1));
      delay(5);
      digitalWrite(SWR_3, LOW);
      if(i==0) break;
      delay(5);
      digitalWrite(SWR_1, LOW);
      digitalWrite(SWR_2, LOW);
      delay(5);
    }
  }
  else if(side == 1 && part ==0){
    digitalWrite(SWL_1, LOW);
    digitalWrite(SWL_2, LOW);
    digitalWrite(SWL_3, LOW);
    delay(100);
    for(i=3;i>=0;i--){
      digitalWrite(SWL_1, HIGH);
      delay(5);
      digitalWrite(SWL_2, (codedigit[i] & 0b01));
      digitalWrite(SWL_3, ((codedigit[i] & 0b10)>>1));
      delay(5);
      digitalWrite(SWL_1, LOW);
      if(i==0) break;
      delay(5);
      digitalWrite(SWL_2, LOW);
      digitalWrite(SWL_3, LOW);
      delay(5);
    }
  }
  else if(side == 1 && part ==1){
    digitalWrite(SWL_1, LOW);
    digitalWrite(SWL_2, LOW);
    digitalWrite(SWL_3, LOW);
    delay(100);
    for(i=3;i>=0;i--){
      digitalWrite(SWL_3, HIGH);
      delay(5);
      digitalWrite(SWL_1, (codedigit[i] & 0b01));
      digitalWrite(SWL_2, ((codedigit[i] & 0b10)>>1));
      delay(5);
      digitalWrite(SWL_3, LOW);
      if(i==0) break;
      delay(5);
      digitalWrite(SWL_1, LOW);
      digitalWrite(SWL_2, LOW);
      delay(5);
    }
  }
  delay(100);
}



//タイマー割込みはここ!!
void IRAM_ATTR Timer10ms(){
// Increment the counter and set the time of ISR


//LEDValueR = (int)255*((double)(4095-analogRead(LEDREAD_R))/4095);
//LEDValueG = (int)255*((double)(4095-analogRead(LEDREAD_G))/4095);
//LEDValueB = (int)255*((double)(4095-analogRead(LEDREAD_B))/4095);

portENTER_CRITICAL_ISR(&timerMux);

buttonholdset(&swr1);
buttonholdset(&swr2);
buttonholdset(&swr3);
buttonholdset(&swl1);
buttonholdset(&swl2);
buttonholdset(&swl3);

portEXIT_CRITICAL_ISR(&timerMux);

//Serial.printf("%d\n",bdstate.henshinprocess);


}



void setup() {
  WiFi.mode(WIFI_OFF);
  Serial.begin(115200);

  pinMode(SWLV, INPUT);

  pinMode(MODESW0, INPUT);
  pinMode(MODESW1, INPUT);

  pinMode(LEDREAD_R, INPUT);
  pinMode(LEDREAD_G, INPUT);
  pinMode(LEDREAD_B, INPUT);

  
  pinMode(LEDR_R, OUTPUT);
  pinMode(LEDR_G, OUTPUT);
  pinMode(LEDR_B, OUTPUT);
  pinMode(LEDL_R, OUTPUT);
  pinMode(LEDL_G, OUTPUT);
  pinMode(LEDL_B, OUTPUT);
  ledcSetup(0,12800,8);
  ledcAttachPin(LEDR_R,0);
  ledcSetup(1,12800,8);
  ledcAttachPin(LEDR_G,1);
  ledcSetup(2,12800,8);
  ledcAttachPin(LEDR_B,2);
  ledcSetup(3,12800,8);
  ledcAttachPin(LEDL_R,3);
  ledcSetup(4,12800,8);
  ledcAttachPin(LEDL_G,4);
  ledcSetup(5,12800,8);
  ledcAttachPin(LEDL_B,5);

  if(!digitalRead(MODESW0)){//フルボトル疑似装填モードか判定
    bdmode = 1;//modeはフルボトル疑似装填モード
  }
  else{
    if(digitalRead(MODESW1)){
      bdmode = 0;//modeは単体発光モード
    }
    else{
      bdmode = 2;//modeはラビットドラゴンモード
    }
  }

  //連携モードならbluetoothをonにする
  //bluetoothおんだと処理が重いせいかアナログリードが動かん
  if(bdmode == 1 || bdmode ==2) SerialBT.begin("ESP32");//これも115200bps

  if(bdmode == 1){
    pinMode(SWR_1, OUTPUT);
    pinMode(SWR_2, OUTPUT);
    pinMode(SWR_3, OUTPUT);
    pinMode(SWL_1, OUTPUT);
    pinMode(SWL_2, OUTPUT);
    pinMode(SWL_3, OUTPUT);
    
  }
  else{
    pinMode(SWR_1, INPUT);
    pinMode(SWR_2, INPUT);
    pinMode(SWR_3, INPUT);
    pinMode(SWL_1, INPUT);
    pinMode(SWL_2, INPUT);
    pinMode(SWL_3, INPUT);

  }

  //デバック用
  //ledcWrite(4, 255);
  //while(1);

  timer10ms_p = timerBegin(0, 80, true);
  timerAttachInterrupt(timer10ms_p, &Timer10ms, true);
  timerAlarmWrite(timer10ms_p, 10000, true);
  timerAlarmEnable(timer10ms_p);

  delay(3000);//bluetoothとか起動に時間かかりそうだし少し待つ

  Serial.printf("BDLEDKIT起動　モード%d\n",bdmode);

    if(bdmode == 1){//モード1のループ
    //受信データは文字列で どっちサイドか,フルボトル番号
    //例　右側にラビットを装填する時 R,1\n
    //例　左側にメダルを装填する時　L,85\n
    //例　右側のボトルを取り出す時　R,-1\n
    SerialBT.print("フルボトル疑似装填モード待機\n");
    int8_t tmp = 0;
    while(1){
      if(SerialBT.available()){//何か受信したら
        tmp = SerialBT.read();
        SerialBT.write(tmp);//ターミナルに打った文字が繁栄されるように
        if(tmp == '\n'){//改行で受信完了
          SerialrecSTR[SerialrecCNT] = '\0';
          SerialrecCNT = 0;
          if((SerialrecSTR[0] == 'R' || SerialrecSTR[0] == 'L') && SerialrecSTR[1] == ','){
            tmp = atoi(SerialrecSTR + 2);
            if(tmp == 0){
              SerialBT.print("error\n");
              continue;
            }
            if(SerialrecSTR[0] == 'R'){
              if(tmp == -1) bottlereset(0);
              else {
                bottleset(0,tmp);
                bdstate.rcolor = BottleLib[tmp].color;
                flash0(bdstate.rcolor,blackcolor);
              }
            }
            else if (SerialrecSTR[0] == 'L'){
              if(tmp == -1) bottlereset(1);
              else {
                bottleset(1,tmp);
                bdstate.lcolor = BottleLib[tmp].color;
                flash0(blackcolor, bdstate.lcolor);
              }
            }
          }
          else SerialBT.print("error\n");
        }
        else{
          SerialrecSTR[SerialrecCNT] = tmp;
          SerialrecCNT++;
        }
      } 
      delay(10);
    }
  }

}


void loop() {

  rsw1real = REG_SWR_1;
  rsw3real = REG_SWR_3;
  lsw1real = REG_SWL_1;
  lsw3real = REG_SWL_3;
  swlvreal = REG_SWLV;

  if(rsw1old != rsw1real && rsw1old == 1){//SWR_1立ち下がり

    if(AnalysisCnt == 3 && AnalysisPart == 0 && AnalysisRedyR == true) AnalysisPart = 1;//解析開始

    if(AnalysisPart == 1) AnalysisCode |= (( REG_SWR_2 | (REG_SWR_3 << 1)) <<(AnalysisCnt*2));
//    Serial.printf("%d,%x,%d,%d,%d\n",AnalysisCnt,AnalysisCode,rsw1real,rsw2real,rsw3real);

    if(AnalysisCnt == 0 && AnalysisPart == 1){
        bdstate.Rbottlenum = Encode_Bottle(AnalysisCode);
        bdstate.rcolor = BottleLib[bdstate.Rbottlenum].color;
        AnalysisCode = 0b00000000;
        AnalysisCnt = 3;
        AnalysisPart = 0;
        LoopCnt = 0;

        switch (bdmode) {
          case 0:
            flash0(bdstate.rcolor,blackcolor);
          break;
          case 1:
          break;
          case 2:
            flash0(goldencolor,blackcolor);
          break;
        }
        bdstate.Rset = true;
        AnalysisRedyR = false;
        if(bdstate.Lset == true){
          if(bestmatchcheck(bdstate.Rbottlenum,bdstate.Lbottlenum)) flash4(bdstate.rcolor,bdstate.lcolor);
        }

    }
    else //AnalysisCnt--;

    if(AnalysisPart == 1) AnalysisCnt--;
  }


 if(rsw3old != rsw3real && rsw3old == 1){//SWR_3立ち下がり

    if(AnalysisCnt == 3 && AnalysisPart == 0 && AnalysisRedyR == true) AnalysisPart = 3;//解析開始

    if(AnalysisPart == 3) AnalysisCode |= (( REG_SWR_1 | (REG_SWR_2 << 1)) <<(AnalysisCnt*2));
//    Serial.printf("%d,%x,%d,%d,%d\n",AnalysisCnt,AnalysisCode,rsw1real,rsw2real,rsw3real);

    if(AnalysisCnt == 0 && AnalysisPart == 3){
        bdstate.Rbottlenum = Encode_Bottle(AnalysisCode);
        bdstate.rcolor = BottleLib[bdstate.Rbottlenum].color;
        AnalysisCode = 0b00000000;
        AnalysisCnt = 3;
        AnalysisPart = 0;
        LoopCnt = 0;

        switch (bdmode) {
          case 0:
            flash0(bdstate.rcolor,blackcolor);
          break;
          case 1:
          break;
          case 2:
            flash0(goldencolor,blackcolor);
          break;
        }
        bdstate.Rset = true;
        AnalysisRedyR = false;
        if(bdstate.Lset == true){
          if(bestmatchcheck(bdstate.Rbottlenum,bdstate.Lbottlenum)) flash4(bdstate.rcolor,bdstate.lcolor);
        }

    }
    else //AnalysisCnt--;

    if(AnalysisPart == 3) AnalysisCnt--;

  }

  if(lsw1old != lsw1real && lsw1old == 1){//SWL_1立ち下がり

    if(AnalysisCnt == 3 && AnalysisPart == 0 && AnalysisRedyL == true) AnalysisPart = 1;//解析開始

    if(AnalysisPart == 1) AnalysisCode |= (( REG_SWL_2 | (REG_SWL_3 << 1)) <<(AnalysisCnt*2));
    
    if(AnalysisCnt == 0 && AnalysisPart == 1){
        bdstate.Lbottlenum = Encode_Bottle(AnalysisCode);
        bdstate.lcolor = BottleLib[bdstate.Lbottlenum].color;
        AnalysisCode = 0b00000000;
        AnalysisCnt = 3;
        AnalysisPart = 0;
        LoopCnt = 0;

        switch (bdmode) {
          case 0:
            flash0(blackcolor, bdstate.lcolor);
          break;
          case 1:
          break;
          case 2:
            flash0(blackcolor,silvercolor);
          break;
        }
        bdstate.Lset = true;
        AnalysisRedyL = false;
        if(bdstate.Rset == true){
          if(bestmatchcheck(bdstate.Rbottlenum,bdstate.Lbottlenum)) flash4(bdstate.rcolor,bdstate.lcolor);
        }

    }
    else //AnalysisCnt--;

    if(AnalysisPart == 1) AnalysisCnt--;
  }

 if(lsw3old != lsw3real && lsw3old == 1){//SWL_3立ち下がり

    if(AnalysisCnt == 3 && AnalysisPart == 0 && AnalysisRedyL == true) AnalysisPart = 3;//解析開始

    if(AnalysisPart == 3) AnalysisCode |= (( REG_SWL_1 | (REG_SWL_2 << 1)) <<(AnalysisCnt*2));

    if(AnalysisCnt == 0 && AnalysisPart == 3){
        bdstate.Lbottlenum = Encode_Bottle(AnalysisCode);
        bdstate.lcolor = BottleLib[bdstate.Lbottlenum].color;
        AnalysisCode = 0b00000000;
        AnalysisCnt = 3;
        AnalysisPart = 0;
        LoopCnt = 0;

        switch (bdmode) {
          case 0:
            flash0(blackcolor, bdstate.lcolor);
          break;
          case 1:
          break;
          case 2:
            flash0(blackcolor,silvercolor);
          break;
        }
        bdstate.Lset = true;
        AnalysisRedyL = false;
        if(bdstate.Rset == true){
          if(bestmatchcheck(bdstate.Rbottlenum,bdstate.Lbottlenum)) flash4(bdstate.rcolor,bdstate.lcolor);
        }

    }
    else //AnalysisCnt--;

    if(AnalysisPart == 3) AnalysisCnt--;

  }

  if(LoopCnt > 500){
    AnalysisCode = 0b00000000;
    AnalysisCnt = 3;
    AnalysisPart = 0;
    LoopCnt = 0;
    if(bdstate.Rset == false && AnalysisRedyR == false) AnalysisRedyR = true;
    if(bdstate.Lset == false && AnalysisRedyL == false) AnalysisRedyL = true;
    
  }

  

  if(bdstate.Rset && bdstate.Lset && swlvold != swlvreal && swlvold == 0){
    bdstate.henshinprocess++;//変身開始(レバースイッチon)

    switch (bdmode) {
	  case 0:
      flash1(bdstate.rcolor,bdstate.lcolor);
		break;
	  case 1:
		break;
	  case 2:
      flash1(goldencolor,silvercolor);
		break;
    }

    bdstate.henshinprocess++;//トンテンカン終了

    switch (bdmode) {
	  case 0:
      flash2(bdstate.rcolor,bdstate.lcolor);
		break;
	  case 1:
		break;
	  case 2:
      flash2(goldencolor,silvercolor);
		break;
    }

    bdstate.henshinprocess++;//きゅいんきゅいーーん終了

    //ラビットドラゴンモードならベストマッチを鳴らすため状態を送信する。
    if(bdmode == 2) SerialBT.printf("%d",bdstate.henshinprocess);

    switch (bdmode) {
	  case 0:
      flash3(bdstate.rcolor,bdstate.lcolor);
		break;
	  case 1:
		break;
	  case 2:
      flash3(goldencolor,silvercolor);
		break;
    }

    bdstate.henshinprocess++;//変身プロセス完了

  }

  //ボトル装填状態を解除
  if(swr1.hold == 0 && swr2.hold == 0 && swr3.hold == 0){
    bdstate.Rset = false;
    bdstate.henshinprocess = 0;
  }
  if(swl1.hold == 0 && swl2.hold == 0 && swl3.hold == 0){
     bdstate.Lset = false;
     bdstate.henshinprocess = 0;
  }

  LoopCnt++;

  rsw1old = rsw1real;
  rsw3old = rsw3real;
  lsw1old = lsw1real;
  lsw3old = lsw3real;
  swlvold = swlvreal;

  delay(1);
  //Serial.printf("%d,%d,%d,%d.%d,%d\n",swr1.hold,swr2.hold,swr3.hold,swl1.hold,swl2.hold,swl3.hold);
  
}



void flash0(RGB R, RGB L){
    lc(R,L,50,20);
    lc(R,L,50,40);
    lc(R,L,50,60);
    lc(R,L,50,80);
    lc(R,L,300,100);
    lc(R,L,150,0);
    lc(R,L,150,100);
    lc(R,L,150,0);
    lc(R,L,200,100);
    lc(R,L,50,80);
    lc(R,L,50,70);
    lc(R,L,50,60);
    lc(R,L,50,50);
    lc(R,L,50,40);
    lc(R,L,50,30);
    lc(R,L,50,20);
    lc(R,L,50,10);
    lc(R,L,50,0);
}
void flash1(RGB R, RGB L){
    lc(R,blackcolor,400,100);
    lc(blackcolor,L,400,100);
    lc(R,blackcolor,400,100);
    lc(blackcolor,L,400,100);
    lc(R,blackcolor,400,100);
    lc(blackcolor,L,400,100);
    lc(R,blackcolor,400,100);
    lc(blackcolor,L,400,100);
    lc(R,blackcolor,400,100);
    lc(blackcolor,L,400,100);
    lc(R,blackcolor,400,100);
    lc(blackcolor,L,400,100);
    lc(R,blackcolor,400,100);
    lc(blackcolor,L,400,100);
    lc(R,blackcolor,400,100);
    lc(blackcolor,L,400,100);
    lc(R,blackcolor,400,100);
    lc(blackcolor,L,400,100);
    lc(R,blackcolor,400,100);
    lc(blackcolor,L,400,100);
}
void flash2(RGB R, RGB L){
    lc(R,L,200,0);
    lc(R,L,20,10);
    lc(R,L,20,20);
    lc(R,L,20,30);
    lc(R,L,20,40);
    lc(R,L,20,50);
    lc(R,L,20,60);
    lc(R,L,20,70);
    lc(R,L,20,80);
    lc(R,L,20,90);
    lc(R,L,200,100);
    lc(R,L,200,0);
    lc(R,L,20,10);
    lc(R,L,20,20);
    lc(R,L,20,30);
    lc(R,L,20,40);
    lc(R,L,20,50);
    lc(R,L,20,60);
    lc(R,L,20,70);
    lc(R,L,20,80);
    lc(R,L,20,90);
    lc(R,L,200,100);
}
void flash3(RGB R, RGB L){
    lc(R,L,100,0);
    lc(R,L,50,10);
    lc(R,L,50,20);
    lc(R,L,50,30);
    lc(R,L,50,40);
    lc(R,L,50,50);
    lc(R,L,50,60);
    lc(R,L,50,70);
    lc(R,L,50,80);
    lc(R,L,50,90);
    lc(R,L,300,100);
    lc(R,L,20,90);
    lc(R,L,20,80);
    lc(R,L,20,70);
    lc(R,L,20,60);
    lc(R,L,20,50);
    lc(R,L,20,40);
    lc(R,L,20,30);
    lc(R,L,20,20);
    lc(R,L,20,10);

    lc(R,L,100,0);
    lc(R,L,50,10);
    lc(R,L,50,20);
    lc(R,L,50,30);
    lc(R,L,50,40);
    lc(R,L,50,50);
    lc(R,L,50,60);
    lc(R,L,50,70);
    lc(R,L,50,80);
    lc(R,L,50,90);
    lc(R,L,300,100);
    lc(R,L,20,90);
    lc(R,L,20,80);
    lc(R,L,20,70);
    lc(R,L,20,60);
    lc(R,L,20,50);
    lc(R,L,20,40);
    lc(R,L,20,30);
    lc(R,L,20,20);
    lc(R,L,20,10);

    lc(R,L,100,0);
    lc(R,L,50,10);
    lc(R,L,50,20);
    lc(R,L,50,30);
    lc(R,L,50,40);
    lc(R,L,50,50);
    lc(R,L,50,60);
    lc(R,L,50,70);
    lc(R,L,50,80);
    lc(R,L,50,90);
    lc(R,L,300,100);
    lc(R,L,20,90);
    lc(R,L,20,80);
    lc(R,L,20,70);
    lc(R,L,20,60);
    lc(R,L,20,50);
    lc(R,L,20,40);
    lc(R,L,20,30);
    lc(R,L,20,20);
    lc(R,L,20,10);

    lc(R,L,100,0);
    lc(R,L,50,10);
    lc(R,L,50,20);
    lc(R,L,50,30);
    lc(R,L,50,40);
    lc(R,L,50,50);
    lc(R,L,50,60);
    lc(R,L,50,70);
    lc(R,L,50,80);
    lc(R,L,50,90);
    lc(R,L,300,100);
    lc(R,L,20,90);
    lc(R,L,20,80);
    lc(R,L,20,70);
    lc(R,L,20,60);
    lc(R,L,20,50);
    lc(R,L,20,40);
    lc(R,L,20,30);
    lc(R,L,20,20);
    lc(R,L,20,10);

    lc(R,L,100,0);
    lc(R,L,50,10);
    lc(R,L,50,20);
    lc(R,L,50,30);
    lc(R,L,50,40);
    lc(R,L,50,50);
    lc(R,L,50,60);
    lc(R,L,50,70);
    lc(R,L,50,80);
    lc(R,L,50,90);
    lc(R,L,300,100);
    lc(R,L,20,90);
    lc(R,L,20,80);
    lc(R,L,20,70);
    lc(R,L,20,60);
    lc(R,L,20,50);
    lc(R,L,20,40);
    lc(R,L,20,30);
    lc(R,L,20,20);
    lc(R,L,20,10);

    lc(R,L,100,0);
    lc(R,L,50,10);
    lc(R,L,50,20);
    lc(R,L,50,30);
    lc(R,L,50,40);
    lc(R,L,50,50);
    lc(R,L,50,60);
    lc(R,L,50,70);
    lc(R,L,50,80);
    lc(R,L,50,90);
    lc(R,L,300,100);
    lc(R,L,20,90);
    lc(R,L,20,80);
    lc(R,L,20,70);
    lc(R,L,20,60);
    lc(R,L,20,50);
    lc(R,L,20,40);
    lc(R,L,20,30);
    lc(R,L,20,20);
    lc(R,L,20,10);

    lc(R,L,100,0);
    lc(R,L,50,10);
    lc(R,L,50,20);
    lc(R,L,50,30);
    lc(R,L,50,40);
    lc(R,L,50,50);
    lc(R,L,50,60);
    lc(R,L,50,70);
    lc(R,L,50,80);
    lc(R,L,50,90);
    lc(R,L,300,100);
    lc(R,L,20,90);
    lc(R,L,20,80);
    lc(R,L,20,70);
    lc(R,L,20,60);
    lc(R,L,20,50);
    lc(R,L,20,40);
    lc(R,L,20,30);
    lc(R,L,20,20);
    lc(R,L,20,10);

    //ここから最後のキメッ
    lc(R,L,200 , 0  ); 
    lc(R,L,20  , 10 ); 
    lc(R,L,20  , 20 ); 
    lc(R,L,20  , 30 ); 
    lc(R,L,20  , 40 ); 
    lc(R,L,20  , 50 ); 
    lc(R,L,20  , 60 ); 
    lc(R,L,20  , 70 ); 
    lc(R,L,20  , 80 ); 
    lc(R,L,20  , 90 ); 
    lc(R,L,400 , 100); 
    lc(R,L,200 , 0  ); 
    lc(R,L,200 , 100); 
    lc(R,L,200 , 0  ); 
    lc(R,L,200 , 100); 
    lc(R,L,200  , 0  ); 
    lc(R,L,20  , 10 ); 
    lc(R,L,20  , 20 ); 
    lc(R,L,20  , 30 ); 
    lc(R,L,20  , 40 ); 
    lc(R,L,20  , 50 ); 
    lc(R,L,20  , 60 ); 
    lc(R,L,20  , 70 ); 
    lc(R,L,20  , 80 ); 
    lc(R,L,20  , 90 ); 
    lc(R,L,20  , 100); 
    lc(R,L,5000, 100); 
    lc(R,L,30  , 95 ); 
    lc(R,L,30  , 90 ); 
    lc(R,L,30  , 85 ); 
    lc(R,L,30  , 80 ); 
    lc(R,L,30  , 75 ); 
    lc(R,L,30  , 70 ); 
    lc(R,L,30  , 65 ); 
    lc(R,L,30  , 60 ); 
    lc(R,L,30  , 55 ); 
    lc(R,L,30  , 50 ); 
    lc(R,L,30  , 45 ); 
    lc(R,L,30  , 40 ); 
    lc(R,L,30  , 35 ); 
    lc(R,L,30  , 30 ); 
    lc(R,L,30  , 25 ); 
    lc(R,L,30  , 20 ); 
    lc(R,L,30  , 10 ); 
    lc(R,L,30  , 5  ); 
    lc(R,L,30  , 0  );  


}

void flash4(RGB R, RGB L){
    lc(R,L,350,0);
    lc(R,L,20,50);
    lc(R,L,700,100);      
    lc(R,L,30  , 95 ); 
    lc(R,L,30  , 90 ); 
    lc(R,L,30  , 85 ); 
    lc(R,L,30  , 80 ); 
    lc(R,L,30  , 75 ); 
    lc(R,L,30  , 70 ); 
    lc(R,L,30  , 65 ); 
    lc(R,L,30  , 60 ); 
    lc(R,L,30  , 55 ); 
    lc(R,L,30  , 50 ); 
    lc(R,L,30  , 45 ); 
    lc(R,L,30  , 40 ); 
    lc(R,L,30  , 35 ); 
    lc(R,L,30  , 30 ); 
    lc(R,L,30  , 25 ); 
    lc(R,L,30  , 20 ); 
    lc(R,L,30  , 10 ); 
    lc(R,L,30  , 5  ); 
    lc(R,L,30  , 0  );  
}
