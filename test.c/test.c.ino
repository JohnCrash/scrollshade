void setup() {
  // put your setup code here, to run once:
  pinMode(A0,INPUT_PULLUP);
  pinMode(3,OUTPUT);
  Serial.begin(9600);
}

void loop() {
  // put your main code here, to run repeatedly:
  if(digitalRead(A0)==HIGH){
    digitalWrite(3,HIGH);
    Serial.print("HIGH\n");
  }else{
    digitalWrite(3,LOW);
    Serial.print("LOW\n");
  }
  delay(100);
}
