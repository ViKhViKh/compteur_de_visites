/*                                    ******************************
                                      *  Compteurs de visites V1.0 *
                                      ******************************

******************************************************************************************************************************
CETTE VERSION DU SKETCH N'EST QUE LE POINT DE DEPART DE MON PROJET. UNE PROCHAINE VERSION PERMETTRA :

- d'enregistrer les valeurs dans une mémoire EEPROM, pour ne pas perdre le cumul total des données relevées à chaque reset et 
à chaque ouverture du moniteur série;
- d'automatiser la relève du compteur et d'enregistrer les données horodatées dans un fichier CSV sur carte SD, en vue d'être 
traitées dans un tableur pour fournir des statistiques suivies de fréquentation.

******************************************************************************************************************************
DESCRIPTION :

Dans cette version, le compteur utilise un capteur à ultrasons pour mesurer le passage des personnes. Ce n'est sans doute pas 
la meilleure option (les compteurs du commerce fonctionnent généralement avec de l'infrarouge), mais c'est tout ce que j'avais 
sous la main et le résultat est finalement plutôt bon.

Pour ce qui est du fonctionnement du dispositif dans cette version : un capteur à ultrasons est disposé dans une zone de 
passage ; l'arduino interroge le capteur et certaines conditions : si la distance est inférieure à la largeur du passage(80cm 
dans mon cas), l'état de détection est activé ("etat detecte = 1"), sans que le compteur ne soit incrémenté.
Presque aussitôt (200ms), l'arduino réinterroge le capteur et les conditions : si l'état de détection est activé et la distance 
toujours inférieure à la largeur du passage, alors quelqu'un se trouve toujours devant le capteur et le compteur ne sera pas 
incrémenté. En revanche, si l'état de détection est activé mais la distance supérieure ou égale à la largeur du passage, alors 
quelque chose a changé : une personne qui se trouvait devant le capteur ne s'y trouve plus, le compteur est incrémenté
et l'état de détection est désactivé ("état detecte = 0"). ETC.

A vous d'adapter la distance mesurée par le capteur, le laps de temps entre interrogements du capteur par l'arduino et 
éventuellement le choix des variables à afficher dans le moniteur série et sur l'écran LCD (pour ma part, la variable 'passages' 
comptabilise les entrées et les sorties (pour debug sur moniteur série), tandis que la variable 'visiteurs' divise le total par 
2 pour n'afficher qu'une représentation des visites sur le LCD).

******************************************************************************************************************************

mars 2019

Victor Kherchaoui
twitter: @ViKh_
mars 2019

*/

#include <LiquidCrystal.h>
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);
#define trigPin 9
#define echoPin 8

int passages;
int visiteurs;
int etat_detecte = 0;

long duration, distance;

void setup() {
  
Serial.begin(9600);
lcd.begin(16, 2);
lcd.print("Visiteurs:");
lcd.setCursor(0,1);
pinMode(trigPin, OUTPUT);
pinMode(echoPin, INPUT);
Serial.println("==Debut du programme==");

}

void loop() {
  // envoi et reception de l'onde
digitalWrite(trigPin, LOW);
delayMicroseconds(2);
digitalWrite(trigPin, HIGH);
delayMicroseconds(10);
digitalWrite(trigPin, LOW);
duration = pulseIn(echoPin, HIGH);

// calcul de la distance
distance = (duration/2) / 29.1;
visiteurs = passages / 2;

// détection des personnes

if((distance < 80) && (etat_detecte == 0)){
    etat_detecte = 1;
    Serial.println(passages);
    lcd.setCursor(0,1);
    lcd.print(visiteurs);
    delay(200);
    }
if((distance > 80) && (etat_detecte == 1)){ 
    etat_detecte = 0;
    passages = ++passages;
    Serial.println(passages);
    lcd.setCursor(0,1);
    lcd.print(visiteurs);
    delay(200);
    }
if((distance > 80) && (etat_detecte == 0)){  
    etat_detecte = 0;
    Serial.println(passages);
    lcd.setCursor(0,1);
    lcd.print(visiteurs);
    delay(200);
    }
if((distance < 80) && (etat_detecte == 1)){
    Serial.println(passages);
    lcd.setCursor(0,1);
    lcd.print(visiteurs);
    delay(200);
    }
   
}
