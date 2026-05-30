# Strategic Nexus - Dev Diary

## Purpose

This document is a daily human-readable development report for Strategic Nexus.

Audience:

* project owner
* interested technical friends
* occasional reviewers
* future Codex sessions

Style:

* Czech language
* concise but understandable
* clear official project status tone
* explain what changed and why it matters
* avoid dumping raw logs
* avoid pretending uncertainty is certainty
* do not include sensitive reverse-engineering, executable internals, or process-integration details

Diary policy:

* treat committed daily entries as append-only project history
* add a new dated entry or amend the current day's uncommitted draft when needed
* do not delete or rewrite older committed diary entries just to "clean up" history
* if an old entry is wrong, add a later correction note instead of silently removing it
* uncommitted stale drafts may be removed when they are inaccurate, duplicated, or no longer match repository reality

The diary should help a reader understand the project without reading every architecture document or commit.

---

# Current Status Snapshot

Strategic Nexus is currently in early v0 architecture work.

Confirmed:

* singleplayer scripted state proof of concept works
* multiplayer scripted state synchronization works
* host-authoritative model is viable at scripted game-state level
* bridge-core validation exists
* v0 deterministic pipeline exists for `X -> Y -> U -> payload -> effect batch`
* the production direction is offline companion app, autosave archiving, local analysis, and next-session generated overlay
* older daemon and bridge work remains useful as validation and development harness infrastructure

Main blocker:

* production gameplay still needs a checksum-safe generated overlay packaging and delivery workflow

Current engineering stance:

* do not implement lower-level game-process modification
* do not expose arbitrary command execution
* keep all LLM output bounded, validated, and compiled through a small DSL
* keep gameplay behavior in ordinary Stellaris mod script and generated overlay files staged between sessions

---

# Daily Entries

Poznamka k casove ose: denik je historicky zaznam prace a popisuje stav uvah v dobe daneho zapisu. Neni to zdroj aktivnich pravidel projektu. Pokud se smer, pravidlo nebo bezpecnostni vyklad pozdeji zmeni, ma se doplnit novy casove ukotveny kontext misto ticheho prepisovani historie.

## 2026-05-29

Projekt dnes zpresnil verejny status companion vrstvy pro multiplayer generated overlay pripravu a soucasne doplnil dokumentacni a vyvojove workflow detaily kolem dalsi implementace.

Co pribylo v repozitari:

* Nejnovnejsi lokalni commit rozsiruje SNC status snapshot o viditelnost stavu MP overlay package, tedy o dalsi potvrzeny stavovy signal na integration boundary mezi offline analyzou, generated overlay pripravenosti a companion vystupem.
* Verejne pushnute commity doplnily definici `Free Work` definition of done v projektove dokumentaci a upravily starsi task board list mode tak, aby zustal neinteraktivni a lepe pouzitelny pro automatizovane vyvojove workflow.
* Nejde o novy runtime zasah do bezici session; jde o zpresneni stavove observability a podpurneho developerskeho workflow kolem existujiciho host-authoritative smeru.

Co to znamena pro architekturu a runtime interoperability research:

* Companion vrstva dal spevnuje roli bezpecneho statusoveho rozhrani mezi archivovanou kampani, offline analyzou a pripravenym next-session generated overlay artefaktem.
* Nova viditelnost MP overlay package stavu zmensuje nejasnost kolem toho, zda je scripted event/effect path pripraven do dalsi session jako validni staged vystup, aniz by se rozsiril produkcni zasah do aktivniho runtime.
* Dokumentacni doplneni definition of done pomaha drzet runtime interoperability research v uzsim a auditovatelnem rozsahu, kde se implementace uzavira pres jasne overitelne vystupy misto volne formulovanych mezistavu.

Testy a stav overeni:

* Pro commit `docs: add Free Work definition of done` (`b345c36`) je na verejnem GitHub Actions videt uspesny beh `Windows Tests #19`, spusteny push udalosti dne 28.5.2026 ve 21:01, s celkovou dobou 6 minut.
* Nejnovejsi commit `SNC status snapshot: include MP overlay package status` (`863a1b3`) je v tuto chvili lokalne o jeden commit pred `origin/master`, takze pro nej jeste neni verejny CI vysledek k dispozici.
* Tento denikovy beh neprovadel novy plny lokalni test run; zapis shrnuje potvrzeny stav z existujicich commit signalu a verejne dohledatelneho CI.

