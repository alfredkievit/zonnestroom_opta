# Archief

Historische documentatie, gekoppeld aan werk dat inmiddels is afgerond en gemerged. Bewaard als referentie, niet als actuele instructie — zie de root [README.md](../../README.md) en [CHANGELOG.md](../../CHANGELOG.md) voor de huidige stand van zaken.

| Bestand | Herkomst |
|---|---|
| `IMPLEMENTATIEPLAN.md` | Oorspronkelijk implementatieplan en commissioning-log uit de eerste opzetfase (2026-03/05). De architectuur- en regelfilosofie-beschrijving is nog steeds accuraat en staat nu ook in de root README; de "open punten"/TODO-status is achterhaald. |
| `MQTT_TIMEOUT_FIX.md`, `VERIFICATION_CHECKLIST.md`, `FIELD_DEPLOYMENT_GUIDE.md`, `mqtt_timeout_hysteresis.patch` | Documentatiebundel voor de MQTT-timeout-hysteresefix (juli 2026), inmiddels gemerged en live. Zie CHANGELOG voor de samenvatting. |
| `COMMIT_AND_PUSH.ps1`, `COMMIT_AND_PUSH.sh` | Eenmalig commit-script met een hardcoded commitboodschap voor bovenstaande fix. Niet opnieuw gebruiken — normale `git add`/`git commit`/`git push` volstaat. |
| `UPDATE_FIRMWARE_VELD.ps1` | Onafgemaakt script (placeholder GitHub-URL is nooit ingevuld) voor het clonen+builden in het veld. Zie in plaats daarvan [`docs/FLASHEN_OPTA_VELD.md`](../FLASHEN_OPTA_VELD.md) voor de huidige, geverifieerde veldprocedure. |
