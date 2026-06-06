# Monthly Reactive Owner Test Playbook

## Ucel

Tento playbook rika presne, kdy a jak provest prvni realny Stellaris test pro published monthly reactive branch Strategic Nexus v0.

Nejde o plnou validaci strategicke kvality.
Jde o fail-closed overeni, ze publikovana monthly reactive vetev je v bezne session viditelna pres harmless markery a ze Codex dostane spravne nasledne artefakty.

## Kdy Test Spustit

Test spoustej az kdyz je splneno vsechno niz:

1. `StrategicNexusCompanion` nebo `tools/run_real_session_v0_loop.ps1` hlasi `next_action=run_monthly_reactive_owner_test`.
2. Duvod je `published_monthly_reactive_overlay_ready_for_owner_test`.
3. Aktivni generated overlay uz je publikovany, ne jen staged.
4. Gameplay acceptance zustava overena v `dist/private_reports/generated_overlay_gameplay_acceptance_v0.json`.

Pokud nektery z techto bodu chybi, test nespoustej a ber stav jako nepripraveny.

## Co Presne Udelat

1. Spust Stellaris s aktualnim Strategic Nexus PoC modem a publikovanym generated overlay snapshotem.
2. Otevri nebo pokracuj v realne non-Ironman session, kde je bezpecne sledovat dalsi mesicni pulse.
3. Nech hru dobehnout alespon do dalsiho monthly pulse.
4. Sleduj, jestli se projevi viditelne harmless markery z publikovane monthly reactive vetve.
5. Po zachyceni markeru hru normalne ukonci a nech Codex zkontrolovat artefakty.

## Ocekavane Viditelne Markery

Tato sada je zamerne gameplay-neutralni. Cil je potvrdit branch visibility, ne menit realnou silu rise.

* `Strategic Nexus v0: Defensive military posture`
* `Strategic Nexus v0: Aggressive military posture`
* `Strategic Nexus v0: Economy research bias`
* `Strategic Nexus v0: Military industry research bias`

## Co Ma Codex Zkontrolovat Potom

Po testu ma Codex zkontrolovat hlavne tyto artefakty:

* `dist/private_reports/snc_generated_overlay_publish_status.json`
* `dist/private_reports/generated_overlay_gameplay_acceptance_v0.json`
* posledni `dist/real_session_v0_loop/<session_id>/real_session_v0_loop_evidence.json`
* posledni `dist/real_session_v0_loop/<session_id>/real_session_v0_next_steps.md`
* `Documents/Paradox Interactive/Stellaris/logs/error.log`

U real-session evidence je dulezite, aby zustalo videt:

* `next_action=run_monthly_reactive_owner_test`
* `next_action_reason=published_monthly_reactive_overlay_ready_for_owner_test`
* `snc_owner_test_contract.ready=true`

## Pass

Test ber jako uspesny, kdyz plati vsechno niz:

* monthly reactive owner-test contract byl pred session opravdu ready
* po monthly pulse se projevi ocekavany harmless marker
* `error.log` neobsahuje Strategic Nexus chyby pro generated overlay / monthly dispatch path
* publish status a gameplay acceptance artefakty po testu porad odpovidaji published a verified stavu

## Fail

Test ber jako neuspesny, kdyz nastane cokoli z toho:

* contract pred testem nebyl ready, ale session se presto zkousela jako owner test
* po dalsim monthly pulse se neobjevi zadny ocekavany harmless marker
* `error.log` ukazuje script/event/effect chybu na Strategic Nexus monthly reactive ceste
* published/generated overlay evidence po testu neodpovida ready stavu, ktery byl podminkou testu

## Omezeni

Tento test zatim dokazuje jen to, ze publikovana monthly reactive branch je viditelna a bezela v session pres harmless markery.

Nedokazuje:

* ze dlouhodoba strategicka volba je kvalitni
* ze rise dela optimalni rozhodnuti
* ze je pokryta cela multiplayer/hotjoin kontinuita
* ze dalsi generated pravidla mimo tuto bounded sadu nemaji regresi