Blokery a rizika:

* Hlavni produkcni blocker se nemeni: stale chybi uzavreny checksum-safe packaging a distribucni workflow pro generated overlay artefakty mezi multiplayer ucastniky.
* Lepsi status snapshot zmensuje diagnostickou nejistotu, ale sam o sobe jeste neuzavira finalni export contract, campaign marker handshake ani empire identity resolver.
* Cast dnesniho postupu sla do observability a developerske discipliny; to je uzitecne pro spolehlivost, ale samo o sobe to jeste neprevadi runtime interoperability research do plneho end-to-end dodani.

Doporuceny dalsi krok:

* Pushnout nejnovnejsi snapshot commit spolu s touto denikovou aktualizaci a potvrdit navazujici verejny `Windows Tests` signal i pro novy status snapshot.
* Potom navazat explicitnim generated overlay export contractem a end-to-end overenim checksum-safe distribuce mezi host-authoritative pripravenou dalsi session a companion artefakty.

## 2026-05-30

Projekt dnes posunul hlavne offline analyzu autosave vstupu, navazani archivniho brief/ledger toku a dalsi tvrdsi validaci generated overlay cesty. Smer zustava stejny: nejde o novy runtime zasah do bezici session, ale o zpevnovani integration boundary mezi archivem, analyzou a pripravenym next-session artefaktem.

Co pribylo v repozitari:

* Pribyl uzce ohraniceny Stellaris save parser CLI a navazujici dokumentace save container formatu. Tato vrstva dava projektu prvni konkretni vstup pro offline save parsing v bezpecnem, ctecim a auditovatelnem rozsahu.
* Nasledne commity propojily parsovanou hlavicku savu a season delta ledger s `ArchiveMinistryInputBuilder`, vcetne threading `parsed_headline_war_hint` a `save_date` year hintu do archivniho brief toku.
* Runtime spine byla zpevnena dalsi validaci archive identity a generated overlay DSL validation, aby scripted event/effect path zustal bounded a fail-closed i pri problematickych vstupech.
* Posledni dnesni commit doplnil warning codes do verify CLI vystupu pro MP package cestu, tedy presnejsi diagnostiku na hranici mezi generated overlay packagingem, companion observability a dalsim export workflow.
* Dokumentace roadmapy a offline analysis architektury byla prubezne upravena tak, aby nove interoperabilni kroky odpovidaly skutecne implementovanemu poradi.

Co to znamena pro architekturu a runtime interoperability research:

* Projekt se posunul od ciste kontraktni pipeline k prvni prakticky pouzitelne ceste `save container -> parser -> archive ministry input -> season delta/brief -> generated overlay staging`.
* Runtime interoperability research je ted konkretnejsi v oblasti cteni autosave struktury a mapovani metadat do host-authoritative offline modelu, aniz by se rozsiril zasah do aktivniho runtime.
* Nove hinty z parsovaneho savu zmensuji mezeru mezi archivovanym stavem kampane a budoucim rozhodovacim/briefing tokem, ale stale zustavaji uvnitr kontrolovaneho integration boundary.
* Presnejsi verify warnings pomahaji odlisit chyby packagingu od pouze degradovanych stavu pripravenosti, coz je dulezite pro pozdejsi checksum-safe distribuci.

Testy a stav overeni:

* Verejne GitHub Actions `Windows Tests` jsou pro posledni pushnute commity, ktere uz byly na `origin/master`, viditelne jako uspesne. Dostupny potvrzeny signal v tomto behu zahrnuje napriklad `Clarify roadmap after save parser slice`, dokonceny 30.5.2026 v 01:40:34 mistniho casu, s dobou behu 4 minuty 25 sekund.
* Nejnovnejsi dnesni implementacni commity jsou v tuto chvili lokalne `ahead 6` oproti `origin/master`, takze pro ne jeste neexistuje verejny CI vysledek.
* Tento denikovy beh nespoustel novy plny lokalni test run; zapis vychazi z commit historie a z dostupneho verejneho CI signalu.

Blokery a rizika:

