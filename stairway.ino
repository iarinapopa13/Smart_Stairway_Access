#include <Servo.h>

Servo servoUsa;

// pini pentru toate componentele
const int trigPin = 7;
const int echoPin = 12;
const int servoPin = 8;
const int butonPin = 4;
const int buzzerPin = 3;
const int senzorApaPin = A1;
const int led1 = 5;
const int led2 = 6;
const int led3 = 11;

// setari usa
const int pozitieInchis = 25;
const int pozitieDeschis = 90;
const int distantaActivare = 5;
const int timpDeschidere = 5000;

// senzor apa
const int pragApa = 300;

// variabile usa
int distanta;
bool usaDeschisa = false;
unsigned long timpUltimaDeschidere = 0;

// servo lent
bool servoInMiscare = false;
int pozitieActualaServo = 25;
int pozitieTargetServo = 25;
unsigned long timpUltimPasServo = 0;
const int delayServo = 50;

// buton
bool stareButonAnt = HIGH;
unsigned long timpUltimaApasare = 0;

// fade leduri
bool fadeActiv = false;
int fazaFade = 0;
unsigned long timpUltimPasFade = 0;
int valoriFade[3] = {0, 0, 0};
unsigned long timpStartFaza = 0;

// alarma apa
bool alarmaApaActiva = false;
bool buzzerContinuu = false;
bool alarmaSifortat = false;

void setup() {
  Serial.begin(9600);
  
  servoUsa.attach(servoPin);
  servoUsa.write(pozitieInchis);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(butonPin, INPUT_PULLUP);
  pinMode(buzzerPin, OUTPUT);
  pinMode(led1, OUTPUT);
  pinMode(led2, OUTPUT);
  pinMode(led3, OUTPUT);
  
  stingeLEDuri();
  
  Serial.println("Smart Home System v1.0");
  Serial.println("Buton: LED-uri cu fade");
  Serial.println("Usa: deschidere automata");
  Serial.println("Apa: alarma de urgenta");
  
  // bip pornire
  for(int i = 0; i < 3; i++) {
    digitalWrite(buzzerPin, HIGH);
    delay(100);
    digitalWrite(buzzerPin, LOW);
    delay(100);
  }
  
  Serial.println("Sistem gata!");
}

void loop() {
  masoareDistanta();
  gestioneazaUsa();
  gestioneazaButon();
  monitorizeazaApa();
  procesareFade();
  procesareServoLent();
  
  delay(20);
}

void masoareDistanta() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long durata = pulseIn(echoPin, HIGH);
  distanta = durata * 0.034 / 2;
}

void gestioneazaUsa() {
  // debug periodic
  static unsigned long ultimulDebug = 0;
  if (millis() - ultimulDebug > 5000) {
    Serial.print("Distanta: ");
    Serial.print(distanta);
    Serial.print(" cm | Usa: ");
    Serial.println(usaDeschisa ? "deschisa" : "inchisa");
    ultimulDebug = millis();
  }
  
  // prioritate alarma apa
  if (alarmaApaActiva) {
    if (!usaDeschisa && !servoInMiscare) {
      Serial.println("EVACUARE: deschid usa");
      deschideUsaLent();
      usaDeschisa = true;
      timpUltimaDeschidere = millis() + 3600000;
    }
    return;
  }
  
  // functionare normala
  if (distanta <= distantaActivare && !usaDeschisa) {
    deschideUsaLent();
    usaDeschisa = true;
    timpUltimaDeschidere = millis();
    
    if (!buzzerContinuu) {
      digitalWrite(buzzerPin, HIGH);
      delay(200);
      digitalWrite(buzzerPin, LOW);
    }
  }
  
  if (usaDeschisa && (millis() - timpUltimaDeschidere > timpDeschidere)) {
    inchideUsaLent();
    usaDeschisa = false;
    
    if (!buzzerContinuu) {
      digitalWrite(buzzerPin, HIGH);
      delay(100);
      digitalWrite(buzzerPin, LOW);
    }
  }
}

void gestioneazaButon() {
  bool stareButon = digitalRead(butonPin);
  
  if (stareButon == LOW && stareButonAnt == HIGH) {
    if (millis() - timpUltimaApasare > 200) {
      
      // opreste alarma definitiv
      if (alarmaApaActiva) {
        Serial.println("BUTON: opresc alarma fortat");
        
        alarmaApaActiva = false;
        buzzerContinuu = false;
        alarmaSifortat = true;
        digitalWrite(buzzerPin, LOW);
        
        stingeLEDuri();
        fadeActiv = false;
        fazaFade = 0;
        
        if (usaDeschisa) {
          Serial.println("Inchid usa si revin la normal");
          inchideUsaLent();
          usaDeschisa = false;
        }
        
        // bip confirmare
        delay(200);
        for(int i = 0; i < 3; i++) {
          digitalWrite(buzzerPin, HIGH);
          delay(150);
          digitalWrite(buzzerPin, LOW);
          delay(150);
        }
        
        Serial.println("Alarma oprita - mod normal activ");
        
        timpUltimaApasare = millis();
        stareButonAnt = stareButon;
        return;
      }
      
      // porneste ledurile
      if (!fadeActiv) {
        startFade();
        timpUltimaApasare = millis();
      }
    }
  }
  
  stareButonAnt = stareButon;
}

