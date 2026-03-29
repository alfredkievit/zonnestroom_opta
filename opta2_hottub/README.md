# Opta2 Hottub

Beschrijving van de fysieke in- en uitgangen van de Opta2 hottub-controller.

## Functie

Opta2 verzorgt:

- lokale hottub temperatuurregeling
- circulatiepompregeling
- niveaupompregeling
- lokale alarmen en fail-safe gedrag
- watchdog op permissie en heartbeat van Opta1

## Uitgangen

| PLC uitgang | Code pin | Functie | Opmerking |
|---|---|---|---|
| O1 | D0 | Hottub verwarming | Stuurt heater / warmtevraag |
| O2 | D1 | Circulatiepomp | Voor verwarming en geplande filterruns |
| O3 | D2 | Niveaupomp | Voor niveaucorrectie |
| O4 | D3 | Alarm uitgang | Actief bij foutconditie |

## Analoge ingangen

| PLC ingang | Code pin | Functie | Opmerking |
|---|---|---|---|
| I1 | A0 | Hottub temperatuursensor | PT1000 met 0-10V omvormer |

## Digitale ingangen

| PLC ingang | Code pin | Functie | Opmerking |
|---|---|---|---|
| I2 | A1 | Hoog niveau schakelaar | Detecteert hoog waterniveau |
| I3 | A2 | Externe fout | Extern storingssignaal |
| I4 | A3 | Flow OK | Bevestiging van waterflow |

## Bedrijfslogica

Opta2 mag de hottub alleen verwarmen als:

- permissie van Opta1 aanwezig is
- heartbeat van Opta1 geldig blijft
- lokale alarmen dit toelaten

Bij communicatieverlies of fout gaat Opta2 naar veilige toestand.