* Hlavni produkcni blocker se nemeni: stale chybi uzavreny checksum-safe generated overlay packaging a distribucni workflow pro multiplayer ucastniky.
* Save parser a archive ministry threading zmensuji nejistotu vstupnich dat, ale jeste neuzaviraji finalni export contract, campaign marker handshake ani empire identity resolver.
* Cast dnesniho posunu je zamerne vyzkumna a interoperabilni. Rizikem zustava, ze bez navazujiciho end-to-end export overeni budou nove parser a hint vrstvy jen lepe pripravovat vstupy, nikoli jeste potvrzovat konecne doruceni artefaktu.

Doporuceny dalsi krok:

* Zverejnit dnesni lokalni sadu commitu spolu s timto denikovym zapisem a potvrdit navazujici verejny `Windows Tests` signal pro novy head.
* Potom navazat formalnim generated overlay export contractem a prvnim end-to-end overenim checksum-safe distribuce, ktere propoji save parser, archive ministry input a MP package verify cestu.

## 2026-05-28

Projekt dnes dostal prvni overeny vertikalni produktovy pruchod pro offline spine a soucasne presnejsi casove ukotveni companion status snapshotu.

Co pribylo v repozitari:

* Pribyl CLI rezim `--run-offline-spine`, ktery spoji jednu overenou archivni session, season delta ledger, empire brief, explicitne dodany validni DSL, vygenerovany overlay manifest a SNC status snapshot.
* Companion status vrstva nove umi brat konkretni archivni session s `manifest.json` jako pripraveny archivni vstup, nejen nadrazeny archivni adresar se session podslozkami.
* Smoke runner byl rozsiren tak, aby spine kontrola zahrnula i companion test a nevalidovala jen izolovane pipeline komponenty.
* Nejnovnejsi dnesni commit doplnil do SNC status snapshotu lokalni `generated_at_local` cas, aby bylo z verejneho i lokalniho vystupu jasne, kdy byl snapshot skutecne vytvoren.

Co to znamena pro architekturu a runtime interoperability research:

* Stabilizacni smer `verified archive -> season delta ledger -> empire brief -> validated DSL -> generated overlay -> Status Center visibility` uz neni jen roadmapovy popis, ale testovany produktovy pruchod.
* Projekt tim dale upevnuje host-authoritative model s offline companion vrstvou na integracni boundary mezi archivem, analyzou a next-session generated overlay pripravu.
* Casove oznaceni snapshotu zlepsuje dohledatelnost stavu pri ladeni integracni hranice mezi offline archivem, analyzou a pripravenou next-session overlay vrstvou bez rozsireni produkcniho zasahu do bezici session.
* DSL zustava explicitni a validovany vstup. Tento krok tedy stale nepredava LLM primou pravomoc generovat gameplay skript mimo kontrolovany scripted event/effect path.

Testy a stav overeni:

* Repozitar obsahuje regression coverage pro offline spine pruchod i navazujici companion test.
* Verejny GitHub Actions beh `Windows Tests` pro commit `Add offline spine harness` skoncil uspesne dne 28.5.2026 v 00:54:29 mistniho casu.
* Nejnovnejsi commit `SNC status snapshot: add generated_at_local timestamp` je v tuto chvili lokalne o jeden commit pred `origin/master`, takze jeho verejny CI vysledek zatim neni k dispozici.

Blokery a rizika:

* Hlavni produkcni blocker se nemeni: stale chybi uzavreny checksum-safe packaging a distribucni workflow pro generated overlay artefakty mezi multiplayer ucastniky.
* Offline spine uz spojuje klicove vrstvy, ale stale neuzavira finalni manifest/export contract, campaign marker handshake ani empire identity resolver.
* Presnejsi status snapshot zlepsuje auditovatelnost, ale sam o sobe jeste neresi finalni doruceni artefaktu pres checksum-sensitive integration boundary.

Doporuceny dalsi krok:

* Pushnout nejnovnejsi snapshot commit spolu s touto denikovou aktualizaci a potvrdit navazujici verejny CI signal.
* Potom navazat formalnim generated overlay export contractem a end-to-end testem pro checksum-safe dalsi session distribuci.

## 2026-05-26

Projekt dnes posunul hlavne companion status vrstvu a navazujici testovaci kryti kolem archive-backed workflow. Neslo o novou herni integraci uvnitr bezici session, ale o zpresneni stavu na bezpecne integracni boundary mezi archivem, generated overlay artefakty a offline companion aplikaci.

