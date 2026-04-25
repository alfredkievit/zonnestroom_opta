# Flash instructie Opta in het veld

Deze handleiding is bedoeld om snel firmware te flashen vanaf je laptop in het veld.

## 1) Voorbereiding

1. Open deze projectmap in VS Code.
2. Sluit 1 Opta via USB aan.
3. Controleer welke COM-poort is toegewezen in Apparaatbeheer.
4. Open een terminal in VS Code.

Opmerking:
- In beide PlatformIO configuraties staat nu standaard COM8.
- Als jouw laptop een andere COM-poort gebruikt, geef die expliciet mee met --upload-port.

## 2) Flash Opta2 (hottub)

Voer uit vanaf de project-root:

    ~/.platformio/penv/Scripts/pio.exe run -d opta2_hottub -t upload

Als COM-poort afwijkt:

    ~/.platformio/penv/Scripts/pio.exe run -d opta2_hottub -t upload --upload-port COM7

## 3) Flash Opta1 (master)

Voer uit vanaf de project-root:

    ~/.platformio/penv/Scripts/pio.exe run -d opta1_master -t upload

Als COM-poort afwijkt:

    ~/.platformio/penv/Scripts/pio.exe run -d opta1_master -t upload --upload-port COM7

## 4) Seriële monitor (optioneel)

Opta2 monitor:

    ~/.platformio/penv/Scripts/pio.exe device monitor -d opta2_hottub -b 115200

Opta1 monitor:

    ~/.platformio/penv/Scripts/pio.exe device monitor -d opta1_master -b 115200

## 5) Snelle check na flash

1. Controleer in Home Assistant/MQTT dat statusberichten weer binnenkomen.
2. Controleer dat temperatuurtopics rustiger zijn geworden.
3. Verwachting: temperatuurupdates nu maximaal ongeveer 1x per 2 minuten.

## 6) Veelvoorkomende problemen

Upload blijft hangen of kan poort niet openen:
1. Sluit seriële monitor af.
2. Controleer of geen andere tool de COM-poort bezet houdt.
3. Trek USB los en sluit opnieuw aan.
4. Probeer opnieuw met expliciete --upload-port COMx.

Fout dat pio.exe niet gevonden wordt:
1. Controleer of PlatformIO is geïnstalleerd in VS Code.
2. Gebruik eventueel de PlatformIO terminal of start VS Code opnieuw.

## 7) Bestandlocaties

- Opta2 config: opta2_hottub/platformio.ini
- Opta1 config: opta1_master/platformio.ini
