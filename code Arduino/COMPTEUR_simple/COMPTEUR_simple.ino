/*Compteur de visites _ PROTOTYPE_V2
janvier2020

Objectif/ description: 
Le projet a pour but d'équiper l'entrée de notre médiathèque d'un compteur de passages, afin de déterminer le nombre de visiteurs quotidien de notre établissement.

Principe:
Dans cette deuxième version du prototype, un seul capteur mesure le passage des individus, sans distinction entre entrées et sorties. La fréquentation est affichée
en temps réel sur écran LCD pour debug/ monitoring. Le total des passages mesurés est divisé par 2 et enregistré toutes les 10 minutes dans un fichier CSV sur carte SD (avec heure et date).

Pour fonctionner, le dispositif est constitué de :
- une carte ARduino Uno
- un shield SD/RTC
- une panel LCD
- un capteur ultrasonique
- des liaisons, résistance et potentiomètre (contraste LCD)

Le capteur ultrason mesure en permanence la distance qui le sépare de l'obstacle le plus proche. Si cette distance est inférieure à 50 cm, le programme considère qu'une personne
se trouve devant le capteur, dans le passage, et comptabilisera une entrée/sortie à partir du moment où le passage sera à nouveau dégagé. Tout simplement !

Les mesures sont enregistrées et horodatées à intervalle régulier via le shield.

Crédits:
La partie du code qui concerne l'acquisition et l'horodatage des données est adaptée du travail de Fabien Batteix (aka. Skywodd), partagé sur son blog carnetdumaker.net

A venir:
La prochaine version du projet devra résoudre la question de la distinction entre entrées et sorties, encore source d'erreurs lors des tests effectués.

@ViKh_*/



/*-- PROGRAMME --*/


/*Dépendances et librairies */
#include <Wire.h> // Pour la communication I2C
#include <SPI.h>  // Pour la communication SPI
#include <SD.h>   // Pour la communication avec la carte SD
#include "DS1307.h" // Pour le module DS1307
#include <LiquidCrystal.h> // Pour le module écran LCD

LiquidCrystal lcd(9, 8, 5, 3, 2, 0);

/* Rétro-compatibilité avec Arduino 1.x et antérieur */
#if ARDUINO >= 100
#define Wire_write(x) Wire.write(x)
#define Wire_read() Wire.read()
#else
#define Wire_write(x) Wire.send(x)
#define Wire_read() Wire.receive()
#endif

/*Broches du capteur ultrasonique*/
#define trigPin_C1 A0
#define echoPin_C1 A1

/*Variables pour la mesure*/

int etat_detecte_C1;
int portique = 50;
int passages = 0;

long duree_C1, distance_C1;

/** Broche CS de la carte SD */
const byte SDCARD_CS_PIN = 10; // ChipSelect du shield utilisé (ici : Velleman VMA202)

/** Nom du fichier de sortie */
const char* OUTPUT_FILENAME = "data.csv";

/** Delai entre deux prises de mesures */
const unsigned long DELAY_BETWEEN_MEASURES = 600000; // délai (modifiable) de 600 000 millisecondes, doit 10mn, entre deux enregistrements sur carte SD


/** Fonction de conversion BCD -> decimal */
byte bcd_to_decimal(byte bcd) {
  return (bcd / 16 * 10) + (bcd % 16); 
}

/** Fonction de conversion decimal -> BCD */
byte decimal_to_bcd(byte decimal) {
  return (decimal / 10 * 16) + (decimal % 10);
}

/** 
 * Fonction ajustant l'heure et la date courante du module RTC à partir des informations fournies.
 * N.B. Redémarre l'horloge du module RTC si nécessaire.
 */
void adjust_current_datetime(DateTime_t *datetime) {
  
  /* Début de la transaction I2C */
  Wire.beginTransmission(DS1307_ADDRESS);
  Wire_write((byte) 0); // Ecriture mémoire à l'adresse 0x00
  Wire_write(decimal_to_bcd(datetime->seconds) & 127); // CH = 0
  Wire_write(decimal_to_bcd(datetime->minutes));
  Wire_write(decimal_to_bcd(datetime->hours) & 63); // Mode 24h
  Wire_write(decimal_to_bcd(datetime->day_of_week));
  Wire_write(decimal_to_bcd(datetime->days));
  Wire_write(decimal_to_bcd(datetime->months));
  Wire_write(decimal_to_bcd(datetime->year));
  Wire.endTransmission(); // Fin de transaction I2C
}

/** 
 * Fonction récupérant l'heure et la date courante à partir du module RTC.
 * Place les valeurs lues dans la structure passée en argument (par pointeur).
 * N.B. Retourne 1 si le module RTC est arrêté (plus de batterie, horloge arrêtée manuellement, etc.), 0 le reste du temps.
 */