Co pribylo v repozitari:

* Companion JSON serializace byla zpevnenejsi pro ridici znaky a dalsi problematicke vstupy, aby status snapshot zustal korektni i pri nestandardnich textovych hodnotach.
* Companion status logika nyni rozlisuje prazdny archive root jako stav `starting` misto predcasneho `ready`; pripravenost se potvrzuje az pri nalezeni session adresare s `manifest.json`.
* Navazujici testy a CLI harness byly upraveny tak, aby novy lifecycle vyklad konzistentne overovaly v jednotkovych i pipeline scenarich.
* Vedle toho pribyl maly `cmd` wrapper pro automation run logging; jde o podpurnou developerskou automatizaci, ne o produkcni runtime schopnost.

Co to znamena pro architekturu:

* Runtime interoperability research se dale opira o host-authoritative model, kde companion vrstva pouze popisuje stav a pripravuje bezpecny prechod mezi archivaci, analyzou a dalsi session.
* Rozliseni `starting` vs. `ready` zmensuje riziko, ze se prazdny archiv bude tvarit jako hotovy vstup pro dalsi zpracovani, i kdyz jeste neexistuje potvrzena session historie.
* Tvrdsi serializace status snapshotu posiluje spolehlivost verejnych i internich stavovych vystupu na integration boundary a snizuje riziko poskozeneho JSON pri skriptovanem event/effect path doprovodu.

Testy a stav overeni:

* Posledni commity doplnily a upravily testy pro `StrategicNexusCompanion` i `tools/run_v0_pipeline_tests.ps1`, takze zmena ma primou regression coverage v repozitari.
* V tomto behu nebyl znovu spusten plny lokalni `tools/run_all_tests.ps1`, protoze denikova aktualizace navazuje na jiz ocommitovane zmeny a nebyla doprovazena novou produkcni implementaci.
* Verejny GitHub CI stav se v tomto behu nepodarilo nezavisle potvrdit.

Blokery a rizika:

* Hlavni produkcni blocker se nemeni: stale chybi uzavreny checksum-safe packaging a distribucni workflow pro generated overlay artefakty mezi multiplayer ucastniky.
* Companion status vrstva je presnejsi, ale stale neuzavira finalni manifest/export contract, campaign marker handshake ani empire identity resolver.
* Soucasny posun zlepsuje lifecycle citelnost a robustnost stavovych dat, ale sam o sobe jeste neuzavira end-to-end cestu od archive session k checksum-sensitive dalsi session distribuci.

Doporuceny dalsi krok:

* Navazat na novou companion lifecycle interpretaci prvnim explicitnim bootstrap flow pro prazdnou kampanovou historii, aby stav `starting` mel jasne navazujici kroky.
* Potom propojit tento bootstrap s generated overlay manifest contractem a end-to-end testem mezi prvni archive session, offline analyzou a dalsi session exportem.

## 2026-05-25

Projekt dnes spojil tri dulezite proudy prace: zpresneni release companion lifecycle pro offline companion workflow, dalsi hardening validace na archive a generated overlay hranici a navazujici implementacni uklid kolem v0 pipeline a pomocnych vyvojovych nastroju.

Co pribylo v repozitari:

* Dokumentace vymezila release companion lifecycle, fault-tolerance pravidla a navazani offline campaign analysis architektury na dalsi session generated overlay pripravu.
* Generated overlay vrstva dostala rozsahlejsi verifier coverage a dalsi kontraktni testy pro parser a validaci, aby scripted event/effect path zustal omezitelny, auditovatelny a odolny vuci neplatnym vstupum.
* Autosave archive verifier byl rozsireny tak, aby odmitl unsafe `archived_path` hodnoty a lepe branil integracni boundary mezi archivovanymi artefakty a dalsim zpracovanim.
* V navazujicich commitech probehl implementacni uklid ve v0 pipeline (`Application.cpp`, `SeasonDeltaLedgerBuilder.cpp`, `generated_overlay/DslParser.cpp`) a doplneni testu pro tyto cesty.
* Vedle toho pokracovala podpurna vyvojova automatizace kolem freework cadence, task boardu, automation run logu a kontrol hlavicek zdrojovych souboru; to zlepsuje disciplinu prace, ale samo o sobe jeste nerozsiruje produkcni herni integraci.