void monitorizeazaApa() {
  static unsigned long ultimaVerificare = 0;
  if (millis() - ultimaVerificare < 1000) return;
  ultimaVerificare = millis();
  
  int nivelApa = analogRead(senzorApaPin);
  
  // debug apa
  static unsigned long ultimulDebugApa = 0;
  if (millis() - ultimulDebugApa > 3000) {
    Serial.print("Apa: ");
    Serial.print(nivelApa);
    if (alarmaSifortat) {
      Serial.println(" - alarma dezactivata");
    } else if (alarmaApaActiva) {
      Serial.println(" - ALARMA ACTIVA");
    } else if (nivelApa >= pragApa) {
      Serial.println(" - peste prag");
    } else {
      Serial.println(" - normal");
    }
    ultimulDebugApa = millis();
  }
  
  if (!alarmaSifortat) {
    if (nivelApa >= pragApa) {
      if (!alarmaApaActiva) {
        // ALARMA!
        alarmaApaActiva = true;
        buzzerContinuu = true;
        
        Serial.println("!!! ALARMA APA !!!");
        Serial.print("Nivel: ");
        Serial.println(nivelApa);
        
        digitalWrite(buzzerPin, HIGH);
        
        analogWrite(led1, 255);
        analogWrite(led2, 255);
        analogWrite(led3, 255);
        
        fadeActiv = false;
      }
    } else {
      if (alarmaApaActiva) {
        // opreste alarma natural
        alarmaApaActiva = false;
        buzzerContinuu = false;
        digitalWrite(buzzerPin, LOW);
        stingeLEDuri();
        
        Serial.println("Apa disparuta - alarma oprita");
        
        delay(200);
        digitalWrite(buzzerPin, HIGH);
        delay(500);
        digitalWrite(buzzerPin, LOW);
      }
    }
  } else {
    // alarma oprita fortat
    if (nivelApa < pragApa) {
      alarmaSifortat = false;
      Serial.println("Apa disparuta - alarma reactivata");
    }
  }
}

void startFade() {
  if (alarmaApaActiva) return;
  
  fadeActiv = true;
  fazaFade = 1;
  timpUltimPasFade = millis();
  timpStartFaza = millis();
  
  valoriFade[0] = 0;
  valoriFade[1] = 0;
  valoriFade[2] = 0;
  
  Serial.println("Pornesc fade LED-uri");
  
  if (!buzzerContinuu) {
    for(int i = 0; i < 2; i++) {
      digitalWrite(buzzerPin, HIGH);
      delay(100);
      digitalWrite(buzzerPin, LOW);
      delay(100);
    }
  }
}

void procesareFade() {
  if (alarmaApaActiva && fadeActiv) {
    fadeActiv = false;
    return;
  }
  
  if (!fadeActiv) return;
  
  if (millis() - timpUltimPasFade < 10) return;
  timpUltimPasFade = millis();
  
  switch(fazaFade) {
    case 1: fazaAprindere(); break;
    case 2: fazaMaxim(); break;
    case 3: fazaStingere(); break;
  }
}

void fazaAprindere() {
  bool gata = true;
  
  if (valoriFade[0] < 255) {
    valoriFade[0] += 5;
    if (valoriFade[0] > 255) valoriFade[0] = 255;
    analogWrite(led1, valoriFade[0]);
    gata = false;
  }
  
  if (valoriFade[0] >= 50 && valoriFade[1] < 255) {
    valoriFade[1] += 5;
    if (valoriFade[1] > 255) valoriFade[1] = 255;
    analogWrite(led2, valoriFade[1]);
    gata = false;
  }
  
  if (valoriFade[0] >= 100 && valoriFade[2] < 255) {
    valoriFade[2] += 5;
    if (valoriFade[2] > 255) valoriFade[2] = 255;
    analogWrite(led3, valoriFade[2]);
    gata = false;
  }
  
  if (gata) {
    fazaFade = 2;
    timpStartFaza = millis();
  }
}

void fazaMaxim() {
  if (millis() - timpStartFaza >= 1500) {
    fazaFade = 3;
    timpStartFaza = millis();
  }
}

void fazaStingere() {
  bool gata = true;
  
  if (valoriFade[2] > 0) {
    valoriFade[2] -= 5;
    if (valoriFade[2] < 0) valoriFade[2] = 0;
    analogWrite(led3, valoriFade[2]);
    gata = false;
  }
  
  if (valoriFade[2] <= 50 && valoriFade[1] > 0) {
    valoriFade[1] -= 5;
    if (valoriFade[1] < 0) valoriFade[1] = 0;
    analogWrite(led2, valoriFade[1]);
    gata = false;
  }
  
  if (valoriFade[2] <= 25 && valoriFade[0] > 0) {
    valoriFade[0] -= 5;
    if (valoriFade[0] < 0) valoriFade[0] = 0;
    analogWrite(led1, valoriFade[0]);
    gata = false;
  }
  
  if (gata) {
    fadeActiv = false;
    fazaFade = 0;
    stingeLEDuri();
    Serial.println("Fade complet");
  }
}

void deschideUsaLent() {
  pozitieTargetServo = pozitieDeschis;
  servoInMiscare = true;
  Serial.println("Deschid usa...");
}

void inchideUsaLent() {
  pozitieTargetServo = pozitieInchis;
  servoInMiscare = true;
  Serial.println("Inchid usa...");
}

void procesareServoLent() {
  if (!servoInMiscare) return;
  
  if (millis() - timpUltimPasServo < delayServo) return;
  timpUltimPasServo = millis();
  
  if (pozitieActualaServo < pozitieTargetServo) {
    pozitieActualaServo++;
  } else if (pozitieActualaServo > pozitieTargetServo) {
    pozitieActualaServo--;
  } else {
    servoInMiscare = false;
    Serial.println("Servo gata");
    return;
  }
  
  servoUsa.write(pozitieActualaServo);
}

void stingeLEDuri() {
  analogWrite(led1, 0);
  analogWrite(led2, 0);
  analogWrite(led3, 0);
}