byte read_current_datetime(DateTime_t *datetime) {
  
  /* Début de la transaction I2C */
  Wire.beginTransmission(DS1307_ADDRESS);
  Wire_write((byte) 0); // Lecture mémoire à l'adresse 0x00
  Wire.endTransmission(); // Fin de la transaction I2C
 
  /* Lit 7 octets depuis la mémoire du module RTC */
  Wire.requestFrom(DS1307_ADDRESS, (byte) 7);
  byte raw_seconds = Wire_read();
  datetime->seconds = bcd_to_decimal(raw_seconds);
  datetime->minutes = bcd_to_decimal(Wire_read());
  byte raw_hours = Wire_read();
  if (raw_hours & 64) { // Format 12h
    datetime->hours = bcd_to_decimal(raw_hours & 31);
    datetime->is_pm = raw_hours & 32;
  } else { // Format 24h
    datetime->hours = bcd_to_decimal(raw_hours & 63);
    datetime->is_pm = 0;
  }
  datetime->day_of_week = bcd_to_decimal(Wire_read());
  datetime->days = bcd_to_decimal(Wire_read());
  datetime->months = bcd_to_decimal(Wire_read());
  datetime->year = bcd_to_decimal(Wire_read());
  
  /* Si le bit 7 des secondes == 1 : le module RTC est arrêté */
  return raw_seconds & 128;
}

/** Fichier de sortie avec les mesures */
File file;


// fonction setup
void setup() {
  
  /* Initialise le port I2C */
  Wire.begin();

/*Initialise le LCD*/
  lcd.begin(16, 2);
  lcd.print(F("MEDIATEK DU RIZE :")); // Message fixe de la 1er ligne du LCD, à personnaliser !

 
/* Vérifie si le module RTC est initialisé */
  DateTime_t now;
  
  /*if (read_current_datetime(&now)) {
    //Serial.println(F("L'horloge du module RTC n'est pas active !")); //(décommenter au besoin, pour debug)
    
    // Reconfiguration avec une date et heure en dur (modifier avec l'heure précise et la date du jour)
   / now.seconds = 0;
    now.minutes = 0;
    now.hours = 12; // 12h 00min 0sec
    now.is_pm = 0; 
    now.day_of_week = 2;
    now.days = 2; 
    now.months = 7;
    now.year = 19; // 2 juillet 2019
    adjust_current_datetime(&now);
  }

  /* Initialisation du port SPI */
  pinMode(10, OUTPUT); // Arduino UNO

  /* Initialisation de la carte SD */
  if (!SD.begin(SDCARD_CS_PIN)) {
    for (;;); // Attend appui sur bouton RESET
  }

  /* Ouvre le fichier de sortie en écriture */
  file = SD.open(OUTPUT_FILENAME, FILE_WRITE);
  if (!file) {
    for (;;); // Attend appui sur bouton RESET
  }
  
  /* Ajoute l'entête CSV si le fichier est vide */
  if (file.size() == 0) {
    file.println(F("DATE; HEURE; VISITES;"));
    file.flush();
  }

  /* Assigne les broches des capteurs ultrasons C1 */ 
  pinMode(trigPin_C1, OUTPUT);
  pinMode(echoPin_C1, INPUT);
  digitalWrite(trigPin_C1, LOW);
  }

void loop() {

 DateTime_t now;
  if (read_current_datetime(&now)) {
    //Serial.println(F("L'horloge du module RTC n'est pas active !")); (décommenter au besoin, pour debug)
  }
 
   // envoi et reception de l'onde
  digitalWrite(trigPin_C1, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin_C1, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin_C1, LOW);
   duree_C1 = pulseIn(echoPin_C1, HIGH);

// calcul de la distance
  distance_C1 = (duree_C1/2) / 29.1;

  
 // DETECTION PASSAGES
   
   if((distance_C1 > portique) && (etat_detecte_C1 == 0)) { 
    etat_detecte_C1 = 0;
    delay(150);
    }
  
  if((distance_C1 < portique) && (etat_detecte_C1 == 0)) { 
    etat_detecte_C1 = 1;
    delay(150);
    }
  
  if((distance_C1 < portique) && (etat_detecte_C1 == 1)) { 
    etat_detecte_C1 = 1;
    delay(150);
    } 
  
  if((distance_C1 > portique) && (etat_detecte_C1 == 1)) {  
    etat_detecte_C1 = 0;
    passages = ++passages;
    delay(150);
    }


/* Affiche sur le LCD les données de fréquentation */
  lcd.setCursor(0, 1);
  lcd.print(F("Passages : "));
  lcd.print(passages);
  
  // Temps de la précédente mesure et actuel
  static unsigned long previousMillis = 0;
  unsigned long currentMillis = millis();

  /* Réalise une prise de mesure toutes les DELAY_BETWEEN_MEASURES millisecondes */
  if (currentMillis - previousMillis >= DELAY_BETWEEN_MEASURES) {
    previousMillis = currentMillis;
    measure();
  }
 }

/** Fonction de mesure - à personnaliser selon ses besoins */
void measure() {
  
  /* Lit la date et heure courante */
  DateTime_t now;
  read_current_datetime(&now);
  
  /* Enregistre les données sur la carte SD */
  // Horodatage
  file.print(now.days);
  file.print(F("/"));
  file.print(now.months);
  file.print(F("/"));
  file.print(now.year + 2000);
  file.print(F("; "));
  file.print(now.hours);
  file.print(F(":"));
  file.print(now.minutes);
  file.print(F(":"));
  file.print(now.seconds);
  file.print(F("; "));
  file.print(passages/2);
  file.print(F("\n"));
  file.flush();
}