Co to znamena pro architekturu:

* Runtime interoperability research se dale konsoliduje kolem host-authoritative modelu, kde aktivni runtime zustava oddelen od offline analyzy, archivace a pripravy dalsi session.
* Nove lifecycle a verifier prace zmensuji integracni mezeru mezi autosave archivem, analyzou, generated overlay kompilaci a pozdejsim exportem do checksum-sensitive distribuce.
* Dnesni zmeny posouvaji projekt spis ve smeru tvrdsiho ohraniceni vstupu a spolehlivejsiho release workflow nez ve smeru novych runtime schopnosti uvnitr bezici session.

Testy a stav overeni:

* Lokalni `tools/run_all_tests.ps1` dnes uspesne probehl po spusteni pres PowerShell `ExecutionPolicy Bypass`.
* Prosly safety audit, bridge-core smoke testy, autosave archive testy, generated overlay contract a verifier testy i kompletni v0 strategic pipeline test suite.
* Verejny GitHub CI stav se v tomto behu nepodarilo potvrdit, protoze automatizace nemela prime online overeni Actions vysledku.

Blokery a rizika:

* Hlavni produkcni blocker se nemeni: stale chybi uzavreny checksum-safe packaging a distribucni workflow pro generated overlay artefakty mezi multiplayer ucastniky.
* Release companion lifecycle je uz lepe popsany, ale stale potrebuje finalni manifest/export contract, campaign marker handshake a empire identity resolver, aby byl prechod mezi archivem a dalsi session plne uzavreny.
* Cast dnesniho tempa sla do tooling a dokumentacni discipliny; to snizuje provozni riziko, ale muze kratkodobe zdanlive zpomalovat viditelny posun na uzivatelske vrstve.

Doporuceny dalsi krok:

* Navazat na novy lifecycle a verifier coverage formalnim manifestem generated overlay exportu vcetne checksum-sensitive file classification.
* Potom dopsat end-to-end test, ktery propoji autosave archive vstup, runtime interoperability research artefakty a pripraveny next-session overlay bez poruseni bezpecne integracni boundary.

## 2026-05-24

Projekt dnes posunul dve navazujici oblasti: implementacni v0 harness pro archive-backed workflow a dokumentacni zpresneni pracovni orchestrace, rozpoctovych pravidel a role background freework vrstvy.

Co pribylo v repozitari:

* Do aplikacni vrstvy pribyl archive-backed v0 pipeline harness vcetne noveho test runneru, ktery propojuje archiv autosave artefaktu s dalsim zpracovanim mezi sezenimi.
* Dokumentace byla rozsirena o freework budget rules, vymezeni implementation worker role a navazani roadmapy na background freework model.
* Drobne UI upravy ve vyvojovem task boardu zlepsily citelnost detailu a stav kopirovaci akce, ale nejde o zmenu produkcni herni integrace.

Co to znamena pro architekturu:

* Runtime interoperability research se dale stabilizuje kolem offline, host-authoritative modelu, kde se analyza, archivace a priprava dalsi session odehrava mimo aktivni runtime.
* Novy archive-backed harness zmensuje integracni mezeru mezi autosave analyzou, v0 pipeline zpracovanim a budoucim generated overlay balenim.
* Dokumentacni prace zaroven zpresnila, jak ma byt dalsi implementace delegovana a limitovana, aby scripted event/effect path zustal auditovatelny a bezpecne ohraniceny.

Testy a stav overeni:

* Lokalni `tools/run_all_tests.ps1` dnes uspesne probehl po spusteni s PowerShell `ExecutionPolicy Bypass`.
* Prosly safety audit, bridge-core smoke testy, autosave archive testy, generated overlay contract testy i kompletni v0 strategic pipeline test suite.
* Verejny GitHub CI stav se v tomto behu nepodarilo potvrdit, protoze automatizace bezela bez primeho online overeni Actions vysledku.

Blokery a rizika:

* Hlavni produkcni blocker se nemeni: chybi uzavreny checksum-safe packaging a distribucni workflow pro generated overlay artefakty mezi multiplayer ucastniky.
* Archive-backed harness uz overuje smer pipeline, ale stale neuzavira campaign marker handshake, empire identity resolver a finalni manifest/export contract.
* Cast dnesnich commitu je dokumentacni a orchestrace-focused; produkcni runtime interoperabilita tedy postupuje spise konsolidaci hranic a workflow nez novymi hernimi schopnostmi uvnitr aktivni session.

Doporuceny dalsi krok:

* Navazat na archive-backed harness formalnim manifestem a checksum-sensitive klasifikaci exportu.
* Potom doplnit end-to-end test, ktery propoji autosave archive vstup, overlay compiler a pripravene artefakty pro dalsi session bez poruseni bezpecne integracni boundary.

## 2026-05-23

Projekt dnes potvrdil produkcni smer k offline companion workflow a soucasne pridal prvni implementacni kostru pro generated overlay pipeline.

Co pribylo v repozitari:

* Dokumentace byla doplnena o offline companion architekturu a vyjasnila integracni boundary mezi analyzou mimo hru a host-authoritative modelem uvnitr kampane.
* Do `src/generated_overlay/` pribyl prvni DSL parser, validator a compiler skeleton pro bounded generated overlay vstup.
* Novy compile harness umoznuje prelozit validni overlay definici do staged artifactu pro dalsi kontrolu a baleni.

Co to znamena pro architekturu:

* Runtime interoperability research se nyni soustredi na checksum-safe next-session overlay workflow misto pokusu o zmenu aktivni session za behu.
* Strategic Nexus zustava nadstavbou nad beznym hernim vykonem a scripted event/effect path je stale deterministicky, kontrolovany a vhodny pro audit.
* Drivejsi bridge a daemon prace zustava relevantni jako vyvojovy harness a validacni vrstva, ale ne jako produkcni runtime slib.

Testy a stav overeni:

* `tools/run_all_tests.ps1` v tomto behu uspesne proslo.
* Prosly bridge-core smoke testy, v0 strategic pipeline testy i novy generated overlay contract test.
* GitHub CI stav se v tomto behu nepodarilo spolehlive potvrdit, protoze lokalni GitHub CLI konfigurace nebyla pristupna.

Blokery a rizika:

* Zakladni v0 generated overlay layout contract uz existuje; stale chybi checksum-sensitive file classification a export/package workflow pro byte-identical multiplayer distribuci.
* Otevreny je campaign marker handshake, empire identity resolver a navazani dlouhodobe pameti kampane na autosave parser bez poruseni bezpecne integracni boundary.
* Soucasny compiler je zamerne jen kontraktni kostra; finalni mapovani na gameplay obsah a balancing jeste neni uzavrene.

Doporuceny dalsi krok:

* Navazat na existujici v0 generated overlay layout contract presnym manifestem, balicimi pravidly a checksum-sensitive klasifikaci.
* Potom rozsirit validator a prvni end-to-end test exportu mezi offline analyzou a dalsi herni session.

## 2026-05-22

Projekt byl preorientovan na bezpecnejsi vyvojovy rozsah.

Co je hotovo:

* SP i MP PoC jsou potvrzene na urovni scripted state.
* Bridge-core umi validovat bounded payload a generovat allowlisted effect batch artifact.
* V0 pipeline ma prvni deterministickou podobu: ministry input `X`, ministry proposal `Y`, clerk-reduced brief `U`, cabinet review, final payload.
* Repository scope byl zpresnen: supported mod scripting, lokalni JSON daemon, validace, fallbacky a testy.

Proc je to dulezite:

* Projekt zustava zamereny na zlepseni herniho zazitku, ale bez nizkourovnoveho zasahu do hry.
* Codex muze dal pomahat s architekturou, validaci, LLM pipeline, mod skripty a testy.
* Verejne nebo sdilene materialy nemaji obsahovat citlive poznamky o proprietarnich runtime internalech.

Riziko:

* Bez nizkourovnove integrace muze byt finalni automaticke doruceni rozhodnuti do hry omezenejsi.
* To je prijatelny tradeoff pro stabilitu, bezpecnost a udrzitelnost projektu.

Dalsi nejdulezitejsi krok:

* Dodelat checksum-safe scripted mod workflow.
* Napojit daemon pipeline na bezpecny mod-side receiver nebo explicitni vyvojovy harness.
* Zachovat striktni schema validation a fallback chovani.

Strucny zaver:

Strategic Nexus pokracuje jako bezpecny modding a strategic-assistant projekt, ne jako runtime modification projekt.
